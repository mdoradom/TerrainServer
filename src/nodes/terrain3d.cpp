#include "terrain3d.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/viewport.hpp>
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
	set_process(true);
	_renderer.instantiate();
	_generator.instantiate();
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

void Terrain3D::_process(double delta) {
	if (_renderer.is_valid() && is_inside_tree()) {
		Viewport *viewport = get_viewport();
		if (viewport != nullptr) {
			Camera3D *camera = viewport->get_camera_3d();
			if (camera != nullptr) {
				_renderer->update_camera_position(camera->get_global_position());
			}
		}
	}
}

void Terrain3D::_notification(int p_what) {
	switch (p_what) {
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
	if (!_config.is_valid() || !_renderer.is_valid() || !_generator.is_valid()) {
		return;
	}

	// Inyectamos las dependencias
	_generator->setup(_config);
	_renderer->set_generator(_generator);
	_renderer->set_configuration(_config);

	int resolution = _config->get_mesh_resolution();
	_renderer->rebuild_mesh(_config->get_terrain_size(), resolution);

	if (_config->get_noise().is_valid()) {
		Ref<Image> img = _config->get_noise()->get_image(resolution, resolution);
		Ref<ImageTexture> tex = ImageTexture::create_from_image(img);
		_renderer->update_shader_params(tex, _config->get_height_scale());
	}

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