#include "terrain3d.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/noise_texture2d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace ts {

void Terrain3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_configuration", "config"), &Terrain3D::set_configuration);
	ClassDB::bind_method(D_METHOD("get_configuration"), &Terrain3D::get_configuration);
	ClassDB::bind_method(D_METHOD("_on_config_changed"), &Terrain3D::_on_config_changed);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "configuration", PROPERTY_HINT_RESOURCE_TYPE, "TerrainConfiguration"), "set_configuration", "get_configuration");
}

Terrain3D::Terrain3D() {
	set_notify_transform(true);
	_renderer.instantiate();
}

Terrain3D::~Terrain3D() {
	if (_config.is_valid()) {
		_config->disconnect("changed", Callable(this, "_on_config_changed"));
	}

	if (_renderer.is_valid()) {
		_renderer->cleanup();
	}
}

void Terrain3D::_ready() {
	_renderer->initialize(this);
	_on_config_changed();
}

void Terrain3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_TRANSFORM_CHANGED: {
			if (_renderer.is_valid()) {
				_renderer->update_render_state();
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (_renderer.is_valid()) {
				_renderer->cleanup();
			}
		} break;

		case NOTIFICATION_ENTER_TREE: {
			if (_renderer.is_valid()) {
				_renderer->initialize(this);
				_on_config_changed();
			}
		} break;
	}
}

void Terrain3D::_on_config_changed() {
	if (!_config.is_valid() || !_renderer.is_valid()) {
		return;
	}

	_renderer->rebuild_mesh(_config->get_terrain_size(), _config->get_mesh_resolution());

	if (_config->get_noise().is_valid()) {
		Ref<NoiseTexture2D> tex;
		tex.instantiate();
		tex->set_noise(_config->get_noise());
		_renderer->update_shader_params(tex, _config->get_height_scale());
	}

	_renderer->update_render_state();
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

} //namespace ts