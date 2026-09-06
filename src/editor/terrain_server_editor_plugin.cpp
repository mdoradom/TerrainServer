#include "terrain_server_editor_plugin.h"

#include "nodes/terrain3d.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace ts {

void TerrainServerEditorPlugin::_bind_methods() {}

TerrainServerEditorPlugin::TerrainServerEditorPlugin() {
	set_process(true);
}

String TerrainServerEditorPlugin::_get_plugin_name() const {
	return "TerrainServer";
}

void TerrainServerEditorPlugin::_enter_tree() {
	UtilityFunctions::print_verbose("TerrainServer: editor plugin ready.");
}

void TerrainServerEditorPlugin::_exit_tree() {
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
	}
}

} //namespace ts
