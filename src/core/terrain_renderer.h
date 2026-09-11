#pragma once

#include "terrain_configuration.h"
#include "terrain_slope_layer.h"

#include <cstdint>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <vector>

namespace ts {

class TerrainRenderer : public godot::RefCounted {
	GDCLASS(TerrainRenderer, godot::RefCounted);

	// Sentinel slice index for a biome with no rock material in effect.
	static constexpr int ROCK_SLICE_NONE = -1;

private:
	godot::RID _mesh_rid;
	godot::RID _mesh_ring_rid;
	godot::RID _mesh_trim_rids[4];
	godot::RID _internal_shader_rid;
	bool _shader_reload_pending = false;
	int _debug_view = 0;
	godot::Node3D *_parent_node = nullptr;

	godot::Ref<TerrainConfiguration> _config;

	struct ClipmapLevel {
		godot::RID instance_rid;
		godot::RID material_rid;
		float scale;
		godot::RID trim_instance_rid;
		int trim_variant = -1;
		godot::Vector2 origin;
	};

	std::vector<ClipmapLevel> _clipmap_levels;

	godot::RID _biome_albedo_array_rid;
	godot::RID _biome_normal_array_rid;
	godot::RID _biome_roughness_array_rid;
	godot::RID _biome_height_array_rid;
	godot::RID _biome_ao_array_rid;
	std::vector<uint64_t> _biome_texture_signature;
	// Distinguishes "never built" from "built with zero layers" (both leave the RIDs/signature
	// empty), so a zero-layer config doesn't get rebuilt every call.
	bool _biome_texture_arrays_built = false;
	int _biome_layer_count = 0;

	godot::RID _rock_albedo_array_rid;
	godot::RID _rock_normal_array_rid;
	godot::RID _rock_roughness_array_rid;
	godot::RID _rock_height_array_rid;
	godot::RID _rock_ao_array_rid;
	std::vector<uint64_t> _rock_texture_signature;
	// Same "never built" vs. "built with no rock layer" distinction as the biome arrays above.
	bool _rock_textures_built = false;
	// Slice each biome samples its rock from, and the slice used when no biome layers exist.
	// ROCK_SLICE_NONE means that biome has no rock material at all.
	std::vector<int> _rock_biome_slice;
	int _rock_fallback_slice = ROCK_SLICE_NONE;
	int _rock_slice_count = 0;

	void _free_mesh_instances();

	std::vector<uint64_t> _compute_biome_texture_signature(const godot::TypedArray<TerrainBiomeLayer> &p_layers) const;
	void _rebuild_biome_texture_arrays_if_dirty(const godot::TypedArray<TerrainBiomeLayer> &p_layers);
	int _build_biome_texture_arrays(const godot::TypedArray<TerrainBiomeLayer> &p_layers);
	static godot::Ref<godot::Image> _prepare_layer_image(const godot::Ref<godot::Texture2D> &p_texture,
			int p_expected_width, int p_expected_height, const godot::Color &p_placeholder_color,
			const char *p_debug_kind, int p_layer_index, bool &r_size_mismatch);
	void _free_biome_texture_arrays();

	// A biome's own slope layer if it has one, otherwise the configuration's shared rock layer.
	static godot::Ref<TerrainSlopeLayer> _effective_slope_layer(const godot::Ref<TerrainBiomeLayer> &p_biome,
			const godot::Ref<TerrainSlopeLayer> &p_fallback);
	std::vector<uint64_t> _compute_rock_texture_signature(const godot::TypedArray<TerrainBiomeLayer> &p_biomes,
			const godot::Ref<TerrainSlopeLayer> &p_fallback) const;
	void _rebuild_rock_texture_arrays_if_dirty(const godot::TypedArray<TerrainBiomeLayer> &p_biomes,
			const godot::Ref<TerrainSlopeLayer> &p_fallback);
	void _build_rock_texture_arrays(const godot::TypedArray<TerrainBiomeLayer> &p_biomes,
			const godot::Ref<TerrainSlopeLayer> &p_fallback);
	void _free_rock_texture_arrays();

protected:
	static void _bind_methods();

public:
	TerrainRenderer();
	~TerrainRenderer();

	void initialize(godot::Node3D *p_parent);
	void cleanup();

	void set_configuration(const godot::Ref<TerrainConfiguration> &p_config);

	void rebuild_mesh(float p_size, int p_resolution);
	void request_shader_reload();
	void update_focus_position(godot::Vector3 p_focus_pos);

	void set_debug_view(int p_debug_view);
	int get_debug_view() const;

	int get_clipmap_level_count() const;
	float get_clipmap_level_scale(int p_level) const;
	godot::Vector2 get_snapped_focus_xz() const;
	godot::Vector2 get_clipmap_level_origin(int p_level) const;

	static constexpr float MORPH_BAND_START = 0.33f;
};

} //namespace ts