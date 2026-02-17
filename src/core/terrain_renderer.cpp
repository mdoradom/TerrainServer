#include "terrain_renderer.h"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

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

	_internal_shader_rid = rs->shader_create();

	rs->shader_set_code(_internal_shader_rid, R"(
	    shader_type spatial;

	    uniform sampler2D height_map : repeat_disable;
	    uniform float height_scale;

	    // TODO Basic visual parameters (will be replaced by layer system in the biomes implementation)
	    uniform vec3 albedo_color : source_color = vec3(0.5, 0.7, 0.3);
	    uniform float roughness : hint_range(0,1) = 0.8;

	    void vertex() {
	        float h = texture(height_map, UV).r;
	        VERTEX.y += h * height_scale;

	        vec2 tex_size = vec2(textureSize(height_map, 0));
	        vec2 e = vec2(1.0 / tex_size.x, 1.0 / tex_size.y);

	        vec2 uv_clamped_l = clamp(UV - vec2(e.x, 0.0), vec2(0.0), vec2(1.0));
	        vec2 uv_clamped_r = clamp(UV + vec2(e.x, 0.0), vec2(0.0), vec2(1.0));
	        vec2 uv_clamped_u = clamp(UV - vec2(0.0, e.y), vec2(0.0), vec2(1.0));
	        vec2 uv_clamped_d = clamp(UV + vec2(0.0, e.y), vec2(0.0), vec2(1.0));

	        float h_l = texture(height_map, uv_clamped_l).r;
	        float h_r = texture(height_map, uv_clamped_r).r;
	        float h_u = texture(height_map, uv_clamped_u).r;
			float h_d = texture(height_map, uv_clamped_d).r;

            vec3 normal_vec;
            normal_vec.x = (h_l - h_r) * height_scale;
            normal_vec.z = (h_u - h_d) * height_scale;
            normal_vec.y = 2.0;

            NORMAL = normalize(normal_vec);
        }

        void fragment() {
            // TODO: Replace with multi-layer material system
            ALBEDO = albedo_color;
            ROUGHNESS = roughness;
        }
    )");

	_internal_material_rid = rs->material_create();
	rs->material_set_shader(_internal_material_rid, _internal_shader_rid);

	PackedVector3Array vertices;
	PackedVector2Array uvs;
	PackedVector3Array normals;
	PackedInt32Array indices;

	int vertex_count = p_resolution + 1;
	float step = p_size / (float)p_resolution;
	Vector3 offset = Vector3(-p_size * 0.5f, 0.0f, -p_size * 0.5f);

	vertices.resize(vertex_count * vertex_count);
	uvs.resize(vertex_count * vertex_count);
	normals.resize(vertex_count * vertex_count);

	for (int z = 0; z < vertex_count; z++) {
		for (int x = 0; x < vertex_count; x++) {
			int i = z * vertex_count + x;
			vertices[i] = offset + Vector3(x * step, 0.0f, z * step);
			uvs[i] = Vector2((float)x / p_resolution, (float)z / p_resolution);
			normals[i] = Vector3(0.0f, 1.0f, 0.0f);
		}
	}

	indices.resize(p_resolution * p_resolution * 6);
	int idx = 0;
	for (int z = 0; z < p_resolution; z++) {
		for (int x = 0; x < p_resolution; x++) {
			int top_left = z * vertex_count + x;
			int top_right = top_left + 1;
			int bottom_left = (z + 1) * vertex_count + x;
			int bottom_right = bottom_left + 1;

			indices[idx++] = top_left;
			indices[idx++] = top_right;
			indices[idx++] = bottom_left;

			indices[idx++] = top_right;
			indices[idx++] = bottom_right;
			indices[idx++] = bottom_left;
		}
	}

	Array arrays;
	arrays.resize(ArrayMesh::ARRAY_MAX);
	arrays[ArrayMesh::ARRAY_VERTEX] = vertices;
	arrays[ArrayMesh::ARRAY_TEX_UV] = uvs;
	arrays[ArrayMesh::ARRAY_NORMAL] = normals;
	arrays[ArrayMesh::ARRAY_INDEX] = indices;

	_mesh_rid = rs->mesh_create();
	rs->mesh_add_surface_from_arrays(_mesh_rid, RenderingServer::PRIMITIVE_TRIANGLES, arrays);

	_instance_rid = rs->instance_create();
	rs->instance_set_base(_instance_rid, _mesh_rid);
	rs->instance_geometry_set_material_override(_instance_rid, _internal_material_rid);

	if (_parent_node && _parent_node->is_inside_tree()) {
		rs->instance_set_scenario(_instance_rid, _parent_node->get_world_3d()->get_scenario());
	}
}

void TerrainRenderer::update_shader_params(const Ref<Texture2D> &p_height_map, float p_scale) {
	_height_map_texture = p_height_map;

	if (!_internal_material_rid.is_valid() || _height_map_texture.is_null()) {
		return;
	}
	RenderingServer *rs = RenderingServer::get_singleton();

	rs->material_set_param(_internal_material_rid, "height_map", _height_map_texture->get_rid());
	rs->material_set_param(_internal_material_rid, "height_scale", p_scale);
}


void TerrainRenderer::update_render_state() {
	if (!_instance_rid.is_valid() || !_parent_node) {
		return;
	}
	RenderingServer::get_singleton()->instance_set_transform(_instance_rid, _parent_node->get_global_transform());
}

} //namespace ts