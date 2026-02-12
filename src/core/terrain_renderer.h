#pragma once

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/texture2d.hpp>

namespace ts {

class TerrainRenderer : public godot::RefCounted {
	GDCLASS(TerrainRenderer, godot::RefCounted);

private:
	godot::RID _mesh_rid;
	godot::RID _instance_rid;
	godot::RID _internal_shader_rid;
	godot::RID _internal_material_rid;
	godot::Node3D *_parent_node = nullptr;

protected:
	static void _bind_methods();

public:
	TerrainRenderer();
	~TerrainRenderer();

	void initialize(godot::Node3D *p_parent);
	void cleanup();

	void rebuild_mesh(float p_size, int p_resolution);

	void update_render_state();
	void set_user_material(const godot::Ref<godot::Material> &p_material);
	void update_shader_params(const godot::Ref<godot::Texture2D> &p_height_map, float p_scale);
};

} //namespace ts