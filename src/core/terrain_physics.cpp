#include "terrain_physics.h"

#include <algorithm>
#include <cmath>
#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <limits>

using namespace godot;

namespace ts {

constexpr int PHYSICS_MIN_GRID_RESOLUTION = 8;
constexpr int PHYSICS_MAX_GRID_RESOLUTION = 256;

// Mirrors WHITTAKER_TEMP_SOFTNESS_MIN / WHITTAKER_MOIST_SOFTNESS_MIN in terrain.gdshader fragment().
constexpr float WHITTAKER_TEMP_SOFTNESS_MIN = 0.5f; // deg C
constexpr float WHITTAKER_MOIST_SOFTNESS_MIN = 0.02f; // normalized [0,1]

void TerrainPhysics::_bind_methods() {}

TerrainPhysics::TerrainPhysics() = default;

TerrainPhysics::~TerrainPhysics() {
	cleanup();
}

void TerrainPhysics::initialize(Node3D *p_parent) {
	_parent_node = p_parent;
}

void TerrainPhysics::cleanup() {
	if (_rebuild_in_flight) {
		WorkerThreadPool::get_singleton()->wait_for_task_completion(_rebuild_task_id);
		_rebuild_in_flight = false;
	}

	PhysicsServer3D *ps = PhysicsServer3D::get_singleton();

	if (_shape_rid.is_valid()) {
		ps->free_rid(_shape_rid);
		_shape_rid = RID();
	}

	if (_body_rid.is_valid()) {
		ps->free_rid(_body_rid);
		_body_rid = RID();
	}

	_has_built = false;
}

void TerrainPhysics::set_configuration(const Ref<TerrainConfiguration> &p_config) {
	_config = p_config;

	if (!_config.is_valid()) {
		return;
	}

	const TerrainNoise::FbmParams previous_noise_params = _noise_params;
	const float previous_range = _range;
	const int previous_grid_resolution = _grid_resolution;

	_noise_params.octaves = _config->get_noise_octaves();
	_noise_params.base_frequency = _config->get_noise_base_frequency();
	_noise_params.lacunarity = _config->get_noise_lacunarity();
	_noise_params.gain = _config->get_noise_gain();
	_noise_params.height_scale = static_cast<float>(_config->get_height_scale());

	_temp_moist_params.temperature_frequency = _config->get_temperature_frequency();
	_temp_moist_params.temperature_offset = _config->get_temperature_offset();
	_temp_moist_params.temperature_noise_influence = _config->get_temperature_noise_influence();
	_temp_moist_params.temperature_altitude_reference = _config->get_temperature_altitude_reference();
	_temp_moist_params.moisture_frequency = _config->get_moisture_frequency();
	_temp_moist_params.moisture_offset = _config->get_moisture_offset();

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

	const bool heightmap_params_changed = _grid_resolution != previous_grid_resolution ||
			_range != previous_range ||
			_noise_params.octaves != previous_noise_params.octaves ||
			_noise_params.base_frequency != previous_noise_params.base_frequency ||
			_noise_params.lacunarity != previous_noise_params.lacunarity ||
			_noise_params.gain != previous_noise_params.gain ||
			_noise_params.height_scale != previous_noise_params.height_scale;

	if (heightmap_params_changed) {
		_has_built = false;
	}
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

void TerrainPhysics::update_focus_position(const Vector3 p_focus_pos) {
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

	if (_rebuild_in_flight) {
		if (!WorkerThreadPool::get_singleton()->is_task_completed(_rebuild_task_id)) {
			return;
		}

		WorkerThreadPool::get_singleton()->wait_for_task_completion(_rebuild_task_id);
		_rebuild_in_flight = false;
		_apply_heightmap();
		_last_built_origin = Vector2(_job_center_x, _job_center_z);
		_has_built = true;
	}

	const float cell_size = _range / static_cast<float>(_grid_resolution);
	const float snapped_x = floorf(p_focus_pos.x / cell_size) * cell_size;
	const float snapped_z = floorf(p_focus_pos.z / cell_size) * cell_size;

	const float rebuild_margin = _range * 0.25f;
	const bool need_rebuild = !_has_built ||
			fabsf(snapped_x - _last_built_origin.x) >= rebuild_margin ||
			fabsf(snapped_z - _last_built_origin.y) >= rebuild_margin;

	if (need_rebuild) {
		_start_heightmap_rebuild(snapped_x, snapped_z);
	}
}

float TerrainPhysics::get_height_at(const Vector2 p_world_xz) const {
	if (!_config.is_valid()) {
		return 0.0f;
	}

	return TerrainNoise::get_height_at(p_world_xz, _noise_params);
}

Ref<TerrainBiomeLayer> TerrainPhysics::get_biome_at(const Vector2 p_world_xz) const {
	if (!_config.is_valid()) {
		return nullptr;
	}

	const TypedArray<TerrainBiomeLayer> layers = _config->get_biome_layers();
	if (layers.is_empty()) {
		return nullptr;
	}

	const float height = TerrainNoise::get_height_at(p_world_xz, _noise_params);
	const float temperature = TerrainNoise::temperature_at(p_world_xz, height, _temp_moist_params);
	const float moisture = TerrainNoise::moisture_at(p_world_xz, _temp_moist_params);

	Ref<TerrainBiomeLayer> best_layer;
	float best_weight = -1.0f;

	for (const auto &i : layers) {
		const Ref<TerrainBiomeLayer> layer = i;
		if (!layer.is_valid()) {
			continue;
		}

		const float min_t = layer->get_min_temperature();
		const float max_t = layer->get_max_temperature();
		const float min_m = layer->get_min_moisture();
		const float max_m = layer->get_max_moisture();

		const float softness_t = std::max(WHITTAKER_TEMP_SOFTNESS_MIN, (max_t - min_t) * 0.25f);
		const float softness_m = std::max(WHITTAKER_MOIST_SOFTNESS_MIN, (max_m - min_m) * 0.25f);

		const float wt = Math::smoothstep(min_t - softness_t, min_t, temperature) *
				(1.0f - Math::smoothstep(max_t, max_t + softness_t, temperature));
		const float wm = Math::smoothstep(min_m - softness_m, min_m, moisture) *
				(1.0f - Math::smoothstep(max_m, max_m + softness_m, moisture));

		if (const float weight = wt * wm; weight > best_weight) {
			best_weight = weight;
			best_layer = layer;
		}
	}

	return best_layer;
}

void TerrainPhysics::_start_heightmap_rebuild(const float p_center_x, const float p_center_z) {
	_job_center_x = p_center_x;
	_job_center_z = p_center_z;
	_job_grid_resolution = _grid_resolution;
	_job_range = _range;
	_job_noise_params = _noise_params;

	_rebuild_task_id = WorkerThreadPool::get_singleton()->add_task(callable_mp(this, &TerrainPhysics::_compute_heightmap_task), false, "TerrainHeightmapRebuild");
	_rebuild_in_flight = true;
}

void TerrainPhysics::_compute_heightmap_task() {
	const int width = _job_grid_resolution + 1;
	const int depth = width;
	const float cell_size = _job_range / static_cast<float>(_job_grid_resolution);
	const float half_res = static_cast<float>(_job_grid_resolution) * 0.5f;

	PackedFloat32Array heights;
	heights.resize(width * depth);
	float *w = heights.ptrw();

	float min_h = std::numeric_limits<float>::max();
	float max_h = -std::numeric_limits<float>::max();

	for (int row = 0; row < depth; row++) {
		for (int col = 0; col < width; col++) {
			const float local_x = static_cast<float>(col) - half_res;
			const float local_z = static_cast<float>(row) - half_res;
			const float world_x = _job_center_x + local_x * cell_size;
			const float world_z = _job_center_z + local_z * cell_size;

			const float h = TerrainNoise::get_height_at(Vector2(world_x, world_z), _job_noise_params);
			w[row * width + col] = h;
			min_h = std::min(min_h, h);
			max_h = std::max(max_h, h);
		}
	}

	_job_heights = heights;
	_job_min_h = min_h;
	_job_max_h = max_h;
}

void TerrainPhysics::_apply_heightmap() {
	PhysicsServer3D *ps = PhysicsServer3D::get_singleton();

	const int width = _job_grid_resolution + 1;
	const int depth = width;
	const float cell_size = _job_range / static_cast<float>(_job_grid_resolution);

	Dictionary shape_data;
	shape_data["width"] = width;
	shape_data["depth"] = depth;
	shape_data["heights"] = _job_heights;
	shape_data["min_height"] = _job_min_h;
	shape_data["max_height"] = _job_max_h;
	ps->shape_set_data(_shape_rid, shape_data);

	Transform3D xform;
	xform.basis = xform.basis.scaled(Vector3(cell_size, 1.0f, cell_size));
	xform.origin = Vector3(_job_center_x, 0.0f, _job_center_z);
	ps->body_set_state(_body_rid, PhysicsServer3D::BODY_STATE_TRANSFORM, xform);
}

} // namespace ts
