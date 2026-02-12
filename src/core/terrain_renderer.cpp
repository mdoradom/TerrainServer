#include "terrain_renderer.h"
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/world3d.hpp>

using namespace godot;

namespace ts {

void TerrainRenderer::_bind_methods() {}

TerrainRenderer::TerrainRenderer() {}

TerrainRenderer::~TerrainRenderer() {
	cleanup();
}

void TerrainRenderer::initialize(Node3D *p_parent) {
	_parent_node = p_parent;
}

void TerrainRenderer::cleanup() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (_instance_rid.is_valid()) {
		rs->free_rid(_instance_rid);
		_instance_rid = RID();
	}
	if (_mesh_rid.is_valid()) {
		rs->free_rid(_mesh_rid);
		_mesh_rid = RID();
	}
	if (_internal_shader_rid.is_valid()) {
		rs->free_rid(_internal_shader_rid);
		_internal_shader_rid = RID();
	}
	if (_internal_material_rid.is_valid()) {
		rs->free_rid(_internal_material_rid);
		_internal_material_rid = RID();
	}
}

void TerrainRenderer::rebuild_mesh(float p_size, int p_resolution) {
	RenderingServer *rs = RenderingServer::get_singleton();
	cleanup();

	// Create displacement shader for vertex manipulation based on height map
	// TODO fix this, right now is not working, also we need to add UVs to the mesh
	_internal_shader_rid = rs->shader_create();
	rs->shader_set_code(_internal_shader_rid, R"(
		shader_type spatial;
		uniform sampler2D height_map;
		uniform float height_scale;
		void vertex() {
			float h = texture(height_map, UV).r;
			VERTEX.y += h * height_scale;
		}
		void fragment() {
			ALBEDO = vec3(0.5); // Color gris por defecto
			ROUGHNESS = 0.8;
		}
	)");

	// Create internal material using the shader
	_internal_material_rid = rs->material_create();
	rs->material_set_shader(_internal_material_rid, _internal_shader_rid);

	// Create geometry
	_mesh_rid = rs->mesh_create();
	Ref<PlaneMesh> pm;
	pm.instantiate();
	pm->set_size(Vector2(p_size, p_size));
	pm->set_subdivide_depth(p_resolution);
	pm->set_subdivide_width(p_resolution);
	rs->mesh_add_surface_from_arrays(_mesh_rid, RenderingServer::PRIMITIVE_TRIANGLES, pm->get_mesh_arrays());

	// Create instance and apply material
	_instance_rid = rs->instance_create();
	rs->instance_set_base(_instance_rid, _mesh_rid);
	rs->instance_geometry_set_material_override(_instance_rid, _internal_material_rid);

	if (_parent_node && _parent_node->is_inside_tree()) {
		rs->instance_set_scenario(_instance_rid, _parent_node->get_world_3d()->get_scenario());
	}
}

void TerrainRenderer::update_shader_params(const Ref<Texture2D> &p_height_map, float p_scale) {
	if (!_internal_material_rid.is_valid() || p_height_map.is_null()) {
		return;
	}
	RenderingServer *rs = RenderingServer::get_singleton();

	rs->material_set_param(_internal_material_rid, "height_map", p_height_map->get_rid());
	rs->material_set_param(_internal_material_rid, "height_scale", p_scale);
}

void TerrainRenderer::update_render_state() {
	if (!_instance_rid.is_valid() || !_parent_node) {
		return;
	}
	RenderingServer::get_singleton()->instance_set_transform(_instance_rid, _parent_node->get_global_transform());
}

} //namespace ts