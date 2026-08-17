#include "terrain_physics.h"

#include <algorithm>
#include <cmath>
#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <limits>

using namespace godot;

namespace ts {

namespace {
constexpr int PHYSICS_MIN_GRID_RESOLUTION = 8;
constexpr int PHYSICS_MAX_GRID_RESOLUTION = 256;
} // namespace

void TerrainPhysics::_bind_methods() {}

TerrainPhysics::TerrainPhysics() = default;

TerrainPhysics::~TerrainPhysics() {
	cleanup();
}

void TerrainPhysics::initialize(Node3D *p_parent) {
	_parent_node = p_parent;
}

void TerrainPhysics::cleanup() {
	PhysicsServer3D *ps = PhysicsServer3D::get_singleton();

	if (_shape_rid.is_valid()) {
		ps->free_rid(_shape_rid);
		_shape_rid = RID();
	}

	if (_body_rid.is_valid()) {
		ps->free_rid(_body_rid);
		_body_rid = RID();
	}

	_cleanup_debug_mesh();

	_has_built = false;
}

void TerrainPhysics::set_configuration(const Ref<TerrainConfiguration> &p_config) {
	_config = p_config;

	if (!_config.is_valid()) {
		return;
	}

	_noise_params.octaves = _config->get_noise_octaves();
	_noise_params.base_frequency = _config->get_noise_base_frequency();
	_noise_params.lacunarity = _config->get_noise_lacunarity();
	_noise_params.gain = _config->get_noise_gain();
	_noise_params.height_scale = static_cast<float>(_config->get_height_scale());

	_range = _config->get_physics_range();
	if (_range <= 0.0f) {
		_range = 64.0f;
	}

	_collision_layer = static_cast<uint32_t>(_config->get_physics_collision_layer());
	_collision_mask = static_cast<uint32_t>(_config->get_physics_collision_mask());
	if (_body_rid.is_valid()) {
		PhysicsServer3D *ps = PhysicsServer3D::get_singleton();
		ps->body_set_collision_layer(_body_rid, _collision_layer);
		ps->body_set_collision_mask(_body_rid, _collision_mask);
	}

	float terrain_size = _config->get_terrain_size();
	if (terrain_size <= 0.0f) {
		terrain_size = 256.0f;
	}
	int mesh_resolution = _config->get_mesh_resolution();
	if (mesh_resolution <= 0) {
		mesh_resolution = 64;
	}
	const float visual_cell_size = terrain_size / static_cast<float>(mesh_resolution);
	const int desired_resolution = static_cast<int>(ceilf(_range / visual_cell_size));
	_grid_resolution = std::clamp(desired_resolution, PHYSICS_MIN_GRID_RESOLUTION, PHYSICS_MAX_GRID_RESOLUTION);

	_has_built = false;
}

void TerrainPhysics::_ensure_body_created() {
	if (_body_rid.is_valid()) {
		return;
	}

	if (!_parent_node || !_parent_node->is_inside_tree()) {
		return;
	}

	PhysicsServer3D *ps = PhysicsServer3D::get_singleton();

	_body_rid = ps->body_create();
	ps->body_set_mode(_body_rid, PhysicsServer3D::BODY_MODE_STATIC);
	ps->body_set_collision_layer(_body_rid, _collision_layer);
	ps->body_set_collision_mask(_body_rid, _collision_mask);

	_shape_rid = ps->heightmap_shape_create();
	ps->body_add_shape(_body_rid, _shape_rid);

	const RID space = _parent_node->get_world_3d()->get_space();
	ps->body_set_space(_body_rid, space);
}

void TerrainPhysics::update_camera_position(const Vector3 p_camera_pos) {
	if (!_config.is_valid() || _grid_resolution <= 0 || _range <= 0.0f) {
		return;
	}

	if (!_parent_node || !_parent_node->is_inside_tree()) {
		return;
	}

	_ensure_body_created();
	if (!_body_rid.is_valid()) {
		return;
	}

	const float cell_size = _range / static_cast<float>(_grid_resolution);
	const float snapped_x = floorf(p_camera_pos.x / cell_size) * cell_size;
	const float snapped_z = floorf(p_camera_pos.z / cell_size) * cell_size;

	const float rebuild_margin = _range * 0.25f;
	const bool need_rebuild = !_has_built ||
			fabsf(snapped_x - _last_built_origin.x) >= rebuild_margin ||
			fabsf(snapped_z - _last_built_origin.y) >= rebuild_margin;

	if (need_rebuild) {
		_rebuild_heightmap(snapped_x, snapped_z);
		_last_built_origin = Vector2(snapped_x, snapped_z);
		_has_built = true;
	}
}

float TerrainPhysics::get_height_at(const Vector2 p_world_xz) const {
	if (!_config.is_valid()) {
		return 0.0f;
	}

	return TerrainNoise::get_height_at(p_world_xz, _noise_params);
}

void TerrainPhysics::_rebuild_heightmap(const float p_center_x, const float p_center_z) {
	PhysicsServer3D *ps = PhysicsServer3D::get_singleton();

	const int width = _grid_resolution + 1;
	const int depth = width;
	const float cell_size = _range / static_cast<float>(_grid_resolution);
	const float half_res = static_cast<float>(_grid_resolution) * 0.5f;

	PackedFloat32Array heights;
	heights.resize(width * depth);
	float *w = heights.ptrw();

	float min_h = std::numeric_limits<float>::max();
	float max_h = -std::numeric_limits<float>::max();

	for (int row = 0; row < depth; row++) {
		for (int col = 0; col < width; col++) {
			const float local_x = static_cast<float>(col) - half_res;
			const float local_z = static_cast<float>(row) - half_res;
			const float world_x = p_center_x + local_x * cell_size;
			const float world_z = p_center_z + local_z * cell_size;

			const float h = TerrainNoise::get_height_at(Vector2(world_x, world_z), _noise_params);
			w[row * width + col] = h;
			min_h = std::min(min_h, h);
			max_h = std::max(max_h, h);
		}
	}

	Dictionary shape_data;
	shape_data["width"] = width;
	shape_data["depth"] = depth;
	shape_data["heights"] = heights;
	shape_data["min_height"] = min_h;
	shape_data["max_height"] = max_h;
	ps->shape_set_data(_shape_rid, shape_data);

	Transform3D xform;
	xform.basis = xform.basis.scaled(Vector3(cell_size, 1.0f, cell_size));
	xform.origin = Vector3(p_center_x, 0.0f, p_center_z);
	ps->body_set_state(_body_rid, PhysicsServer3D::BODY_STATE_TRANSFORM, xform);

	_update_debug_mesh(heights, width, depth, cell_size, p_center_x, p_center_z);
}

void TerrainPhysics::_update_debug_mesh(const PackedFloat32Array &p_heights, const int p_width, const int p_depth, const float p_cell_size, const float p_center_x, const float p_center_z) {
	if (!_parent_node || !_parent_node->get_tree() || !_parent_node->get_tree()->is_debugging_collisions_hint()) {
		_cleanup_debug_mesh();
		return;
	}

	RenderingServer *rs = RenderingServer::get_singleton();

	const float half_res = static_cast<float>(p_width - 1) * 0.5f;
	PackedVector3Array lines;
	lines.resize(((p_width - 1) * p_depth + p_width * (p_depth - 1)) * 2);
	Vector3 *w = lines.ptrw();
	int idx = 0;

	for (int row = 0; row < p_depth; row++) {
		for (int col = 0; col < p_width - 1; col++) {
			w[idx++] = Vector3(static_cast<float>(col) - half_res, p_heights[row * p_width + col], static_cast<float>(row) - half_res);
			w[idx++] = Vector3(static_cast<float>(col + 1) - half_res, p_heights[row * p_width + col + 1], static_cast<float>(row) - half_res);
		}
	}
	for (int col = 0; col < p_width; col++) {
		for (int row = 0; row < p_depth - 1; row++) {
			w[idx++] = Vector3(static_cast<float>(col) - half_res, p_heights[row * p_width + col], static_cast<float>(row) - half_res);
			w[idx++] = Vector3(static_cast<float>(col) - half_res, p_heights[(row + 1) * p_width + col], static_cast<float>(row + 1) - half_res);
		}
	}

	if (_debug_mesh_rid.is_valid()) {
		rs->free_rid(_debug_mesh_rid);
	}
	_debug_mesh_rid = rs->mesh_create();

	Array arrays;
	arrays.resize(RenderingServer::ARRAY_MAX);
	arrays[RenderingServer::ARRAY_VERTEX] = lines;
	rs->mesh_add_surface_from_arrays(_debug_mesh_rid, RenderingServer::PRIMITIVE_LINES, arrays);

	if (!_debug_shader_rid.is_valid()) {
		_debug_shader_rid = rs->shader_create();
		rs->shader_set_code(_debug_shader_rid,
				"shader_type spatial;\n"
				"render_mode unshaded, cull_disabled, depth_draw_always;\n"
				"void fragment() { ALBEDO = vec3(0.15, 1.0, 0.3); }\n");
	}

	if (!_debug_material_rid.is_valid()) {
		_debug_material_rid = rs->material_create();
		rs->material_set_shader(_debug_material_rid, _debug_shader_rid);
	}

	if (!_debug_instance_rid.is_valid()) {
		_debug_instance_rid = rs->instance_create();
		rs->instance_geometry_set_material_override(_debug_instance_rid, _debug_material_rid);
	}

	rs->instance_set_base(_debug_instance_rid, _debug_mesh_rid);

	if (_parent_node->is_inside_tree()) {
		rs->instance_set_scenario(_debug_instance_rid, _parent_node->get_world_3d()->get_scenario());
	}

	Transform3D xform;
	xform.basis = xform.basis.scaled(Vector3(p_cell_size, 1.0f, p_cell_size));
	xform.origin = Vector3(p_center_x, 0.0f, p_center_z);
	rs->instance_set_transform(_debug_instance_rid, xform);
}

void TerrainPhysics::_cleanup_debug_mesh() {
	RenderingServer *rs = RenderingServer::get_singleton();

	if (_debug_instance_rid.is_valid()) {
		rs->free_rid(_debug_instance_rid);
		_debug_instance_rid = RID();
	}
	if (_debug_mesh_rid.is_valid()) {
		rs->free_rid(_debug_mesh_rid);
		_debug_mesh_rid = RID();
	}
	if (_debug_material_rid.is_valid()) {
		rs->free_rid(_debug_material_rid);
		_debug_material_rid = RID();
	}
	if (_debug_shader_rid.is_valid()) {
		rs->free_rid(_debug_shader_rid);
		_debug_shader_rid = RID();
	}
}

} // namespace ts
