#pragma once

#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include "terrain_biome_layer.h"

namespace ts {

class TerrainConfiguration : public godot::Resource {
	GDCLASS(TerrainConfiguration, godot::Resource);

public:
	static constexpr int32_t MAX_BIOME_LAYERS = 32;

private:
	double _height_scale;
	int _mesh_resolution;
	float _terrain_size;

	int _noise_octaves;
	float _noise_base_frequency;
	float _noise_lacunarity;
	float _noise_gain;

	int _clipmap_levels;

	float _physics_range;
	int _physics_collision_layer;
	int _physics_collision_mask;

	godot::TypedArray<TerrainBiomeLayer> _biome_layers;

	float _temperature_frequency;
	godot::Vector2 _temperature_offset;
	float _temperature_noise_influence;
	float _temperature_altitude_reference;

	float _moisture_frequency;
	godot::Vector2 _moisture_offset;

	godot::Ref<godot::FastNoiseLite> _internal_noise;
	godot::Ref<godot::ImageTexture> _noise_preview;
	void _update_preview();

protected:
	static void _bind_methods();

public:
	TerrainConfiguration();
	~TerrainConfiguration() override;

	double get_height_scale() const;
	void set_height_scale(double p_scale);

	int get_mesh_resolution() const;
	void set_mesh_resolution(int p_resolution);

	int get_noise_octaves() const;
	void set_noise_octaves(int p_octaves);

	float get_noise_base_frequency() const;
	void set_noise_base_frequency(float p_base_frequency);

	float get_noise_lacunarity() const;
	void set_noise_lacunarity(float p_lacunarity);

	float get_noise_gain() const;
	void set_noise_gain(float p_gain);

	float get_terrain_size() const;
	void set_terrain_size(float p_size);

	int get_clipmap_levels() const;
	void set_clipmap_levels(int p_levels);

	float get_physics_range() const;
	void set_physics_range(float p_range);

	int get_physics_collision_layer() const;
	void set_physics_collision_layer(int p_layer);

	int get_physics_collision_mask() const;
	void set_physics_collision_mask(int p_mask);

	godot::TypedArray<TerrainBiomeLayer> get_biome_layers() const;
	void set_biome_layers(const godot::TypedArray<TerrainBiomeLayer> &p_layers);

	float get_temperature_frequency() const;
	void set_temperature_frequency(float p_frequency);

	godot::Vector2 get_temperature_offset() const;
	void set_temperature_offset(godot::Vector2 p_offset);

	float get_temperature_noise_influence() const;
	void set_temperature_noise_influence(float p_influence);

	float get_temperature_altitude_reference() const;
	void set_temperature_altitude_reference(float p_reference);

	float get_moisture_frequency() const;
	void set_moisture_frequency(float p_frequency);

	godot::Vector2 get_moisture_offset() const;
	void set_moisture_offset(godot::Vector2 p_offset);

	godot::Ref<godot::ImageTexture> get_noise_preview() const;
};

} //namespace ts