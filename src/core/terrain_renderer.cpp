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

	for (const auto &level : _clipmap_levels) {
		if (level.instance_rid.is_valid()) {
			rs->free_rid(level.instance_rid);
		}
	}
	_clipmap_levels.clear();

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

void TerrainRenderer::set_generator(const Ref<TerrainGenerator> &p_generator) {
	_generator = p_generator;
}

void TerrainRenderer::set_configuration(const Ref<TerrainConfiguration> &p_config) {
	_config = p_config;
}

void TerrainRenderer::rebuild_mesh(float p_size, int p_resolution) {

	RenderingServer *rs = RenderingServer::get_singleton();
	cleanup();

	if (!_generator.is_valid() || !_config.is_valid()) {
		return;
	}

	_internal_shader_rid = rs->shader_create();
	rs->shader_set_code(_internal_shader_rid, R"(
		shader_type spatial;
		render_mode cull_back;

		uniform sampler2D height_map;
		uniform float height_scale;

		varying vec3 v_normal;

		float get_h(vec2 pos_uv) {
			return texture(height_map, pos_uv).r * height_scale;
		}

		void vertex() {
			vec3 world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
			vec2 tex_uv = vec2(world_pos.x, world_pos.z) / 1024.0 + 0.5;

			VERTEX.y = get_h(tex_uv);

			float e = 1.0 / 1024.0;
			float h_l = get_h(tex_uv + vec2(-e, 0.0));
			float h_r = get_h(tex_uv + vec2(e, 0.0));
			float h_u = get_h(tex_uv + vec2(0.0, -e));
			float h_d = get_h(tex_uv + vec2(0.0, e));

			vec3 n;
			n.x = h_l - h_r;
			n.z = h_u - h_d;
			n.y = 2.0;
			v_normal = normalize(n);
			NORMAL = v_normal;
		}

		void fragment() {
			// TODO Basic visual parameters (will be replaced by layer system in the biomes implementation). Replace with multi-layer material system
			ALBEDO = vec3(1.0, 1.0, 1.0);
			ROUGHNESS = 0.8;
			NORMAL = mat3(VIEW_MATRIX) * v_normal;
		}
	)");

	_internal_material_rid = rs->material_create();
	rs->material_set_shader(_internal_material_rid, _internal_shader_rid);

	float base_spacing = 1.0f / p_resolution;
	Ref<ArrayMesh> mesh = _generator->create_mesh_data(p_resolution, base_spacing);

	_mesh_rid = rs->mesh_create();
	Array arrays = mesh->surface_get_arrays(0);
	rs->mesh_add_surface_from_arrays(_mesh_rid, RenderingServer::PRIMITIVE_TRIANGLES, arrays);

	int num_levels = _config->get_clipmap_levels();
	float base_size = 32.0f;

	for (int i = 0; i < num_levels; i++) {
		RID instance = rs->instance_create();
		rs->instance_set_base(instance, _mesh_rid);
		rs->instance_geometry_set_material_override(instance, _internal_material_rid);

		if (_parent_node && _parent_node->is_inside_tree()) {
			rs->instance_set_scenario(instance, _parent_node->get_world_3d()->get_scenario());
		}

		ClipmapLevel level;
		level.instance_rid = instance;
		level.scale = base_size * pow(2.0f, i);

		_clipmap_levels.push_back(level);
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

void TerrainRenderer::update_camera_position(Vector3 p_camera_pos) {
	RenderingServer *rs = RenderingServer::get_singleton();

	if (!_config.is_valid()) {
		return;
	}
	int resolution = _config->get_mesh_resolution();
	if (resolution <= 0) {
		resolution = 64;
	}

	for (size_t i = 0; i < _clipmap_levels.size(); i++) {
		const auto &level = _clipmap_levels[i];

		float cell_size = level.scale / (float)resolution;

		float snapped_x = floor(p_camera_pos.x / cell_size) * cell_size;
		float snapped_z = floor(p_camera_pos.z / cell_size) * cell_size;

		Transform3D xform;
		xform.basis = xform.basis.scaled(Vector3(level.scale, 1.0, level.scale));
		xform.origin = Vector3(snapped_x, -0.01f * i, snapped_z);

		rs->instance_set_transform(level.instance_rid, xform);
	}
}

} //namespace ts