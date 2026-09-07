#pragma once

#include <godot_cpp/classes/editor_node3d_gizmo.hpp>
#include <godot_cpp/classes/editor_node3d_gizmo_plugin.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/string.hpp>

#include <vector>

namespace ts {

// The nested clipmap level footprints.
class TerrainClipmapGizmoPlugin : public godot::EditorNode3DGizmoPlugin {
	GDCLASS(TerrainClipmapGizmoPlugin, godot::EditorNode3DGizmoPlugin);

private:
	std::vector<godot::Ref<godot::StandardMaterial3D>> _level_materials;

protected:
	static void _bind_methods();

public:
	TerrainClipmapGizmoPlugin();

	bool _has_gizmo(godot::Node3D *p_for_node_3d) const override;
	godot::String _get_gizmo_name() const override;
	void _redraw(const godot::Ref<godot::EditorNode3DGizmo> &p_gizmo) override;
};

// The applied heightmap volume and the margin that triggers the next regrid.
class TerrainCollisionGizmoPlugin : public godot::EditorNode3DGizmoPlugin {
	GDCLASS(TerrainCollisionGizmoPlugin, godot::EditorNode3DGizmoPlugin);

private:
	godot::Ref<godot::StandardMaterial3D> _volume_material;
	godot::Ref<godot::StandardMaterial3D> _margin_material;
	godot::Ref<godot::StandardMaterial3D> _pending_material;

protected:
	static void _bind_methods();

public:
	TerrainCollisionGizmoPlugin();

	bool _has_gizmo(godot::Node3D *p_for_node_3d) const override;
	godot::String _get_gizmo_name() const override;
	void _redraw(const godot::Ref<godot::EditorNode3DGizmo> &p_gizmo) override;
};

// The raw focus position marker.
class TerrainFocusGizmoPlugin : public godot::EditorNode3DGizmoPlugin {
	GDCLASS(TerrainFocusGizmoPlugin, godot::EditorNode3DGizmoPlugin);

private:
	godot::Ref<godot::StandardMaterial3D> _raw_material;

protected:
	static void _bind_methods();

public:
	TerrainFocusGizmoPlugin();

	bool _has_gizmo(godot::Node3D *p_for_node_3d) const override;
	godot::String _get_gizmo_name() const override;
	void _redraw(const godot::Ref<godot::EditorNode3DGizmo> &p_gizmo) override;
};

} //namespace ts
