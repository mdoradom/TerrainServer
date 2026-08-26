#pragma once

#include "terrain_configuration.h"

#include <cstdint>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <vector>

namespace ts {

class TerrainRenderer : public godot::RefCounted {
	GDCLASS(TerrainRenderer, godot::RefCounted);

private:
	godot::RID _mesh_rid;
	godot::RID _mesh_ring_rid;
	godot::RID _internal_shader_rid;
	godot::Node3D *_parent_node = nullptr;

	godot::Ref<TerrainConfiguration> _config;

	struct ClipmapLevel {
		godot::RID instance_rid;
		godot::RID material_rid;
		float scale;
	};

	std::vector<ClipmapLevel> _clipmap_levels;

	godot::RID _biome_albedo_array_rid;
	godot::RID _biome_normal_array_rid;
	godot::RID _biome_roughness_array_rid;
	std::vector<uint64_t> _biome_texture_signature;
	bool _biome_texture_arrays_built = false;
	int _biome_layer_count = 0;

	void _free_mesh_instances();

	std::vector<uint64_t> _compute_biome_texture_signature(const godot::TypedArray<TerrainBiomeLayer> &p_layers) const;
	void _rebuild_biome_texture_arrays_if_dirty(const godot::TypedArray<TerrainBiomeLayer> &p_layers);
	int _build_biome_texture_arrays(const godot::TypedArray<TerrainBiomeLayer> &p_layers);
	static godot::Ref<godot::Image> _prepare_layer_image(const godot::Ref<godot::Texture2D> &p_texture,
			int p_expected_width, int p_expected_height, const godot::Color &p_placeholder_color,
			const char *p_debug_kind, int p_layer_index, bool &r_size_mismatch);
	void _free_biome_texture_arrays();

protected:
	static void _bind_methods();

public:
	TerrainRenderer();
	~TerrainRenderer();

	void initialize(godot::Node3D *p_parent);
	void cleanup();

	void set_configuration(const godot::Ref<TerrainConfiguration> &p_config);

	void rebuild_mesh(float p_size, int p_resolution);
	void update_focus_position(godot::Vector3 p_focus_pos);
};

} //namespace ts