#include "terrain_renderer.h"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

using namespace godot;

namespace ts {

void TerrainRenderer::_bind_methods() {}

TerrainRenderer::TerrainRenderer() = default;

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

void TerrainRenderer::rebuild_mesh(const float p_size, const int p_resolution) {
	RenderingServer *rs = RenderingServer::get_singleton();
	cleanup();

	if (!_generator.is_valid() || !_config.is_valid()) {
		return;
	}

	_internal_shader_rid = rs->shader_create();
	const String shader_code = FileAccess::get_file_as_string("res://addons/terrain_server/shaders/terrain.gdshader");
	rs->shader_set_code(_internal_shader_rid, shader_code);

	const Ref<ArrayMesh> block_mesh = TerrainGenerator::create_block_mesh(p_resolution);
	_mesh_rid = rs->mesh_create();
	rs->mesh_add_surface_from_arrays(_mesh_rid, RenderingServer::PRIMITIVE_TRIANGLES, block_mesh->surface_get_arrays(0));

	const Ref<ArrayMesh> ring_mesh = TerrainGenerator::create_ring_fixup_mesh(p_resolution);
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

		rs->material_set_param(material, "height_scale", static_cast<float>(_config->get_height_scale()));
		rs->material_set_param(material, "terrain_size", _config->get_terrain_size());
		rs->material_set_param(material, "resolution", static_cast<float>(p_resolution));

		if (_config->get_noise_texture().is_valid() && _config->get_noise_texture()->get_width() > 0) {
			rs->material_set_param(material, "heightmap", _config->get_noise_texture()->get_rid());
		} else {
			rs->material_set_param(material, "heightmap", RID()); // Set to empty RID to avoid shader errors when texture is missing
		}

		const float level_scale = p_size * powf(2.0f, static_cast<float>(i));

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

void TerrainRenderer::update_camera_position(const Vector3 p_camera_pos) {
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
	const float base_cell_size = _clipmap_levels[0].scale / static_cast<float>(resolution);
	const float snapped_x = floorf(p_camera_pos.x / base_cell_size) * base_cell_size;
	const float snapped_z = floorf(p_camera_pos.z / base_cell_size) * base_cell_size;

	for (const auto & level : _clipmap_levels) {
		Transform3D xform;
		xform.basis = xform.basis.scaled(Vector3(level.scale, 1.0, level.scale));
		xform.origin = Vector3(snapped_x, 0.0f, snapped_z);

		rs->instance_set_transform(level.instance_rid, xform);
	}
}

// ============== Getters and setters ==============

void TerrainRenderer::set_generator(const Ref<TerrainGenerator> &p_generator) {
	_generator = p_generator;
}

void TerrainRenderer::set_configuration(const Ref<TerrainConfiguration> &p_config) {
	_config = p_config;
}

} //namespace ts