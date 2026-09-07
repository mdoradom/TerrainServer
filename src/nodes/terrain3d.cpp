#include "terrain3d.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>

using namespace godot;

namespace ts {

std::vector<Terrain3D *> Terrain3D::_editor_instances;

void Terrain3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_configuration", "config"), &Terrain3D::set_configuration);
	ClassDB::bind_method(D_METHOD("get_configuration"), &Terrain3D::get_configuration);
	ClassDB::bind_method(D_METHOD("get_height_at", "world_xz"), &Terrain3D::get_height_at);
	ClassDB::bind_method(D_METHOD("get_biome_at", "world_xz"), &Terrain3D::get_biome_at);
	ClassDB::bind_method(D_METHOD("get_temperature_at", "world_xz"), &Terrain3D::get_temperature_at);
	ClassDB::bind_method(D_METHOD("get_moisture_at", "world_xz"), &Terrain3D::get_moisture_at);
	ClassDB::bind_method(D_METHOD("get_focus_position"), &Terrain3D::get_focus_position);
	ClassDB::bind_method(D_METHOD("get_clipmap_level_count"), &Terrain3D::get_clipmap_level_count);
	ClassDB::bind_method(D_METHOD("get_clipmap_level_extent", "level"), &Terrain3D::get_clipmap_level_extent);
	ClassDB::bind_method(D_METHOD("get_clipmap_center"), &Terrain3D::get_clipmap_center);
	ClassDB::bind_method(D_METHOD("get_clipmap_level_origin", "level"), &Terrain3D::get_clipmap_level_origin);
	ClassDB::bind_method(D_METHOD("get_morph_band_start"), &Terrain3D::get_morph_band_start);
	ClassDB::bind_method(D_METHOD("get_collision_range"), &Terrain3D::get_collision_range);
	ClassDB::bind_method(D_METHOD("get_collision_resolution"), &Terrain3D::get_collision_resolution);
	ClassDB::bind_method(D_METHOD("get_collision_center"), &Terrain3D::get_collision_center);
	ClassDB::bind_method(D_METHOD("get_collision_built_range"), &Terrain3D::get_collision_built_range);
	ClassDB::bind_method(D_METHOD("get_collision_built_resolution"), &Terrain3D::get_collision_built_resolution);
	ClassDB::bind_method(D_METHOD("get_collision_height_range"), &Terrain3D::get_collision_height_range);
	ClassDB::bind_method(D_METHOD("is_collision_built"), &Terrain3D::is_collision_built);
	ClassDB::bind_method(D_METHOD("is_collision_rebuild_pending"), &Terrain3D::is_collision_rebuild_pending);
	ClassDB::bind_method(D_METHOD("set_focus_path", "path"), &Terrain3D::set_focus_path);
	ClassDB::bind_method(D_METHOD("get_focus_path"), &Terrain3D::get_focus_path);
	ClassDB::bind_method(D_METHOD("set_editor_focus_override", "world_position"), &Terrain3D::set_editor_focus_override);
	ClassDB::bind_method(D_METHOD("clear_editor_focus_override"), &Terrain3D::clear_editor_focus_override);
	ClassDB::bind_method(D_METHOD("set_editor_preview", "enabled"), &Terrain3D::set_editor_preview);
	ClassDB::bind_method(D_METHOD("get_editor_preview"), &Terrain3D::get_editor_preview);
	ClassDB::bind_method(D_METHOD("set_editor_preview_physics", "enabled"), &Terrain3D::set_editor_preview_physics);
	ClassDB::bind_method(D_METHOD("get_editor_preview_physics"), &Terrain3D::get_editor_preview_physics);
	ClassDB::bind_method(D_METHOD("reload_shader"), &Terrain3D::reload_shader);
	ClassDB::bind_method(D_METHOD("rebuild"), &Terrain3D::rebuild);
	ClassDB::bind_method(D_METHOD("_on_config_changed"), &Terrain3D::_on_config_changed);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "configuration", PROPERTY_HINT_RESOURCE_TYPE, "TerrainConfiguration"), "set_configuration", "get_configuration");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "focus_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Node3D"), "set_focus_path", "get_focus_path");

	ADD_GROUP("Editor", "editor_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "editor_preview"), "set_editor_preview", "get_editor_preview");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "editor_preview_physics"), "set_editor_preview_physics", "get_editor_preview_physics");
}

Terrain3D::Terrain3D() {
	set_process(true);
	_renderer.instantiate();
	_generator.instantiate();
	_physics.instantiate();
}

Terrain3D::~Terrain3D() {
	_editor_instances.erase(std::remove(_editor_instances.begin(), _editor_instances.end(), this), _editor_instances.end());

	if (_config.is_valid()) {
		_config->disconnect("changed", Callable(this, "_on_config_changed"));
	}

	if (_physics.is_valid()) {
		_physics->cleanup();
	}

	if (_renderer.is_valid()) {
		_renderer->cleanup();
	}
}

void Terrain3D::_ready() {
	_renderer->initialize(this);
	_physics->initialize(this);
}

void Terrain3D::_process(double delta) {
	if (!is_inside_tree()) {
		return;
	}

	const bool editing = Engine::get_singleton()->is_editor_hint();
	if (editing && !_editor_preview) {
		return;
	}

	Vector3 focus_pos;
	bool has_focus = false;

	if (!_focus_path.is_empty()) {
		if (const Node3D *focus_node = Object::cast_to<Node3D>(get_node_or_null(_focus_path))) {
			focus_pos = focus_node->get_global_position();
			has_focus = true;
		}
	}

	if (!has_focus && _has_editor_focus_override) {
		focus_pos = _editor_focus_override;
		has_focus = true;
	}

	if (!has_focus) {
		const Viewport *viewport = get_viewport();
		if (const Camera3D *camera = viewport != nullptr ? viewport->get_camera_3d() : nullptr) {
			focus_pos = camera->get_global_position();
			has_focus = true;
		}
	}

	if (!has_focus) {
		return;
	}

	_focus_position = focus_pos;

	if (_renderer.is_valid()) {
		_renderer->update_focus_position(focus_pos);
	}

	if (_physics.is_valid() && (!editing || _editor_preview_physics)) {
		_physics->update_focus_position(focus_pos);
	}
}

void Terrain3D::_notification(const int p_what) {
	switch (p_what) {
		case NOTIFICATION_EXIT_TREE: {
			_editor_instances.erase(std::remove(_editor_instances.begin(), _editor_instances.end(), this), _editor_instances.end());

			if (_physics.is_valid()) {
				_physics->cleanup();
			}
			if (_renderer.is_valid()) {
				_renderer->cleanup();
			}
		} break;

		case NOTIFICATION_ENTER_TREE: {
			if (Engine::get_singleton()->is_editor_hint() &&
					std::find(_editor_instances.begin(), _editor_instances.end(), this) == _editor_instances.end()) {
				_editor_instances.push_back(this);
			}

			if (_physics.is_valid()) {
				_physics->initialize(this);
			}
			if (_renderer.is_valid()) {
				_renderer->initialize(this);
				_on_config_changed();
			}
		} break;
	}
}

void Terrain3D::_on_config_changed() {
	if (!_config.is_valid() || !_renderer.is_valid() || !_generator.is_valid() || !_physics.is_valid()) {
		return;
	}

	_generator->setup(_config);
	_physics->set_configuration(_config);
	_renderer->set_configuration(_config);

	if (Engine::get_singleton()->is_editor_hint() && !_editor_preview) {
		return;
	}

	_renderer->rebuild_mesh(_config->get_terrain_size(), _config->get_mesh_resolution());
}

void Terrain3D::set_configuration(const Ref<TerrainConfiguration> &p_config) {
	if (_config == p_config) {
		return;
	}

	if (_config.is_valid()) {
		_config->disconnect("changed", Callable(this, "_on_config_changed"));
	}

	_config = p_config;

	if (_config.is_valid()) {
		_config->connect("changed", Callable(this, "_on_config_changed"));
	}

	_on_config_changed();
}

Ref<TerrainConfiguration> Terrain3D::get_configuration() const {
	return _config;
}

float Terrain3D::get_height_at(const Vector2 p_world_xz) const {
	if (!_physics.is_valid()) {
		return 0.0f;
	}

	return _physics->get_height_at(p_world_xz);
}

Ref<TerrainBiomeLayer> Terrain3D::get_biome_at(const Vector2 p_world_xz) const {
	if (!_physics.is_valid()) {
		return nullptr;
	}

	return _physics->get_biome_at(p_world_xz);
}

float Terrain3D::get_temperature_at(const Vector2 p_world_xz) const {
	if (!_physics.is_valid()) {
		return 0.0f;
	}

	return _physics->temperature_at(p_world_xz);
}

float Terrain3D::get_moisture_at(const Vector2 p_world_xz) const {
	if (!_physics.is_valid()) {
		return 0.0f;
	}

	return _physics->moisture_at(p_world_xz);
}

Vector3 Terrain3D::get_focus_position() const {
	return _focus_position;
}

int Terrain3D::get_clipmap_level_count() const {
	if (!_renderer.is_valid()) {
		return 0;
	}

	return _renderer->get_clipmap_level_count();
}

float Terrain3D::get_clipmap_level_extent(const int p_level) const {
	if (!_renderer.is_valid()) {
		return 0.0f;
	}

	return _renderer->get_clipmap_level_scale(p_level);
}

Vector2 Terrain3D::get_clipmap_center() const {
	if (!_renderer.is_valid()) {
		return {};
	}

	return _renderer->get_snapped_focus_xz();
}

Vector2 Terrain3D::get_clipmap_level_origin(const int p_level) const {
	if (!_renderer.is_valid()) {
		return {};
	}

	return _renderer->get_clipmap_level_origin(p_level);
}

float Terrain3D::get_morph_band_start() const {
	return TerrainRenderer::get_morph_band_start();
}

float Terrain3D::get_collision_range() const {
	if (!_physics.is_valid()) {
		return 0.0f;
	}

	return _physics->get_range();
}

int Terrain3D::get_collision_resolution() const {
	if (!_physics.is_valid()) {
		return 0;
	}

	return _physics->get_grid_resolution();
}

Vector2 Terrain3D::get_collision_center() const {
	if (!_physics.is_valid()) {
		return {};
	}

	return _physics->get_last_built_origin();
}

float Terrain3D::get_collision_built_range() const {
	if (!_physics.is_valid()) {
		return 0.0f;
	}

	return _physics->get_built_range();
}

int Terrain3D::get_collision_built_resolution() const {
	if (!_physics.is_valid()) {
		return 0;
	}

	return _physics->get_built_grid_resolution();
}

Vector2 Terrain3D::get_collision_height_range() const {
	if (!_physics.is_valid()) {
		return {};
	}

	return _physics->get_built_height_range();
}

bool Terrain3D::is_collision_built() const {
	if (!_physics.is_valid()) {
		return false;
	}

	return _physics->is_built();
}

bool Terrain3D::is_collision_rebuild_pending() const {
	if (!_physics.is_valid()) {
		return false;
	}

	return _physics->is_rebuild_in_flight();
}

void Terrain3D::set_focus_path(const NodePath &p_path) {
	_focus_path = p_path;
}

NodePath Terrain3D::get_focus_path() const {
	return _focus_path;
}

void Terrain3D::set_editor_focus_override(const Vector3 p_world_position) {
	_editor_focus_override = p_world_position;
	_has_editor_focus_override = true;
}

void Terrain3D::clear_editor_focus_override() {
	_editor_focus_override = Vector3();
	_has_editor_focus_override = false;
}

void Terrain3D::set_editor_preview(const bool p_enabled) {
	if (_editor_preview == p_enabled) {
		return;
	}

	_editor_preview = p_enabled;

	if (!Engine::get_singleton()->is_editor_hint() || !_renderer.is_valid()) {
		return;
	}

	if (_editor_preview) {
		_renderer->initialize(this);
		_on_config_changed();
	} else {
		_renderer->cleanup();
	}
}

bool Terrain3D::get_editor_preview() const {
	return _editor_preview;
}

void Terrain3D::set_editor_preview_physics(const bool p_enabled) {
	if (_editor_preview_physics == p_enabled) {
		return;
	}

	_editor_preview_physics = p_enabled;

	if (!Engine::get_singleton()->is_editor_hint() || !_physics.is_valid()) {
		return;
	}

	if (_editor_preview_physics) {
		_physics->initialize(this);
	} else {
		_physics->cleanup();
	}
}

bool Terrain3D::get_editor_preview_physics() const {
	return _editor_preview_physics;
}

void Terrain3D::reload_shader() {
	if (_renderer.is_valid()) {
		_renderer->request_shader_reload();
	}

	_on_config_changed();
}

void Terrain3D::rebuild() {
	_on_config_changed();
}

const std::vector<Terrain3D *> &Terrain3D::get_editor_instances() {
	return _editor_instances;
}

} //namespace ts