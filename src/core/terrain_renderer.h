#pragma once

#include "core/terrain_generator.h"

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/ref_counted.hpp>

namespace ts {

/**
 * @class TerrainRenderer
 * @brief Manages the creation and rendering of terrain meshes in Godot.
 *
 * TerrainRenderer is responsible for generating and updating MeshInstance3D nodes
 * that display the terrain. It works closely with TerrainGenerator to obtain height
 * data and creates optimized triangle meshes for rendering.
 *
 * The renderer can apply material overrides and dynamically update mesh resolution
 * without recreating the entire scene structure.
 *
 * @note This class manages a MeshInstance3D child node named "TerrainMesh" under the parent node.
 */
class TerrainRenderer : public godot::RefCounted {
	GDCLASS(TerrainRenderer, godot::RefCounted);

private:
	/** @brief The MeshInstance3D node that displays the terrain mesh */
	godot::MeshInstance3D *_mesh_instance;

	/** @brief Optional material override applied to the terrain mesh */
	godot::Ref<godot::Material> _material_override;

	/** @brief The generator used to calculate terrain heights */
	godot::Ref<TerrainGenerator> _generator;

	/** @brief Parent Node3D that owns the mesh instance */
	godot::Node3D *_parent_node;

	/** @brief Number of vertices along each axis of the terrain grid */
	int _mesh_resolution;

protected:
	/**
	 * @brief Binds methods to Godot's ClassDB system.
	 *
	 * Registers the renderer's methods for use in GDScript.
	 */
	static void _bind_methods();

public:
	/**
	 * @brief Constructs a new TerrainRenderer with default settings.
	 *
	 * Initializes the mesh instance pointer to null and sets default mesh resolution to 32.
	 */
	TerrainRenderer();

	/**
	 * @brief Destructor for TerrainRenderer.
	 *
	 * Calls cleanup to properly free the mesh instance.
	 */
	~TerrainRenderer();

	/**
	 * @brief Initializes the renderer with a parent node.
	 *
	 * Sets the parent node and attempts to find an existing "TerrainMesh" child node.
	 * If found, reuses it; otherwise, a new one will be created during mesh generation.
	 *
	 * @param p_parent The Node3D that will be the parent of the terrain mesh.
	 */
	void initialize(godot::Node3D *p_parent);

	/**
	 * @brief Cleans up the renderer by freeing the mesh instance.
	 *
	 * Queues the mesh instance for deletion and sets the pointer to null.
	 * Should be called before destroying the renderer or when changing parent nodes.
	 */
	void cleanup();

	/**
	 * @brief Sets the terrain generator used for height calculations.
	 *
	 * The generator is used during mesh generation to obtain height values
	 * for each vertex position.
	 *
	 * @param p_generator Reference to a TerrainGenerator instance.
	 */
	void set_generator(const godot::Ref<TerrainGenerator> &p_generator);

	/**
	 * @brief Sets the mesh resolution (number of vertices per axis).
	 *
	 * Higher values create more detailed terrain but require more processing.
	 * The mesh must be regenerated after changing this value.
	 *
	 * @param p_resolution The number of vertices along each axis of the terrain grid.
	 */
	void set_mesh_resolution(int p_resolution);

	/**
	 * @brief Sets a material override for the terrain mesh.
	 *
	 * If a mesh instance exists, the material is applied immediately.
	 * Otherwise, it will be applied during the next mesh generation.
	 *
	 * @param p_material Reference to a Material resource to apply to the terrain.
	 */
	void set_material_override(const godot::Ref<godot::Material> &p_material);

	/**
	 * @brief Generates a new terrain mesh based on current settings.
	 *
	 * Creates or updates the MeshInstance3D node with a new mesh generated from
	 * the terrain generator. The mesh is created using SurfaceTool with proper
	 * normals, UVs, and triangle topology.
	 *
	 * If no mesh instance exists, creates one named "TerrainMesh" as a child
	 * of the parent node. Applies the material override if one is set.
	 *
	 * @note Requires a valid generator and parent node to function.
	 */
	void generate_mesh();

	/**
	 * @brief Updates the existing terrain mesh.
	 *
	 * Convenience method that calls generate_mesh() to refresh the terrain.
	 * Used when configuration changes require a mesh update.
	 */
	void update_mesh();
};

} //namespace ts