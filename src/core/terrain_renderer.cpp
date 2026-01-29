#include "terrain_renderer.h"

#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace ts {

void TerrainRenderer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh_resolution", "resolution"), &TerrainRenderer::set_mesh_resolution);
	ClassDB::bind_method(D_METHOD("set_material_override", "material"), &TerrainRenderer::set_material_override);
	ClassDB::bind_method(D_METHOD("generate_debug_mesh"), &TerrainRenderer::generate_debug_mesh);
}

TerrainRenderer::TerrainRenderer() : _debug_mesh_instance(nullptr), _parent_node(nullptr), _mesh_resolution(32) {
}

TerrainRenderer::~TerrainRenderer() {
	cleanup();
}

void TerrainRenderer::initialize(Node3D *p_parent) {
	_parent_node = p_parent;
}

void TerrainRenderer::cleanup() {
	if (_debug_mesh_instance != nullptr) {
		_debug_mesh_instance->queue_free();
		_debug_mesh_instance = nullptr;
	}
}

void TerrainRenderer::set_generator(const Ref<TerrainGenerator> &p_generator) {
	_generator = p_generator;
}

void TerrainRenderer::set_mesh_resolution(int p_resolution) {
	_mesh_resolution = p_resolution;
}

void TerrainRenderer::set_material_override(const Ref<Material> &p_material) {
	_material_override = p_material;
	if (_debug_mesh_instance != nullptr && _material_override.is_valid()) {
		_debug_mesh_instance->set_material_override(_material_override);
	}
}

void TerrainRenderer::generate_debug_mesh() {
	if (!_generator.is_valid() || _parent_node == nullptr) {
		return;
	}

	if (_debug_mesh_instance == nullptr) {
		_debug_mesh_instance = memnew(MeshInstance3D);
		_parent_node->add_child(_debug_mesh_instance);
		_debug_mesh_instance->set_name("DebugTerrainMesh");
		_debug_mesh_instance->set_owner(_parent_node->get_owner());
	}

	int size = _mesh_resolution;
	float vertex_spacing = 1.0f;

	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES);

	for (int z = 0; z < size; z++) {
		for (int x = 0; x < size; x++) {
			float x0 = x * vertex_spacing;
			float z0 = z * vertex_spacing;
			float x1 = (x + 1) * vertex_spacing;
			float z1 = (z + 1) * vertex_spacing;

			float y00 = _generator->get_height(x0, z0);
			float y10 = _generator->get_height(x1, z0);
			float y01 = _generator->get_height(x0, z1);
			float y11 = _generator->get_height(x1, z1);

			// Triangle 1
			st->set_normal(Vector3(0, 1, 0));
			st->set_uv(Vector2(0, 0));
			st->add_vertex(Vector3(x0, y00, z0));

			st->set_normal(Vector3(0, 1, 0));
			st->set_uv(Vector2(1, 0));
			st->add_vertex(Vector3(x1, y10, z0));

			st->set_normal(Vector3(0, 1, 0));
			st->set_uv(Vector2(0, 1));
			st->add_vertex(Vector3(x0, y01, z1));

			// Triangle 2
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

	if (_material_override.is_valid()) {
		_debug_mesh_instance->set_material_override(_material_override);
	}
}

void TerrainRenderer::update_mesh() {
	generate_debug_mesh();
}

} //namespace ts