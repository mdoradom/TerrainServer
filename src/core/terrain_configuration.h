#pragma once

#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/texture2d.hpp>

namespace ts {

class TerrainConfiguration : public godot::Resource {
	GDCLASS(TerrainConfiguration, godot::Resource);

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

	godot::Ref<godot::Texture2D> _albedo_texture;

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

	godot::Ref<godot::Texture2D> get_albedo_texture() const;
	void set_albedo_texture(const godot::Ref<godot::Texture2D> &p_texture);

	godot::Ref<godot::ImageTexture> get_noise_preview() const;
};

} //namespace ts