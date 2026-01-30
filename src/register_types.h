#pragma once

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

/**
 * @brief Initializes the Terrain Server GDExtension module.
 *
 * This function is called by Godot during the extension loading process.
 * It registers all custom classes (Terrain3D, TerrainConfiguration, TerrainGenerator,
 * and TerrainRenderer) with Godot's ClassDB system when the initialization level
 * matches MODULE_INITIALIZATION_LEVEL_SCENE.
 *
 * @param p_level The current module initialization level being processed.
 *                Only acts when p_level equals MODULE_INITIALIZATION_LEVEL_SCENE.
 */
void initialize_terrain_server_module(ModuleInitializationLevel p_level);

/**
 * @brief Uninitializes the Terrain Server GDExtension module.
 *
 * This function is called by Godot during the extension unloading process.
 * It performs cleanup operations when the module is being shut down.
 * Currently, handles cleanup only at MODULE_INITIALIZATION_LEVEL_SCENE level.
 *
 * @param p_level The current module initialization level being processed.
 *                Only acts when p_level equals MODULE_INITIALIZATION_LEVEL_SCENE.
 */
void uninitialize_terrain_server_module(ModuleInitializationLevel p_level);