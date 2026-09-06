#include "terrain_server_editor_plugin.h"

#include "nodes/terrain3d.h"
#include "terrain_gizmo_plugins.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace ts {

constexpr const char *RELOAD_SHADER_MENU_ITEM = "Reload Terrain Shader";
constexpr const char *REBUILD_MENU_ITEM = "Rebuild Terrain";

namespace {

void collect_terrains(Node *p_node, std::vector<Terrain3D *> &r_terrains) {
	if (Terrain3D *terrain = Object::cast_to<Terrain3D>(p_node)) {
		r_terrains.push_back(terrain);
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		collect_terrains(p_node->get_child(i), r_terrains);
	}
}

} //namespace

void TerrainServerEditorPlugin::_bind_methods() {}

TerrainServerEditorPlugin::TerrainServerEditorPlugin() {
	set_process(true);
}

String TerrainServerEditorPlugin::_get_plugin_name() const {
	return "TerrainServer";
}

void TerrainServerEditorPlugin::_enter_tree() {
	add_tool_menu_item(RELOAD_SHADER_MENU_ITEM, callable_mp(this, &TerrainServerEditorPlugin::_on_reload_shader_pressed));
	add_tool_menu_item(REBUILD_MENU_ITEM, callable_mp(this, &TerrainServerEditorPlugin::_on_rebuild_pressed));

	// Each of these have its own View > Gizmos entry, so the overlays toggle independently.
	_gizmo_plugins.emplace_back(Ref(memnew(TerrainClipmapGizmoPlugin)));
	_gizmo_plugins.emplace_back(Ref(memnew(TerrainCollisionGizmoPlugin)));
	_gizmo_plugins.emplace_back(Ref(memnew(TerrainFocusGizmoPlugin)));
	_gizmo_plugins.emplace_back(Ref(memnew(TerrainMorphGizmoPlugin)));

	for (const Ref<EditorNode3DGizmoPlugin> &gizmo_plugin : _gizmo_plugins) {
		add_node_3d_gizmo_plugin(gizmo_plugin);
	}
	_attach_gizmos_to_open_scene();

	UtilityFunctions::print_verbose("TerrainServer: editor plugin ready.");
}

void TerrainServerEditorPlugin::_exit_tree() {
	remove_tool_menu_item(RELOAD_SHADER_MENU_ITEM);
	remove_tool_menu_item(REBUILD_MENU_ITEM);

	for (const Ref<EditorNode3DGizmoPlugin> &gizmo_plugin : _gizmo_plugins) {
		remove_node_3d_gizmo_plugin(gizmo_plugin);
	}
	_gizmo_plugins.clear();

	for (Terrain3D *terrain : Terrain3D::get_editor_instances()) {
		terrain->clear_editor_focus_override();
	}
}

void TerrainServerEditorPlugin::_process(double delta) {
	const std::vector<Terrain3D *> &terrains = Terrain3D::get_editor_instances();
	if (terrains.empty()) {
		return;
	}

	EditorInterface *editor_interface = EditorInterface::get_singleton();
	if (editor_interface == nullptr) {
		return;
	}

	SubViewport *viewport = editor_interface->get_editor_viewport_3d(0);
	Camera3D *camera = viewport != nullptr ? viewport->get_camera_3d() : nullptr;
	if (camera == nullptr) {
		return;
	}

	const Vector3 camera_position = camera->get_global_position();
	for (Terrain3D *terrain : terrains) {
		terrain->set_editor_focus_override(camera_position);
		terrain->update_gizmos();
	}
}

void TerrainServerEditorPlugin::_attach_gizmos_to_open_scene() {
	const EditorInterface *editor_interface = EditorInterface::get_singleton();
	Node *root = editor_interface != nullptr ? editor_interface->get_edited_scene_root() : nullptr;
	if (root == nullptr) {
		return;
	}

	std::vector<Terrain3D *> terrains;
	collect_terrains(root, terrains);

	for (Terrain3D *terrain : terrains) {
		terrain->clear_gizmos();
		terrain->update_gizmos();
	}
}

void TerrainServerEditorPlugin::_on_reload_shader_pressed() {
	for (Terrain3D *terrain : Terrain3D::get_editor_instances()) {
		terrain->reload_shader();
	}
}

void TerrainServerEditorPlugin::_on_rebuild_pressed() {
	for (Terrain3D *terrain : Terrain3D::get_editor_instances()) {
		terrain->rebuild();
	}
}

} //namespace ts
