#include "terrain3d.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/surface_tool.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace ts {

void Terrain3D::_bind_methods() {
	// --- Configuration (Resource) ---
	ClassDB::bind_method(D_METHOD("set_configuration", "config"), &Terrain3D::set_configuration);
	ClassDB::bind_method(D_METHOD("get_configuration"), &Terrain3D::get_configuration);
	ClassDB::bind_method(D_METHOD("_update_generator"), &Terrain3D::_update_generator);

	ADD_GROUP("Data Source", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "configuration", PROPERTY_HINT_RESOURCE_TYPE, "TerrainConfiguration"), "set_configuration", "get_configuration");

	// --- Visuals (Node Parameters) ---
	ClassDB::bind_method(D_METHOD("set_mesh_resolution", "resolution"), &Terrain3D::set_mesh_resolution);
	ClassDB::bind_method(D_METHOD("get_mesh_resolution"), &Terrain3D::get_mesh_resolution);

	ClassDB::bind_method(D_METHOD("set_material", "material"), &Terrain3D::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &Terrain3D::get_material);

	ADD_GROUP("Visuals", "");
	// Range from 2 to 256 vertex
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_resolution", PROPERTY_HINT_RANGE, "2,256,1"), "set_mesh_resolution", "get_mesh_resolution");
	// Slot for material override
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_material", "get_material");
}

void Terrain3D::_update_generator() {
	if (_config.is_valid() && _generator.is_valid()) {
		_generator->setup(_config);
		_generate_mesh();
		UtilityFunctions::print("TerrainServer:: Generator configuration updated.");
	}
}

void Terrain3D::_generate_mesh() {
	if (!_generator.is_valid()) {
		return;
	}

	if (_mesh_instance == nullptr) {
		_mesh_instance = memnew(MeshInstance3D);
		add_child(_mesh_instance);
		_mesh_instance->set_name("TerrainMesh");
		_mesh_instance->set_owner(get_owner());
	}

	float size = _mesh_resolution * 1.0f;
	Ref<ArrayMesh> mesh = _generator->generate_mesh(_mesh_resolution, size);
	_mesh_instance->set_mesh(mesh);

	// Apply material if set
	if (_material_override.is_valid()) {
		_mesh_instance->set_material_override(_material_override);
	}
}

Terrain3D::Terrain3D() {
	_generator.instantiate();
	UtilityFunctions::print("TerrainServer: initialized");
}

Terrain3D::~Terrain3D() {
	_mesh_instance = nullptr;
	UtilityFunctions::print("TerrainServer: destroyed");
}

void Terrain3D::_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	UtilityFunctions::print("Height at (0,0): ", _generator->get_height(0.0f, 0.0f));
}

Ref<TerrainConfiguration> Terrain3D::get_configuration() const {
	return _config;
}

void Terrain3D::set_configuration(const Ref<TerrainConfiguration> &p_config) {
	// If the same config, do nothing
	if (_config == p_config) {
		return;
	}

	// Disconnect from previous config signals
	if (_config.is_valid()) {
		_config->disconnect("changed", Callable(this, "_update_generator"));
	}

	// Set new config
	_config = p_config;

	// Connect to new config signals
	if (_config.is_valid()) {
		_config->connect("changed", Callable(this, "_update_generator"));
	}

	_update_generator();
}

int Terrain3D::get_mesh_resolution() const {
	return _mesh_resolution;
}

void Terrain3D::set_mesh_resolution(int p_resolution) {
	if (p_resolution < 2) {
		p_resolution = 2;
	}

	if (p_resolution == _mesh_resolution) {
		return;
	}

	_mesh_resolution = p_resolution;
	_generate_mesh();
}

Ref<Material> Terrain3D::get_material() const {
	return _material_override;
}

void Terrain3D::set_material(const Ref<Material> &p_material) {
	if (_material_override == p_material) {
		return;
	}

	_material_override = p_material;

	if (_mesh_instance != nullptr) {
		_mesh_instance->set_material_override(_material_override);
	}
}

} //namespace ts