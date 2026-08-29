#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/string.hpp>

namespace ts {

class TerrainBiomeLayer : public godot::Resource {
	GDCLASS(TerrainBiomeLayer, godot::Resource);

public:
	static constexpr int32_t MAX_TEXTURE_DIMENSION = 2048;

private:
	godot::Ref<godot::Texture2D> _albedo_texture;
	godot::Ref<godot::Texture2D> _normal_texture;
	godot::Ref<godot::Texture2D> _roughness_texture;

	float _min_temperature;
	float _max_temperature;

	float _min_moisture;
	float _max_moisture;

	godot::String _biome_name;

	void _set_texture(godot::Ref<godot::Texture2D> &r_slot, const godot::Ref<godot::Texture2D> &p_texture, const char *p_debug_kind);

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
