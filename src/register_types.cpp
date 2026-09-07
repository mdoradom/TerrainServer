#include "register_types.h"

#include "core/terrain_biome_layer.h"
#include "core/terrain_configuration.h"
#include "core/terrain_generator.h"
#include "core/terrain_physics.h"
#include "core/terrain_slope_layer.h"
#include "editor/terrain_dock.h"
#include "editor/terrain_gizmo_plugins.h"
#include "editor/terrain_server_editor_plugin.h"
#include "editor/whittaker_chart.h"
#include "nodes/terrain3d.h"

#include <gdextension_interface.h>
#include <godot_cpp/classes/editor_plugin_registration.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

void initialize_terrain_server_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(ts::TerrainBiomeLayer);
		GDREGISTER_CLASS(ts::TerrainSlopeLayer);
		GDREGISTER_CLASS(ts::TerrainConfiguration);
		GDREGISTER_CLASS(ts::TerrainGenerator);
		GDREGISTER_CLASS(ts::TerrainRenderer);
		GDREGISTER_CLASS(ts::TerrainPhysics);
		GDREGISTER_CLASS(ts::Terrain3D);
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_INTERNAL_CLASS(ts::TerrainClipmapGizmoPlugin);
		GDREGISTER_INTERNAL_CLASS(ts::TerrainCollisionGizmoPlugin);
		GDREGISTER_INTERNAL_CLASS(ts::TerrainFocusGizmoPlugin);
		GDREGISTER_INTERNAL_CLASS(ts::WhittakerChart);
		GDREGISTER_INTERNAL_CLASS(ts::TerrainDock);
		GDREGISTER_INTERNAL_CLASS(ts::TerrainServerEditorPlugin);
		EditorPlugins::add_by_type<ts::TerrainServerEditorPlugin>();
	}
}

void uninitialize_terrain_server_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::remove_by_type<ts::TerrainServerEditorPlugin>();
	}
}

extern "C" {
// Initialization.
GDExtensionBool GDE_EXPORT terrain_server_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_terrain_server_module);
	init_obj.register_terminator(uninitialize_terrain_server_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}
