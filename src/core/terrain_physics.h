#pragma once
#include "terrain_configuration.h"
#include "terrain_noise.h"
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/vector2.hpp>
namespace ts {
class TerrainPhysics : public godot::RefCounted {
	GDCLASS(TerrainPhysics, godot::RefCounted);

private:
	godot::Node3D *_parent_node = nullptr;
	godot::Ref<TerrainConfiguration> _config;

	godot::RID _body_rid;
	godot::RID _shape_rid;

	godot::RID _debug_mesh_rid;
	godot::RID _debug_material_rid;
	godot::RID _debug_shader_rid;
	godot::RID _debug_instance_rid;

	TerrainNoise::FbmParams _noise_params;
	int _grid_resolution = 0;
	float _range = 0.0f;
	uint32_t _collision_layer = 1;
	uint32_t _collision_mask = 0;

	bool _has_built = false;
	godot::Vector2 _last_built_origin;

	bool _rebuild_in_flight = false;
	godot::WorkerThreadPool::TaskID _rebuild_task_id = godot::WorkerThreadPool::INVALID_TASK_ID;

	float _job_center_x = 0.0f;
	float _job_center_z = 0.0f;
	int _job_grid_resolution = 0;
	float _job_range = 0.0f;
	TerrainNoise::FbmParams _job_noise_params;

	godot::PackedFloat32Array _job_heights;
	float _job_min_h = 0.0f;
	float _job_max_h = 0.0f;

	void _ensure_body_created();
	void _start_heightmap_rebuild(float p_center_x, float p_center_z);
	void _compute_heightmap_task();
	void _apply_heightmap();
	void _update_debug_mesh(const godot::PackedFloat32Array &p_heights, int p_width, int p_depth, float p_cell_size, float p_center_x, float p_center_z);
	void _cleanup_debug_mesh();

protected:
	static void _bind_methods();

public:
	TerrainPhysics();
	~TerrainPhysics() override;
	void initialize(godot::Node3D *p_parent);
	void cleanup();
	void set_configuration(const godot::Ref<TerrainConfiguration> &p_config);
	void update_camera_position(godot::Vector3 p_camera_pos);
	float get_height_at(godot::Vector2 p_world_xz) const;
};
} //namespace ts
