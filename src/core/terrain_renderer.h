#pragma once

#include "core/terrain_generator.h"

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/ref_counted.hpp>

namespace ts {

class TerrainRenderer : public godot::RefCounted {
	GDCLASS(TerrainRenderer, godot::RefCounted);

private:
	godot::MeshInstance3D *_mesh_instance;
	godot::Ref<godot::Material> _material_override;
	godot::Ref<TerrainGenerator> _generator;
	godot::Node3D *_parent_node;
	int _mesh_resolution;

protected:
	static void _bind_methods();

public:
	TerrainRenderer();
	~TerrainRenderer();

	void initialize(godot::Node3D *p_parent);
	void cleanup();

	void set_generator(const godot::Ref<TerrainGenerator> &p_generator);
	void set_mesh_resolution(int p_resolution);
	void set_material_override(const godot::Ref<godot::Material> &p_material);

	void generate_mesh();
	void update_mesh();
};

} //namespace ts