#include "terrain_renderer.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace ts {

void TerrainRenderer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh_resolution", "resolution"), &TerrainRenderer::set_mesh_resolution);
	ClassDB::bind_method(D_METHOD("set_terrain_size", "size"), &TerrainRenderer::set_terrain_size);
	ClassDB::bind_method(D_METHOD("set_material_override", "material"), &TerrainRenderer::set_material_override);
	ClassDB::bind_method(D_METHOD("generate_mesh"), &TerrainRenderer::generate_mesh);
}

TerrainRenderer::TerrainRenderer() : _mesh_instance(nullptr), _parent_node(nullptr), _mesh_resolution(32), _terrain_size(1024.0f) {
}

TerrainRenderer::~TerrainRenderer() {
	cleanup();
}

void TerrainRenderer::initialize(Node3D *p_parent) {
	_parent_node = p_parent;

	if (_mesh_instance == nullptr) {
		_mesh_instance = Object::cast_to<MeshInstance3D>(_parent_node->get_node_or_null("TerrainMesh"));
	}
}

void TerrainRenderer::cleanup() {
	if (_mesh_instance != nullptr && ObjectDB::get_instance(_mesh_instance->get_instance_id()) != nullptr) {
		if (_mesh_instance->is_inside_tree()) {
			_mesh_instance->queue_free();
		} else {
			memdelete(_mesh_instance);
		}
		_mesh_instance = nullptr;
	}
}

void TerrainRenderer::set_generator(const Ref<TerrainGenerator> &p_generator) {
	_generator = p_generator;
}

void TerrainRenderer::set_mesh_resolution(int p_resolution) {
	_mesh_resolution = p_resolution;
}

void TerrainRenderer::set_terrain_size(float p_size) {
	_terrain_size = p_size;
}

void TerrainRenderer::set_material_override(const Ref<Material> &p_material) {
	_material_override = p_material;
	if (_mesh_instance != nullptr && _material_override.is_valid()) {
		_mesh_instance->set_material_override(_material_override);
	}
}

void TerrainRenderer::generate_mesh() {
	if (!_generator.is_valid()) {
		ERR_FAIL_MSG("TerrainRenderer: Invalid generator");
	}

	if (_parent_node == nullptr) {
		ERR_FAIL_MSG("TerrainRenderer: No parent node set");
	}

	if (_mesh_instance == nullptr || ObjectDB::get_instance(_mesh_instance->get_instance_id()) == nullptr) {
		_mesh_instance = memnew(MeshInstance3D);
		_parent_node->add_child(_mesh_instance);
		_mesh_instance->set_name("TerrainMesh");
		_mesh_instance->set_owner(_parent_node->get_owner());
	}

	float vertex_spacing = _terrain_size / _mesh_resolution;
	Ref<ArrayMesh> mesh = _generator->create_mesh_data(_mesh_resolution, vertex_spacing);
	_mesh_instance->set_mesh(mesh);

	if (_material_override.is_valid()) {
		_mesh_instance->set_material_override(_material_override);
	}
}

void TerrainRenderer::update_mesh() {
	generate_mesh();
}

} //namespace ts