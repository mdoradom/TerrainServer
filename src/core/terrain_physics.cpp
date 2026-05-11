#include "terrain_physics.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace ts {

	void TerrainPhysics::_bind_methods() {}

	TerrainPhysics::TerrainPhysics() {}
	TerrainPhysics::~TerrainPhysics() {}
	void TerrainPhysics::initialize(Node3D *p_parent) {}
	void TerrainPhysics::cleanup() {}
	void TerrainPhysics::set_configuration(const Ref<TerrainConfiguration> &p_config) {}
	void TerrainPhysics::update_camera_position(Vector3 p_camera_pos) {}

} // namespace ts
