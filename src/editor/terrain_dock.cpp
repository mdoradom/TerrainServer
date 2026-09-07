#include "terrain_dock.h"

#include "core/terrain_biome_layer.h"
#include "core/terrain_configuration.h"
#include "nodes/terrain3d.h"
#include "whittaker_chart.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_selection.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_separator.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/classes/timer.hpp>

#include <algorithm>
#include <limits>

using namespace godot;

namespace ts {

namespace {

constexpr double STATS_REFRESH_INTERVAL = 0.5;
constexpr int HEIGHT_PREVIEW_RESOLUTION = 40;
constexpr float HEIGHT_PREVIEW_FALLBACK_WINDOW = 64.0f;
constexpr float HEIGHT_PREVIEW_WINDOW_FRACTION = 0.25f;

Terrain3D *terrain_from_selected_node(Node *p_node) {
	for (Node *node = p_node; node != nullptr; node = node->get_parent()) {
		if (Terrain3D *terrain = Object::cast_to<Terrain3D>(node)) {
			return terrain;
		}
	}
	return nullptr;
}

} //namespace

void TerrainDock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_selection_changed"), &TerrainDock::_on_selection_changed);
	ClassDB::bind_method(D_METHOD("_on_stats_timer_timeout"), &TerrainDock::_on_stats_timer_timeout);
	ClassDB::bind_method(D_METHOD("_on_rebuild_pressed"), &TerrainDock::_on_rebuild_pressed);
	ClassDB::bind_method(D_METHOD("_on_reload_shader_pressed"), &TerrainDock::_on_reload_shader_pressed);
}

TerrainDock::TerrainDock() {
	_build_ui();
}

Label *TerrainDock::_add_section_header(Control *p_parent, const String &p_text) {
	Label *header = memnew(Label);
	header->set_text(p_text);
	p_parent->add_child(header);
	_section_headers.push_back(header);
	return header;
}

void TerrainDock::_build_ui() {
	add_theme_constant_override("separation", 6);

	_empty_label = memnew(Label);
	_empty_label->set_text("No Terrain3D in the scene or editor registry.");
	_empty_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD);
	add_child(_empty_label);

	_content_root = memnew(VBoxContainer);
	_content_root->add_theme_constant_override("separation", 6);
	add_child(_content_root);

	HBoxContainer *stats_row = memnew(HBoxContainer);
	stats_row->add_theme_constant_override("separation", 24);
	_content_root->add_child(stats_row);

	VBoxContainer *clipmap_column = memnew(VBoxContainer);
	stats_row->add_child(clipmap_column);
	_add_section_header(clipmap_column, "Clipmap");
	_clipmap_label = memnew(Label);
	clipmap_column->add_child(_clipmap_label);

	VBoxContainer *collision_column = memnew(VBoxContainer);
	stats_row->add_child(collision_column);
	_add_section_header(collision_column, "Collision");
	_collision_label = memnew(Label);
	collision_column->add_child(_collision_label);

	VBoxContainer *configuration_column = memnew(VBoxContainer);
	stats_row->add_child(configuration_column);
	_add_section_header(configuration_column, "Configuration");
	_configuration_label = memnew(Label);
	configuration_column->add_child(_configuration_label);

	VBoxContainer *height_column = memnew(VBoxContainer);
	stats_row->add_child(height_column);
	_add_section_header(height_column, "Local height sample");
	_height_preview = memnew(TextureRect);
	_height_preview->set_custom_minimum_size(Vector2(128.0f, 128.0f));
	_height_preview->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	_height_preview->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
	height_column->add_child(_height_preview);

	VBoxContainer *probe_column = memnew(VBoxContainer);
	stats_row->add_child(probe_column);
	_add_section_header(probe_column, "Live probe");
	_probe_label = memnew(Label);
	probe_column->add_child(_probe_label);

	_content_root->add_child(memnew(HSeparator));

	_add_section_header(_content_root, "Biome climate");
	_chart = memnew(WhittakerChart);
	_chart->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	_content_root->add_child(_chart);

	_content_root->add_child(memnew(HSeparator));

	HBoxContainer *actions_row = memnew(HBoxContainer);
	actions_row->add_theme_constant_override("separation", 6);
	_content_root->add_child(actions_row);

	_rebuild_button = memnew(Button);
	_rebuild_button->set_text("Rebuild");
	actions_row->add_child(_rebuild_button);

	_reload_shader_button = memnew(Button);
	_reload_shader_button->set_text("Reload shader");
	actions_row->add_child(_reload_shader_button);

	_stats_timer = memnew(Timer);
	_stats_timer->set_wait_time(STATS_REFRESH_INTERVAL);
	add_child(_stats_timer);
}

void TerrainDock::_pull_theme() const {
	const EditorInterface *editor_interface = EditorInterface::get_singleton();
	if (editor_interface == nullptr) {
		return;
	}

	const float editor_scale = editor_interface->get_editor_scale();
	const Ref<Theme> theme = editor_interface->get_editor_theme();

	Ref<Font> bold_font;
	int font_size = 13;
	Color accent_color(0.6f, 0.8f, 1.0f);
	if (theme.is_valid()) {
		bold_font = theme->get_font("bold", "EditorFonts");
		font_size = theme->get_font_size("bold_size", "EditorFonts");
		accent_color = theme->get_color("accent_color", "Editor");
	}

	for (Label *header : _section_headers) {
		if (bold_font.is_valid()) {
			header->add_theme_font_override("font", bold_font);
		}
		header->add_theme_font_size_override("font_size", font_size);
		header->add_theme_color_override("font_color", accent_color);
	}

	_height_preview->set_custom_minimum_size(Vector2(128.0f, 128.0f) * editor_scale);
}

void TerrainDock::_ready() {
	_pull_theme();

	const EditorInterface *editor_interface = EditorInterface::get_singleton();
	EditorSelection *selection = editor_interface != nullptr ? editor_interface->get_selection() : nullptr;
	if (selection != nullptr) {
		selection->connect("selection_changed", Callable(this, "_on_selection_changed"));
	}

	_stats_timer->connect("timeout", Callable(this, "_on_stats_timer_timeout"));
	_stats_timer->start();

	_rebuild_button->connect("pressed", Callable(this, "_on_rebuild_pressed"));
	_reload_shader_button->connect("pressed", Callable(this, "_on_reload_shader_pressed"));

	_set_current_terrain(_resolve_current_terrain());
	_refresh_all();
}

Terrain3D *TerrainDock::_resolve_current_terrain() const {
	const EditorInterface *editor_interface = EditorInterface::get_singleton();
	EditorSelection *selection = editor_interface != nullptr ? editor_interface->get_selection() : nullptr;
	if (selection != nullptr) {
		const TypedArray<Node> selected = selection->get_selected_nodes();
		for (const auto &i : selected) {
			if (Terrain3D *terrain = terrain_from_selected_node(Object::cast_to<Node>(i))) {
				return terrain;
			}
		}
	}

	const std::vector<Terrain3D *> &registry = Terrain3D::get_editor_instances();
	return registry.empty() ? nullptr : registry.front();
}

void TerrainDock::_set_current_terrain(Terrain3D *p_terrain) {
	_current_terrain = p_terrain;
	_update_empty_state();
}

void TerrainDock::_update_empty_state() const {
	const bool has_terrain = _current_terrain != nullptr;
	_empty_label->set_visible(!has_terrain);
	_content_root->set_visible(has_terrain);
}

void TerrainDock::_refresh_all() {
	_refresh_stats();
	_refresh_probe();
	_refresh_height_preview();
	_chart->set_configuration(_current_terrain != nullptr ? _current_terrain->get_configuration() : Ref<TerrainConfiguration>());
}

void TerrainDock::_refresh_stats() const {
	if (_current_terrain == nullptr) {
		_clipmap_label->set_text("");
		_collision_label->set_text("");
		_configuration_label->set_text("");
		return;
	}

	String levels;
	const int level_count = _current_terrain->get_clipmap_level_count();
	for (int i = 0; i < level_count; i++) {
		if (i > 0) {
			levels += ", ";
		}
		levels += String::num(_current_terrain->get_clipmap_level_extent(i), 0);
	}
	_clipmap_label->set_text(vformat("Levels (%d): %s\nCenter (L0): %s\nMorph band: %.2f-0.50 of each ring", level_count, levels, _current_terrain->get_clipmap_center(), _current_terrain->get_morph_band_start()));

	_collision_label->set_text(vformat("Range: %.0f  Resolution: %d\nCenter: %s\nRebuild pending: %s",
			_current_terrain->get_collision_range(), _current_terrain->get_collision_resolution(),
			_current_terrain->get_collision_center(), _current_terrain->is_collision_rebuild_pending() ? "yes" : "no"));

	const Ref<TerrainConfiguration> config = _current_terrain->get_configuration();
	if (config.is_valid()) {
		_configuration_label->set_text(vformat("Mesh resolution: %d\nClipmap levels: %d\nBiome layers: %d",
				config->get_mesh_resolution(), config->get_clipmap_levels(), config->get_biome_layers().size()));
	} else {
		_configuration_label->set_text("(no configuration assigned)");
	}
}

void TerrainDock::_refresh_probe() const {
	if (_current_terrain == nullptr) {
		_probe_label->set_text("");
		_chart->clear_probe();
		return;
	}

	const Vector3 focus = _current_terrain->get_focus_position();
	const Vector2 world_xz(focus.x, focus.z);

	const float height = _current_terrain->get_height_at(world_xz);
	const float temperature = _current_terrain->get_temperature_at(world_xz);
	const float moisture = _current_terrain->get_moisture_at(world_xz);
	const Ref<TerrainBiomeLayer> biome = _current_terrain->get_biome_at(world_xz);
	const String biome_name = biome.is_valid() ? biome->get_biome_name() : "(none)";

	_probe_label->set_text(vformat("Height: %.2f\nTemperature: %.1f C\nMoisture: %.2f\nBiome: %s", height, temperature, moisture, biome_name));
	_chart->set_probe(temperature, moisture);
}

void TerrainDock::_refresh_height_preview() {
	if (_current_terrain == nullptr) {
		_height_preview->set_texture(Ref<Texture2D>());
		return;
	}

	const Vector3 focus = _current_terrain->get_focus_position();
	const Vector2 center(focus.x, focus.z);

	const int level_count = _current_terrain->get_clipmap_level_count();
	const float extent0 = level_count > 0 ? _current_terrain->get_clipmap_level_extent(0) : 0.0f;
	const float window = extent0 > 0.0f ? extent0 * HEIGHT_PREVIEW_WINDOW_FRACTION : HEIGHT_PREVIEW_FALLBACK_WINDOW;
	const float half = window * 0.5f;

	float heights[HEIGHT_PREVIEW_RESOLUTION * HEIGHT_PREVIEW_RESOLUTION];
	float min_h = std::numeric_limits<float>::max();
	float max_h = std::numeric_limits<float>::lowest();

	for (int y = 0; y < HEIGHT_PREVIEW_RESOLUTION; y++) {
		const float wz = center.y - half + window * (static_cast<float>(y) + 0.5f) / HEIGHT_PREVIEW_RESOLUTION;
		for (int x = 0; x < HEIGHT_PREVIEW_RESOLUTION; x++) {
			const float wx = center.x - half + window * (static_cast<float>(x) + 0.5f) / HEIGHT_PREVIEW_RESOLUTION;
			const float h = _current_terrain->get_height_at(Vector2(wx, wz));
			heights[y * HEIGHT_PREVIEW_RESOLUTION + x] = h;
			min_h = std::min(min_h, h);
			max_h = std::max(max_h, h);
		}
	}

	const float range = std::max(max_h - min_h, 0.001f);
	const Ref<Image> image = Image::create(HEIGHT_PREVIEW_RESOLUTION, HEIGHT_PREVIEW_RESOLUTION, false, Image::FORMAT_RGB8);
	for (int y = 0; y < HEIGHT_PREVIEW_RESOLUTION; y++) {
		for (int x = 0; x < HEIGHT_PREVIEW_RESOLUTION; x++) {
			const float t = (heights[y * HEIGHT_PREVIEW_RESOLUTION + x] - min_h) / range;
			image->set_pixel(x, y, Color(t, t, t));
		}
	}

	constexpr int center_px = HEIGHT_PREVIEW_RESOLUTION / 2;
	image->set_pixel(center_px, center_px, Color(1.0f, 0.85f, 0.2f));

	if (_height_preview_texture.is_valid()) {
		_height_preview_texture->set_image(image);
	} else {
		_height_preview_texture = ImageTexture::create_from_image(image);
	}
	_height_preview->set_texture(_height_preview_texture);
}

void TerrainDock::_on_selection_changed() {
	_set_current_terrain(_resolve_current_terrain());
	_refresh_all();
}

void TerrainDock::_on_stats_timer_timeout() {
	_set_current_terrain(_resolve_current_terrain());
	_refresh_all();
}

void TerrainDock::_on_rebuild_pressed() const {
	if (_current_terrain != nullptr) {
		_current_terrain->rebuild();
	}
}

void TerrainDock::_on_reload_shader_pressed() const {
	if (_current_terrain != nullptr) {
		_current_terrain->reload_shader();
	}
}

} //namespace ts
