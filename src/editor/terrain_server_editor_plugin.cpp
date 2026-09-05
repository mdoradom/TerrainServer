#include "terrain_server_editor_plugin.h"

#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace ts {

void TerrainServerEditorPlugin::_bind_methods() {}

String TerrainServerEditorPlugin::_get_plugin_name() const {
	return "TerrainServer";
}

void TerrainServerEditorPlugin::_enter_tree() {
	UtilityFunctions::print_verbose("TerrainServer: editor plugin ready.");
}

} //namespace ts
