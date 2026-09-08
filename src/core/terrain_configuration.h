#pragma once

#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include "terrain_biome_layer.h"
#include "terrain_slope_layer.h"

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

	float _noise_ridge_amount;
	float _noise_ridge_offset;
	float _noise_ridge_weight_gain;

	float _noise_warp_amount;
	float _noise_warp_frequency;

	float _noise_continent_frequency;
	float _noise_continent_influence;
	float _noise_continent_contrast;
	float _noise_continent_elevation;
	float _noise_continent_sea_level;
	float _noise_relief_floor;

	float _noise_redistribution;

	int _clipmap_levels;

	float _physics_range;
	int _physics_collision_layer;
	int _physics_collision_mask;

	godot::TypedArray<TerrainBiomeLayer> _biome_layers;
	godot::Ref<TerrainSlopeLayer> _rock_layer;

	float _temperature_frequency;
	godot::Vector2 _temperature_offset;
	float _temperature_noise_influence;
	float _temperature_altitude_reference;

	float _moisture_frequency;
	godot::Vector2 _moisture_offset;

	int _pom_min_steps;
	int _pom_max_steps;
	float _pom_fade_start;
	float _pom_fade_end;
	float _triplanar_sharpness;

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

	float get_noise_ridge_amount() const;
	void set_noise_ridge_amount(float p_amount);

	float get_noise_ridge_offset() const;
	void set_noise_ridge_offset(float p_offset);

	float get_noise_ridge_weight_gain() const;
	void set_noise_ridge_weight_gain(float p_weight_gain);

	float get_noise_warp_amount() const;
	void set_noise_warp_amount(float p_amount);

	float get_noise_warp_frequency() const;
	void set_noise_warp_frequency(float p_frequency);

	float get_noise_continent_frequency() const;
	void set_noise_continent_frequency(float p_frequency);

	float get_noise_continent_influence() const;
	void set_noise_continent_influence(float p_influence);

	float get_noise_continent_contrast() const;
	void set_noise_continent_contrast(float p_contrast);

	float get_noise_continent_elevation() const;
	void set_noise_continent_elevation(float p_elevation);

	float get_noise_continent_sea_level() const;
	void set_noise_continent_sea_level(float p_sea_level);

	float get_noise_relief_floor() const;
	void set_noise_relief_floor(float p_floor);

	float get_noise_redistribution() const;
	void set_noise_redistribution(float p_redistribution);

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

	godot::Ref<TerrainSlopeLayer> get_rock_layer() const;
	void set_rock_layer(const godot::Ref<TerrainSlopeLayer> &p_layer);

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

	int get_pom_min_steps() const;
	void set_pom_min_steps(int p_steps);

	int get_pom_max_steps() const;
	void set_pom_max_steps(int p_steps);

	float get_pom_fade_start() const;
	void set_pom_fade_start(float p_distance);

	float get_pom_fade_end() const;
	void set_pom_fade_end(float p_distance);

	float get_triplanar_sharpness() const;
	void set_triplanar_sharpness(float p_sharpness);

	godot::Ref<godot::ImageTexture> get_noise_preview() const;
};

} //namespace ts