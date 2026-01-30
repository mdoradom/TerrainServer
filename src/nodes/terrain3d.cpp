#include "terrain3d.h"

#include "godot_cpp/classes/engine.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace ts {

void Terrain3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_configuration", "config"), &Terrain3D::set_configuration);
	ClassDB::bind_method(D_METHOD("get_configuration"), &Terrain3D::get_configuration);
	ClassDB::bind_method(D_METHOD("get_generator"), &Terrain3D::get_generator);
	ClassDB::bind_method(D_METHOD("get_renderer"), &Terrain3D::get_renderer);
	ClassDB::bind_method(D_METHOD("_update_generator"), &Terrain3D::_update_generator);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "configuration", PROPERTY_HINT_RESOURCE_TYPE, "TerrainConfiguration"), "set_configuration", "get_configuration");
}

void Terrain3D::_update_generator() {
	if (_config.is_valid() && _generator.is_valid() && _renderer.is_valid()) {
		_generator->setup(_config);
		_renderer->set_generator(_generator);
		_renderer->set_mesh_resolution(_config->get_mesh_resolution());
		_renderer->set_terrain_size(_config->get_terrain_size());
		_renderer->set_material_override(_config->get_material_override());

		// Regenerate the mesh if we're already in the scene tree
		if (is_inside_tree()) {
			_renderer->update_mesh();
		}
		UtilityFunctions::print("Terrain3D: Generator configuration updated.");
	}
}

Terrain3D::Terrain3D() {
	_generator.instantiate();
	_renderer.instantiate();
	UtilityFunctions::print("Terrain3D: Initialized");
}

Terrain3D::~Terrain3D() {
	if (_config.is_valid() && _config->is_connected("changed", Callable(this, "_update_generator"))) {
		_config->disconnect("changed", Callable(this, "_update_generator"));
	}

	if (_renderer.is_valid()) {
		_renderer->cleanup();
	}

	UtilityFunctions::print("Terrain3D: Destroyed");
}

void Terrain3D::_ready() {
	_renderer->initialize(this);
	_update_generator();
}

void Terrain3D::_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
}

Ref<TerrainConfiguration> Terrain3D::get_configuration() const {
	return _config;
}

void Terrain3D::set_configuration(const Ref<TerrainConfiguration> &p_config) {
	if (_config == p_config) {
		return;
	}

	if (_config.is_valid()) {
		_config->disconnect("changed", Callable(this, "_update_generator"));
	}

	_config = p_config;

	if (_config.is_valid()) {
		_config->connect("changed", Callable(this, "_update_generator"));
	}

	_update_generator();
}

Ref<TerrainGenerator> Terrain3D::get_generator() const {
	return _generator;
}

Ref<TerrainRenderer> Terrain3D::get_renderer() const {
	return _renderer;
}

} //namespace ts