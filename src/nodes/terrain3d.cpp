#include "terrain3d.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/surface_tool.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace ts {

void Terrain3D::_bind_methods() {
	// TODO Binding methods can be done here
	ClassDB::bind_method(D_METHOD("get_configuration"), &Terrain3D::get_configuration);
	ClassDB::bind_method(D_METHOD("set_configuration", "config"), &Terrain3D::set_configuration);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "configuration", PROPERTY_HINT_RESOURCE_TYPE, "TerrainConfiguration"), "set_configuration", "get_configuration");

	//
	ClassDB::bind_method(D_METHOD("_update_generator"), &Terrain3D::_update_generator);
}

void Terrain3D::_update_generator() {
	if (_config.is_valid() and _generator.is_valid()) {
		_generator->setup(_config);
		_generate_debug_mesh();
		UtilityFunctions::print("TerrainServer:: Generator configuration updated.");
	}
}

void Terrain3D::_generate_debug_mesh() {
	if (!_generator.is_valid()) {
		return;
	}

	if (_debug_mesh_instance == nullptr) {
		_debug_mesh_instance = memnew(MeshInstance3D);
		add_child(_debug_mesh_instance);
		_debug_mesh_instance->set_name("DebugTerrainMesh");
		_debug_mesh_instance->set_owner(get_owner());
	}

	int size = 64;
	float vertex_spacing = 1.0f;

	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES);

	for (int z = 0; z < size; z++) {
		for (int x = 0; x < size; x++) {
			// Get the four corners of the quad
			float x0 = x * vertex_spacing;
			float z0 = z * vertex_spacing;
			float x1 = (x + 1) * vertex_spacing;
			float z1 = (z + 1) * vertex_spacing;

			float y00 = _generator->get_height(x0, z0);
			float y10 = _generator->get_height(x1, z0);
			float y01 = _generator->get_height(x0, z1);
			float y11 = _generator->get_height(x1, z1);

			// Triangle 1 (0,0 -> 1,0 -> 0,1)
			st->set_normal(Vector3(0, 1, 0));
			st->set_uv(Vector2(0, 0));
			st->add_vertex(Vector3(x0, y00, z0));

			st->set_normal(Vector3(0, 1, 0));
			st->set_uv(Vector2(1, 0));
			st->add_vertex(Vector3(x1, y10, z0));

			st->set_normal(Vector3(0, 1, 0));
			st->set_uv(Vector2(0, 1));
			st->add_vertex(Vector3(x0, y01, z1));

			// Triangle 2 (1,0 -> 1,1 -> 0,1)
			st->set_normal(Vector3(0, 1, 0));
			st->set_uv(Vector2(1, 0));
			st->add_vertex(Vector3(x1, y10, z0));

			st->set_normal(Vector3(0, 1, 0));
			st->set_uv(Vector2(1, 1));
			st->add_vertex(Vector3(x1, y11, z1));

			st->set_normal(Vector3(0, 1, 0));
			st->set_uv(Vector2(0, 1));
			st->add_vertex(Vector3(x0, y01, z1));
		}
	}
	st->generate_normals();

	_debug_mesh_instance->set_mesh(st->commit());
}

Terrain3D::Terrain3D() {
	_generator.instantiate();
	UtilityFunctions::print("TerrainServer: initialized");
}

Terrain3D::~Terrain3D() {
	_debug_mesh_instance = nullptr;
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

} //namespace ts