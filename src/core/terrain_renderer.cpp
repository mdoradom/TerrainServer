#include "terrain_renderer.h"
#include "fast_noise_lite_glsl.h"

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

	if (_mesh_ring_rid.is_valid()) {
		rs->free_rid(_mesh_ring_rid);
		_mesh_ring_rid = RID();
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
	String shader_code = String(R"(
		shader_type spatial;
		render_mode cull_disabled;

		uniform float height_scale;
		uniform float terrain_size;
		uniform float resolution;

		uniform int fnl_seed = 0;
		uniform float fnl_frequency = 0.01;
		uniform int fnl_noise_type = 1;
		uniform int fnl_fractal_type = 1;
		uniform int fnl_octaves = 3;
		uniform float fnl_lacunarity = 2.0;
		uniform float fnl_gain = 0.5;
	)") + String(FNL_GLSL_CODE) +
			String(R"(
		varying vec3 v_normal;

		fnl_state get_fnl_state() {
			fnl_state state = fnlCreateState(fnl_seed);
			state.noise_type = fnl_noise_type;
			state.fractal_type = fnl_fractal_type;
			state.octaves = fnl_octaves;
			state.lacunarity = fnl_lacunarity;
			state.gain = fnl_gain;
			state.frequency = fnl_frequency;
			return state;
		}

		float get_h(vec2 world_xz, fnl_state state) {
			return fnlGetNoise2D(state, world_xz.x, world_xz.y) * height_scale;
		}

		void vertex() {
			fnl_state state = get_fnl_state();

			float drop = VERTEX.y;

			vec3 world_pos = (MODEL_MATRIX * vec4(VERTEX.x, 0.0, VERTEX.z, 1.0)).xyz;
			vec2 world_xz = vec2(world_pos.x, world_pos.z);

			float h = get_h(world_xz, state);

			vec3 final_world = vec3(world_pos.x, h + (drop * height_scale * 0.5), world_pos.z);
			VERTEX = (inverse(MODEL_MATRIX) * vec4(final_world, 1.0)).xyz;

			float e = 0.1;
			vec3 n;
			n.x = get_h(world_xz + vec2(-e, 0.0), state) - get_h(world_xz + vec2(e, 0.0), state);
			n.z = get_h(world_xz + vec2(0.0, -e), state) - get_h(world_xz + vec2(0.0, e), state);
			n.y = 2.0 * e;
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

	rs->shader_set_code(_internal_shader_rid, shader_code);
	_internal_material_rid = rs->material_create();
	rs->material_set_shader(_internal_material_rid, _internal_shader_rid);

	rs->material_set_param(_internal_material_rid, "terrain_size", p_size);
	rs->material_set_param(_internal_material_rid, "resolution", (float)p_resolution);

	float base_spacing = 1.0f / p_resolution;

	Ref<ArrayMesh> mesh_center = _generator->create_mesh_data(p_resolution, base_spacing, false);
	_mesh_rid = rs->mesh_create();
	rs->mesh_add_surface_from_arrays(_mesh_rid, RenderingServer::PRIMITIVE_TRIANGLES, mesh_center->surface_get_arrays(0));

	Ref<ArrayMesh> mesh_ring = _generator->create_mesh_data(p_resolution, base_spacing, true);
	_mesh_ring_rid = rs->mesh_create();
	rs->mesh_add_surface_from_arrays(_mesh_ring_rid, RenderingServer::PRIMITIVE_TRIANGLES, mesh_ring->surface_get_arrays(0));

	int num_levels = _config->get_clipmap_levels();
	if (num_levels <= 0) {
		num_levels = 6;
	}

	float base_size = p_size / pow(2.0f, num_levels - 1);

	for (int i = 0; i < num_levels; i++) {
		RID instance = rs->instance_create();

		RID mesh_to_use = (i == 0) ? _mesh_rid : _mesh_ring_rid;
		rs->instance_set_base(instance, mesh_to_use);

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

	if (!_config.is_valid() || _clipmap_levels.empty()) {
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
		xform.origin = Vector3(snapped_x, 0.0f, snapped_z);

		rs->instance_set_transform(level.instance_rid, xform);
	}
}

} //namespace ts