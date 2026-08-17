#include "terrain3d.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace ts {

void Terrain3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_configuration", "config"), &Terrain3D::set_configuration);
	ClassDB::bind_method(D_METHOD("get_configuration"), &Terrain3D::get_configuration);
	ClassDB::bind_method(D_METHOD("get_height_at", "world_xz"), &Terrain3D::get_height_at);
	ClassDB::bind_method(D_METHOD("set_focus_path", "path"), &Terrain3D::set_focus_path);
	ClassDB::bind_method(D_METHOD("get_focus_path"), &Terrain3D::get_focus_path);
	ClassDB::bind_method(D_METHOD("_on_config_changed"), &Terrain3D::_on_config_changed);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "configuration", PROPERTY_HINT_RESOURCE_TYPE, "TerrainConfiguration"), "set_configuration", "get_configuration");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "focus_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Node3D"), "set_focus_path", "get_focus_path");
}

Terrain3D::Terrain3D() {
	set_notify_transform(true);
	set_process(true);
	_renderer.instantiate();
	_generator.instantiate();
	_physics.instantiate();
}

Terrain3D::~Terrain3D() {
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

	const Viewport *viewport = get_viewport();
	const Camera3D *camera = viewport != nullptr ? viewport->get_camera_3d() : nullptr;

	Vector3 focus_pos;
	bool has_focus = false;

	if (!_focus_path.is_empty()) {
		if (const Node3D *focus_node = Object::cast_to<Node3D>(get_node_or_null(_focus_path))) {
			focus_pos = focus_node->get_global_position();
			has_focus = true;
		}
	}

	if (!has_focus && camera != nullptr) {
		focus_pos = camera->get_global_position();
		has_focus = true;
	}

	if (!has_focus) {
		return;
	}

	if (_renderer.is_valid()) {
		_renderer->update_focus_position(focus_pos);
	}

	if (_physics.is_valid()) {
		_physics->update_focus_position(focus_pos);
	}
}

void Terrain3D::_notification(const int p_what) {
	switch (p_what) {
		case NOTIFICATION_EXIT_TREE: {
			if (_physics.is_valid()) {
				_physics->cleanup();
			}
			if (_renderer.is_valid()) {
				_renderer->cleanup();
			}
		} break;

		case NOTIFICATION_ENTER_TREE: {
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

void Terrain3D::set_focus_path(const NodePath &p_path) {
	_focus_path = p_path;
}

NodePath Terrain3D::get_focus_path() const {
	return _focus_path;
}

} //namespace ts