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
		if (level.material_rid.is_valid()) {
			rs->free_rid(level.material_rid);
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
		render_mode cull_back;

		uniform float height_scale = 100.0;
		uniform vec3 camera_position;
		uniform float morph_range = 0.75;
		uniform float grid_scale = 1.0;
		uniform int level_index = 0;
		uniform sampler2D heightmap : repeat_enable;
		uniform float terrain_size = 1024.0;

		varying vec3 v_normal;

		float get_height_at(vec2 world_xz) {
			// Convert world coordinates to UV [0, 1]
			vec2 uv = world_xz / terrain_size;
			// Sample the heightmap and scale by height_scale
			float h = texture(heightmap, uv).r;
			return (h * 2.0 - 1.0) * height_scale; // Remap from [0,1] to [-1,1]
		}

		void vertex() {
			// Position of the vertex in world space
			vec3 world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
			vec2 world_xz = vec2(world_pos.x, world_pos.z);

			// Get height at current position
			float h_current = get_height_at(world_xz);

			// Calculate distance to camera
			vec2 cam_xz = vec2(camera_position.x, camera_position.z);
			float dist_to_camera = max(abs(world_xz.x - cam_xz.x), abs(world_xz.y - cam_xz.y));

			// Morphing
			float morph_start = grid_scale * 0.5 * morph_range;
			float morph_end = grid_scale * 0.5;
			float morph_factor = clamp((dist_to_camera - morph_start) / (morph_end - morph_start), 0.0, 1.0);

			// Calculate position on the coarser grid (next clipmap level)
			float coarser_grid = grid_scale * 2.0;
			vec2 coarser_pos = floor(world_xz / coarser_grid) * coarser_grid + coarser_grid * 0.5;
			float h_coarser = get_height_at(coarser_pos);

			// Interpolate between the current height and the coarser level height
			float final_height = mix(h_current, h_coarser, morph_factor);

			// Apply the final height to the vertex position
			vec3 final_world = vec3(world_pos.x, final_height, world_pos.z);
			VERTEX = (inverse(MODEL_MATRIX) * vec4(final_world, 1.0)).xyz;

			// Calculate normals using finite differences
			float epsilon = grid_scale * 0.1;
			vec3 n;
			n.x = get_height_at(world_xz + vec2(-epsilon, 0.0)) - get_height_at(world_xz + vec2(epsilon, 0.0));
			n.z = get_height_at(world_xz + vec2(0.0, -epsilon)) - get_height_at(world_xz + vec2(0.0, epsilon));
			n.y = 2.0 * epsilon;
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

	Ref<ArrayMesh> block_mesh = _generator->create_block_mesh(p_resolution);
	_mesh_rid = rs->mesh_create();
	rs->mesh_add_surface_from_arrays(_mesh_rid, RenderingServer::PRIMITIVE_TRIANGLES, block_mesh->surface_get_arrays(0));

	Ref<ArrayMesh> ring_mesh = _generator->create_ring_fixup_mesh(p_resolution);
	_mesh_ring_rid = rs->mesh_create();
	rs->mesh_add_surface_from_arrays(_mesh_ring_rid, RenderingServer::PRIMITIVE_TRIANGLES, ring_mesh->surface_get_arrays(0));

	int num_levels = _config->get_clipmap_levels();
	if (num_levels <= 0) {
		num_levels = 6;
	}

	for (int i = 0; i < num_levels; i++) {
		RID instance = rs->instance_create();

		RID mesh_to_use = (i == 0) ? _mesh_rid : _mesh_ring_rid;
		rs->instance_set_base(instance, mesh_to_use);

		RID material = rs->material_create();
		rs->material_set_shader(material, _internal_shader_rid);

		rs->material_set_param(material, "height_scale", (float)_config->get_height_scale());
		rs->material_set_param(material, "morph_range", _config->get_morph_range());
		rs->material_set_param(material, "terrain_size", _config->get_terrain_size());

		if (_config->get_noise_texture().is_valid()) {
			rs->material_set_param(material, "heightmap", _config->get_noise_texture()->get_rid());
		}

		float level_scale = p_size * powf(2.0f, (float)i);

		rs->material_set_param(material, "grid_scale", level_scale);
		rs->material_set_param(material, "level_index", i);

		rs->instance_geometry_set_material_override(instance, material);

		if (_parent_node && _parent_node->is_inside_tree()) {
			rs->instance_set_scenario(instance, _parent_node->get_world_3d()->get_scenario());
		}

		ClipmapLevel level;
		level.instance_rid = instance;
		level.material_rid = material;
		level.scale = level_scale;

		_clipmap_levels.push_back(level);
	}
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

		if (level.material_rid.is_valid()) {
			rs->material_set_param(level.material_rid, "camera_position", p_camera_pos);
		}

		float cell_size = level.scale / (float)resolution;

		float snapped_x = floorf(p_camera_pos.x / cell_size) * cell_size;
		float snapped_z = floorf(p_camera_pos.z / cell_size) * cell_size;

		Transform3D xform;
		xform.basis = xform.basis.scaled(Vector3(level.scale, 1.0, level.scale));
		xform.origin = Vector3(snapped_x, 0.0f, snapped_z);

		rs->instance_set_transform(level.instance_rid, xform);
	}
}

} //namespace ts