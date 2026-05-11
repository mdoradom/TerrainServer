#pragma once
#include "terrain_configuration.h"
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
namespace ts {
class TerrainPhysics : public godot::RefCounted {
	GDCLASS(TerrainPhysics, godot::RefCounted);

protected:
	static void _bind_methods();

public:
	TerrainPhysics();
	~TerrainPhysics();
	void initialize(godot::Node3D *p_parent);
	void cleanup();
	void set_configuration(const godot::Ref<TerrainConfiguration> &p_config);
	void update_camera_position(godot::Vector3 p_camera_pos);
};
} //namespace ts
