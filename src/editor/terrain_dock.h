#pragma once

#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/variant/string.hpp>

#include <vector>

namespace godot {
class Label;
class Button;
class TextureRect;
class Timer;
class Control;
} //namespace godot

namespace ts {

class Terrain3D;
class WhittakerChart;

// Persistent bottom-panel dock: tracks the selection (falling back to the first registered
// Terrain3D so it's useful without clicking), and shows stats, action buttons, the Whittaker
// chart, a local height sample and a live climate probe for whichever terrain that resolves to.
class TerrainDock : public godot::VBoxContainer {
	GDCLASS(TerrainDock, godot::VBoxContainer);

private:
	Terrain3D *_current_terrain = nullptr;

	godot::Label *_empty_label = nullptr;
	godot::VBoxContainer *_content_root = nullptr;

	godot::Label *_clipmap_label = nullptr;
	godot::Label *_collision_label = nullptr;
	godot::Label *_configuration_label = nullptr;
	godot::Timer *_stats_timer = nullptr;

	godot::Button *_rebuild_button = nullptr;
	godot::Button *_reload_shader_button = nullptr;

	WhittakerChart *_chart = nullptr;
	godot::TextureRect *_height_preview = nullptr;
	godot::Ref<godot::ImageTexture> _height_preview_texture;

	godot::Label *_probe_label = nullptr;

	std::vector<godot::Label *> _section_headers;

	void _build_ui();
	godot::Label *_add_section_header(Control *p_parent, const godot::String &p_text);
	void _pull_theme() const;

	Terrain3D *_resolve_current_terrain() const;
	void _set_current_terrain(Terrain3D *p_terrain);
	void _update_empty_state() const;

	void _refresh_all();
	void _refresh_stats() const;
	void _refresh_probe() const;
	void _refresh_height_preview();

protected:
	static void _bind_methods();

	void _on_selection_changed();
	void _on_stats_timer_timeout();
	void _on_rebuild_pressed() const;
	void _on_reload_shader_pressed() const;

public:
	TerrainDock();

	void _ready() override;
};

} //namespace ts
