#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/texture2d.hpp>

#include "terrain_texture_slot.h"

namespace ts {

class TerrainSlopeLayer : public godot::Resource {
	GDCLASS(TerrainSlopeLayer, godot::Resource);

public:
	static constexpr int32_t MAX_TEXTURE_DIMENSION = TEXTURE_SLOT_MAX_DIMENSION;

private:
	godot::Ref<godot::Texture2D> _albedo_texture;
	godot::Ref<godot::Texture2D> _normal_texture;
	godot::Ref<godot::Texture2D> _roughness_texture;
	godot::Ref<godot::Texture2D> _height_texture;
	godot::Ref<godot::Texture2D> _ao_texture;
	float _uv_scale;

	float _slope_threshold;
	float _slope_blend_range;

protected:
	static void _bind_methods();

public:
	TerrainSlopeLayer();
	~TerrainSlopeLayer() override;

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

	float get_slope_threshold() const;
	void set_slope_threshold(float p_threshold);

	float get_slope_blend_range() const;
	void set_slope_blend_range(float p_range);
};

} //namespace ts
