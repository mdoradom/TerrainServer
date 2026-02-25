#pragma once

#include "terrain_configuration.h"
#include "terrain_generator.h"

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <vector>

namespace ts {

class TerrainRenderer : public godot::RefCounted {
	GDCLASS(TerrainRenderer, godot::RefCounted);

private:
	godot::RID _mesh_rid;
	godot::RID _mesh_ring_rid;
	godot::RID _internal_shader_rid;
	godot::Node3D *_parent_node = nullptr;

	godot::Ref<TerrainGenerator> _generator;
	godot::Ref<TerrainConfiguration> _config;

	struct ClipmapLevel {
		godot::RID instance_rid;
		godot::RID material_rid;
		float scale;
	};

	std::vector<ClipmapLevel> _clipmap_levels;

protected:
	static void _bind_methods();

public:
	TerrainRenderer();
	~TerrainRenderer();

	void initialize(godot::Node3D *p_parent);
	void cleanup();

	void set_generator(const godot::Ref<TerrainGenerator> &p_generator);
	void set_configuration(const godot::Ref<TerrainConfiguration> &p_config);

	void rebuild_mesh(float p_size, int p_resolution);
	void update_camera_position(godot::Vector3 p_camera_pos);
};

} //namespace ts