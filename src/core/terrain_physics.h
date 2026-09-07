#pragma once
#include "terrain_biome_layer.h"
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

	TerrainNoise::FbmParams _noise_params;
	TerrainNoise::TemperatureMoistureParams _temp_moist_params;
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

	// Snapshot of what _apply_heightmap() actually pushed to PhysicsServer3D. The _job_* values
	// above belong to a possibly in-flight rebuild, and _range / _grid_resolution follow the
	// configuration the instant it is edited, so neither describes the collision surface that
	// currently exists.
	float _built_range = 0.0f;
	int _built_grid_resolution = 0;
	float _built_min_h = 0.0f;
	float _built_max_h = 0.0f;

	void _ensure_body_created();
	void _start_heightmap_rebuild(float p_center_x, float p_center_z);
	void _compute_heightmap_task();
	void _apply_heightmap();

protected:
	static void _bind_methods();

public:
	static constexpr float REBUILD_MARGIN_FRACTION = 0.25f;

	TerrainPhysics();
	~TerrainPhysics() override;
	void initialize(godot::Node3D *p_parent);
	void cleanup();
	void set_configuration(const godot::Ref<TerrainConfiguration> &p_config);
	void update_focus_position(godot::Vector3 p_focus_pos);
	float get_height_at(godot::Vector2 p_world_xz) const;
	godot::Ref<TerrainBiomeLayer> get_biome_at(godot::Vector2 p_world_xz) const;
	float temperature_at(godot::Vector2 p_world_xz) const;
	float moisture_at(godot::Vector2 p_world_xz) const;

	float get_range() const;
	int get_grid_resolution() const;
	float get_built_range() const;
	int get_built_grid_resolution() const;
	godot::Vector2 get_built_height_range() const;
	godot::Vector2 get_last_built_origin() const;
	bool is_built() const;
	bool is_rebuild_in_flight() const;
};
} //namespace ts
