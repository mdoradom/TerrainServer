#pragma once
#include "terrain_configuration.h"
#include "terrain_noise.h"
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
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

	void _ensure_body_created();
	void _rebuild_heightmap(float p_center_x, float p_center_z);
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
};
} //namespace ts
