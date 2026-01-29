#pragma once

#include "terrain_configuration.h"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/ref_counted.hpp>

namespace ts {

/**
 * @class TerrainGenerator
 * @brief Generates procedural terrain heights using noise algorithms.
 *
 * TerrainGenerator is responsible for calculating terrain heights at any given position
 * using FastNoiseLite noise functions. It can be configured through a TerrainConfiguration
 * resource to adjust noise parameters and height scaling.
 *
 */
class TerrainGenerator : public godot::RefCounted {
	GDCLASS(TerrainGenerator, godot::RefCounted);

private:
	/** @brief Noise generator used for terrain height calculations */
	godot::Ref<godot::FastNoiseLite> _noise;

	/** @brief Multiplier applied to noise values to control terrain height range */
	double _height_scale;

protected:
	/**
	 * @brief Binds methods to Godot's ClassDB system.
	 *
	 * Registers the methods for use in GDScript.
	 */
	static void _bind_methods();

public:
	/**
	 * @brief Constructs a new TerrainGenerator with default settings.
	 *
	 * Initializes the noise generator with Perlin noise and FBM fractal settings,
	 * and sets the default height scale to 1.0.
	 */
	TerrainGenerator();

	/**
	 * @brief Destructor for TerrainGenerator.
	 */
	~TerrainGenerator();

	/**
	 * @brief Configures the generator using a TerrainConfiguration resource.
	 *
	 * Applies noise settings and height scale from the provided configuration.
	 * If the configuration is invalid, no changes are made.
	 *
	 * @param p_config The TerrainConfiguration resource containing generator parameters.
	 */
	void setup(const godot::Ref<TerrainConfiguration> &p_config);

	/**
	 * @brief Calculates the terrain height at a specific 2D position.
	 *
	 * Uses the configured noise function to generate a height value, scaled by the
	 * height scale parameter. This method can be called frequently for dynamic terrain queries.
	 *
	 * @param x The X coordinate in world space.
	 * @param y The Y coordinate in world space (corresponds to Z in 3D space).
	 * @return The calculated height value at the given position.
	 */
	float get_height(float x, float y) const;

	/**
	 * @brief Generates a complete terrain mesh with the specified resolution and size.
	 *
	 * Creates an ArrayMesh containing a grid of triangles representing the terrain.
	 * The mesh is generated with proper normals and UV coordinates for texturing.
	 *
	 * @param resolution The number of vertices along each axis (creates resolution x resolution grid).
	 * @param size The total size of the terrain mesh in world units.
	 * @return A reference to the generated ArrayMesh.
	 */
	godot::Ref<godot::ArrayMesh> generate_mesh(int resolution, float size) const;

	/**
	 * @brief Creates mesh data for a terrain grid with specified parameters.
	 *
	 * Helper method that generates the vertex and triangle data for a terrain mesh.
	 * Used internally by generate_mesh and can be called by TerrainRenderer.
	 *
	 * @param resolution Number of quads along each axis.
	 * @param vertex_spacing Distance between adjacent vertices.
	 * @return Reference to the generated ArrayMesh.
	 */
	godot::Ref<godot::ArrayMesh> create_mesh_data(int resolution, float vertex_spacing) const;
};

} //namespace ts