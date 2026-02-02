#pragma once

#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/resource.hpp>

namespace ts {

/**
 * @class TerrainConfiguration
 * @brief A resource that stores configuration parameters for terrain generation and rendering.
 *
 * TerrainConfiguration is a Godot Resource that encapsulates all the parameters needed to
 * generate and render procedural terrain. It includes noise settings for height generation,
 * height scaling, mesh resolution, and optional material overrides.
 *
 * This resource emits the "changed" signal whenever any of its properties are modified,
 * allowing connected systems (like Terrain3D) to automatically update the terrain in response
 * to parameter changes in the editor.
 *
 * @note This class extends Godot's Resource, making it serializable and editable in the inspector.
 */
class TerrainConfiguration : public godot::Resource {
	GDCLASS(TerrainConfiguration, godot::Resource);

private:
	/** @brief Multiplier applied to noise values to control overall terrain height range */
	double _height_scale;

	/** @brief Number of vertices along each axis of the terrain mesh grid */
	int _mesh_resolution;

	/** @brief Physical size of the terrain in world units */
	float _terrain_size;

	/** @brief Optional material to apply to the terrain mesh, overriding default materials */
	godot::Ref<godot::Material> _material_override;

	/** @brief FastNoiseLite instance used for generating procedural height values */
	godot::Ref<godot::FastNoiseLite> _noise;

	/** @brief Number of clipmap levels for LOD terrain rendering */
	int _clipmap_levels;

protected:
	/**
	 * @brief Binds methods and properties to Godot's ClassDB system.
	 *
	 * Registers all getters, setters, and properties for use in GDScript and the editor.
	 */
	static void _bind_methods();

public:
	/**
	 * @brief Constructs a new TerrainConfiguration with default values.
	 *
	 * Initializes height scale to 100.0, mesh resolution to 32, and terrain size to 256.0.
	 * The noise property is null by default and must be set manually in the editor.
	 */
	TerrainConfiguration();

	/**
	 * @brief Destructor for TerrainConfiguration.
	 */
	~TerrainConfiguration();

	/**
	 * @brief Gets the current height scale multiplier.
	 *
	 * @return The height scale value used to multiply noise output.
	 */
	double get_height_scale() const;

	/**
	 * @brief Sets the height scale multiplier.
	 *
	 * Changes the multiplier applied to noise values, affecting the overall height range
	 * of the terrain. Emits the "changed" signal to trigger terrain updates.
	 *
	 * @param p_scale The new height scale value (typically positive).
	 */
	void set_height_scale(double p_scale);

	/**
	 * @brief Gets the current mesh resolution.
	 *
	 * @return The number of vertices along each axis of the terrain grid.
	 */
	int get_mesh_resolution() const;

	/**
	 * @brief Sets the mesh resolution.
	 *
	 * Changes the number of vertices along each axis of the terrain grid. Higher values
	 * produce more detailed terrain but require more processing. Emits the "changed" signal.
	 *
	 * @param p_resolution The new mesh resolution (number of vertices per axis).
	 */
	void set_mesh_resolution(int p_resolution);

	/**
	 * @brief Gets the current material override.
	 *
	 * @return Reference to the Material resource, or null if no override is set.
	 */
	godot::Ref<godot::Material> get_material_override() const;

	/**
	 * @brief Sets a material override for the terrain.
	 *
	 * Allows specifying a custom material to be applied to the terrain mesh.
	 * Emits the "changed" signal to trigger terrain updates.
	 *
	 * @param p_material Reference to a Material resource, or null to clear the override.
	 */
	void set_material_override(const godot::Ref<godot::Material> &p_material);

	/**
	 * @brief Gets the noise generator used for terrain height calculations.
	 *
	 * @return Reference to the FastNoiseLite instance.
	 */
	godot::Ref<godot::FastNoiseLite> get_noise() const;

	/**
	 * @brief Sets the noise generator for terrain height calculations.
	 *
	 * Replaces the current FastNoiseLite instance with a new one, affecting how
	 * terrain heights are generated. Emits the "changed" signal.
	 *
	 * @param p_noise Reference to a FastNoiseLite resource.
	 */
	void set_noise(const godot::Ref<godot::FastNoiseLite> &p_noise);

	/**
	 * @brief Gets the physical size of the terrain in world units.
	 *
	 * @return The terrain size value.
	 */
	float get_terrain_size() const;

	/**
	 * @brief Sets the physical size of the terrain in world units.
	 *
	 * Changes the physical dimensions of the terrain. Emits the "changed" signal
	 * to trigger terrain updates.
	 *
	 * @param p_size The new terrain size in world units.
	 */
	void set_terrain_size(float p_size);

	/**
	 * @brief Gets the number of clipmap levels for LOD terrain rendering.
	 *
	 * @return The number of clipmap levels.
	 */
	int get_clipmap_levels() const;

	/**
	 * @brief Sets the number of clipmap levels for LOD terrain rendering.
	 *
	 * Changes the number of clipmap levels used for level-of-detail terrain rendering.
	 * Emits the "changed" signal to trigger terrain updates.
	 *
	 * @param p_levels The new number of clipmap levels.
	 */
	void set_clipmap_levels(int p_levels);
};

} //namespace ts