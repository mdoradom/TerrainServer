#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/string.hpp>

#include "terrain_texture_slot.h"

namespace ts {

class TerrainBiomeLayer : public godot::Resource {
	GDCLASS(TerrainBiomeLayer, godot::Resource);

public:
	static constexpr int32_t MAX_TEXTURE_DIMENSION = TEXTURE_SLOT_MAX_DIMENSION;

private:
	godot::Ref<godot::Texture2D> _albedo_texture;
	godot::Ref<godot::Texture2D> _normal_texture;
	godot::Ref<godot::Texture2D> _roughness_texture;
	godot::Ref<godot::Texture2D> _height_texture;
	godot::Ref<godot::Texture2D> _ao_texture;
	float _uv_scale;
	float _pom_depth;

	float _min_temperature;
	float _max_temperature;

	float _min_moisture;
	float _max_moisture;

	godot::String _biome_name;

protected:
	static void _bind_methods();

public:
	TerrainBiomeLayer();
	~TerrainBiomeLayer() override;

	godot::Ref<godot::Texture2D> get_albedo_texture() const;
	void set_albedo_texture(const godot::Ref<godot::Texture2D> &p_texture);

	godot::Ref<godot::Texture2D> get_normal_texture() const;
	void set_normal_texture(const godot::Ref<godot::Texture2D> &p_texture);

	godot::Ref<godot::Texture2D> get_roughness_texture() const;
	void set_roughness_texture(const godot::Ref<godot::Texture2D> &p_texture);

	godot::Ref<godot::Texture2D> get_height_texture() const;
	void set_height_texture(const godot::Ref<godot::Texture2D> &p_texture);

	godot::Ref<godot::Texture2D> get_ao_texture() const;
	void set_ao_texture(const godot::Ref<godot::Texture2D> &p_texture);

	float get_uv_scale() const;
	void set_uv_scale(float p_scale);

	float get_pom_depth() const;
	void set_pom_depth(float p_depth);

	float get_min_temperature() const;
	void set_min_temperature(float p_temperature);

	float get_max_temperature() const;
	void set_max_temperature(float p_temperature);

	float get_min_moisture() const;
	void set_min_moisture(float p_moisture);

	float get_max_moisture() const;
	void set_max_moisture(float p_moisture);

	godot::String get_biome_name() const;
	void set_biome_name(const godot::String &p_name);
};

} //namespace ts
