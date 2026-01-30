#pragma once

#include "core/terrain_configuration.h"
#include "core/terrain_generator.h"
#include "core/terrain_renderer.h"

#include <godot_cpp/classes/node3d.hpp>

using namespace godot;

namespace ts {

/**
 * @class Terrain3D
 * @brief A 3D terrain node that generates procedural terrain using noise-based height maps.
 *
 * Terrain3D is a Node3D that manages terrain generation and rendering in Godot.
 * It uses a TerrainConfiguration to define parameters, a TerrainGenerator to compute
 * terrain heights from noise, and a TerrainRenderer to create and update the mesh.
 *
 * The terrain updates automatically when configuration changes are made in the editor,
 * including modifications to noise parameters, height scale, mesh resolution, and materials.
 *
 * @note This class creates a child MeshInstance3D node named "TerrainMesh" to display the terrain.
 */
class Terrain3D : public godot::Node3D {
	GDCLASS(Terrain3D, godot::Node3D);

private:
	/** @brief Configuration resource containing terrain parameters */
	Ref<TerrainConfiguration> _config;

	/** @brief Generator responsible for calculating terrain heights */
	Ref<TerrainGenerator> _generator;

	/** @brief Renderer that creates and updates the terrain mesh */
	Ref<TerrainRenderer> _renderer;

protected:
	/**
	 * @brief Binds methods and properties to Godot's ClassDB system.
	 *
	 * Registers the configuration property and internal methods for signal handling.
	 */
	static void _bind_methods();

	/**
	 * @brief Updates the generator with current configuration and regenerates the mesh.
	 *
	 * This method is called automatically when the configuration changes via the "changed" signal.
	 * It applies new parameters to the generator and triggers a mesh regeneration if the node
	 * is already in the scene tree.
	 */
	void _update_generator();

public:
	/**
	 * @brief Constructs a new Terrain3D node.
	 *
	 * Initializes the generator and renderer instances.
	 */
	Terrain3D();

	/**
	 * @brief Destructor for Terrain3D.
	 */
	~Terrain3D();

	/**
	 * @brief Called when the node enters the scene tree.
	 *
	 * Initializes the renderer with this node as parent and performs initial mesh generation.
	 */
	void _ready() override;

	/**
	 * @brief Called every frame. Currently unused in the editor.
	 *
	 * @param delta Time elapsed since the previous frame in seconds.
	 */
	void _process(double delta) override;

	/**
	 * @brief Gets the current terrain configuration.
	 *
	 * @return Reference to the TerrainConfiguration resource.
	 */
	Ref<TerrainConfiguration> get_configuration() const;

	/**
	 * @brief Sets a new terrain configuration.
	 *
	 * Disconnects signals from the old configuration (if any), connects to the new one,
	 * and triggers a generator update. The "changed" signal from the configuration will
	 * automatically update the terrain when any parameter changes.
	 *
	 * @param p_config The new TerrainConfiguration to use.
	 */
	void set_configuration(const Ref<TerrainConfiguration> &p_config);

	/**
	 * @brief Gets the terrain generator.
	 *
	 * @return Reference to the TerrainGenerator instance.
	 */
	Ref<TerrainGenerator> get_generator() const;

	/**
	 * @brief Gets the terrain renderer.
	 *
	 * @return Reference to the TerrainRenderer instance.
	 */
	Ref<TerrainRenderer> get_renderer() const;
};

} //namespace ts