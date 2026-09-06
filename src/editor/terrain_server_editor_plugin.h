#pragma once

#include <godot_cpp/classes/editor_node3d_gizmo_plugin.hpp>
#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/variant/string.hpp>
#include <vector>

namespace ts {

class Terrain3D;

class TerrainServerEditorPlugin : public godot::EditorPlugin {
	GDCLASS(TerrainServerEditorPlugin, godot::EditorPlugin);

private:
	std::vector<godot::Ref<godot::EditorNode3DGizmoPlugin>> _gizmo_plugins;
	void _attach_gizmos_to_open_scene();

	void _on_reload_shader_pressed();
	void _on_rebuild_pressed();

protected:
	static void _bind_methods();

public:
	TerrainServerEditorPlugin();

	godot::String _get_plugin_name() const override;

	void _enter_tree() override;
	void _exit_tree() override;
	void _process(double delta) override;
};

} //namespace ts
