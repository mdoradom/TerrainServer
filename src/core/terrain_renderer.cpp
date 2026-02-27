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
		uniform sampler2D heightmap : filter_linear, repeat_enable;
		uniform float terrain_size = 1024.0;
		uniform float resolution = 64.0;

		varying vec3 v_normal;

		float get_height_at(vec2 world_xz) {
			vec2 uv = world_xz / terrain_size;
			float h = texture(heightmap, uv).r;
			return (h * 2.0 - 1.0) * height_scale;
		}

		void vertex() {
			// 1. Local x/z morphing
			vec2 local_xz = VERTEX.xz;
			float max_abs = max(abs(local_xz.x), abs(local_xz.y));

			// Detect if we are in the outermost 20% of the ring
			float morph_factor = clamp((max_abs - 0.4) / 0.1, 0.0, 1.0);
			morph_factor = smoothstep(0.0, 1.0, morph_factor);

			// Calculate where the upper LOD grid is
			float coarser_step = 2.0 / resolution;
			vec2 morphed_local_xz = round(local_xz / coarser_step) * coarser_step;

			// Slide the vertex horizontally towards the other grid
			VERTEX.xz = mix(local_xz, morphed_local_xz, morph_factor);

			// 2. Height in world space
			vec3 world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
			vec2 world_xz = vec2(world_pos.x, world_pos.z);

			// Read the exact height where the morphed vertex now lies
			float final_height = get_height_at(world_xz);

			// Apply and return to local Transform
			vec3 final_world = vec3(world_pos.x, final_height, world_pos.z);
			VERTEX = (inverse(MODEL_MATRIX) * vec4(final_world, 1.0)).xyz;

			// 3. Analytical normals
			float e = 1.0;
			vec3 n;
			n.x = get_height_at(world_xz + vec2(-e, 0.0)) - get_height_at(world_xz + vec2(e, 0.0));
			n.z = get_height_at(world_xz + vec2(0.0, -e)) - get_height_at(world_xz + vec2(0.0, e));
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
		rs->material_set_param(material, "terrain_size", _config->get_terrain_size());
		rs->material_set_param(material, "resolution", (float)p_resolution);

		if (_config->get_noise_texture().is_valid()) {
			rs->material_set_param(material, "heightmap", _config->get_noise_texture()->get_rid());
		}

		float level_scale = p_size * powf(2.0f, (float)i);

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

	// Shared snapping
	// We use the finest grid's resolution to snap ALL levels in unison.
	// This prevents the rings from sliding and misaligning.
	float base_cell_size = _clipmap_levels[0].scale / (float)resolution;
	float snapped_x = floorf(p_camera_pos.x / base_cell_size) * base_cell_size;
	float snapped_z = floorf(p_camera_pos.z / base_cell_size) * base_cell_size;

	for (size_t i = 0; i < _clipmap_levels.size(); i++) {
		const auto &level = _clipmap_levels[i];

		Transform3D xform;
		xform.basis = xform.basis.scaled(Vector3(level.scale, 1.0, level.scale));
		xform.origin = Vector3(snapped_x, 0.0f, snapped_z);

		rs->instance_set_transform(level.instance_rid, xform);
	}
}

} //namespace ts