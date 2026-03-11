#pragma once

#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/resource.hpp>

namespace ts {

class TerrainConfiguration : public godot::Resource {
	GDCLASS(TerrainConfiguration, godot::Resource);

private:
	double _height_scale;
	int _mesh_resolution;
	float _terrain_size;

	godot::Ref<godot::FastNoiseLite> _noise;
	godot::Ref<godot::ImageTexture> _noise_texture;
	int _noise_texture_size;

	int _clipmap_levels;

	void _generate_noise_texture() const;
	void _on_noise_changed();

protected:
	static void _bind_methods();

public:
	TerrainConfiguration();
	~TerrainConfiguration();

	double get_height_scale() const;
	void set_height_scale(double p_scale);

	int get_mesh_resolution() const;
	void set_mesh_resolution(int p_resolution);

	godot::Ref<godot::FastNoiseLite> get_noise() const;
	void set_noise(const godot::Ref<godot::FastNoiseLite> &p_noise);

	godot::Ref<godot::ImageTexture> get_noise_texture() const;

	float get_terrain_size() const;
	void set_terrain_size(float p_size);

	int get_clipmap_levels() const;
	void set_clipmap_levels(int p_levels);

	int get_noise_texture_size() const;
	void set_noise_texture_size(int p_size);
};

} //namespace ts