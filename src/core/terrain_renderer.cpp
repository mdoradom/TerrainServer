#include "terrain_renderer.h"

#include "terrain_generator.h"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector3.hpp>

using namespace godot;

namespace ts {

constexpr float HEIGHT_AABB_MARGIN = 1.5f;
constexpr const char *SHADER_PATH = "res://addons/terrain_server/shaders/terrain.gdshader";

namespace {
// Fallback dimensions and fill colours for a layer channel with no texture assigned.
// Height is black rather than mid-grey: a missing height map must read as a flat surface
// to the parallax raymarch instead of sitting half a depth-range below it.
constexpr int DEFAULT_DIM = 4;
const Color PLACEHOLDER_ALBEDO(0.5f, 0.5f, 0.5f, 1.0f);
const Color PLACEHOLDER_NORMAL(0.5f, 0.5f, 1.0f, 1.0f);
const Color PLACEHOLDER_ROUGHNESS(0.5f, 0.5f, 0.5f, 1.0f);
const Color PLACEHOLDER_HEIGHT(0.0f, 0.0f, 0.0f, 1.0f);
const Color PLACEHOLDER_AO(1.0f, 1.0f, 1.0f, 1.0f);

constexpr float ROCK_DISABLED_SLOPE_THRESHOLD = 2.0f;
constexpr float ROCK_DISABLED_SLOPE_BLEND_RANGE = 0.001f;

void free_rid_if_valid(RenderingServer *p_rs, RID &p_rid) {
	if (p_rid.is_valid()) {
		p_rs->free_rid(p_rid);
		p_rid = RID();
	}
}
} //namespace

void TerrainRenderer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_clipmap_level_count"), &TerrainRenderer::get_clipmap_level_count);
	ClassDB::bind_method(D_METHOD("get_clipmap_level_scale", "level"), &TerrainRenderer::get_clipmap_level_scale);
	ClassDB::bind_method(D_METHOD("get_snapped_focus_xz"), &TerrainRenderer::get_snapped_focus_xz);
	ClassDB::bind_method(D_METHOD("get_clipmap_level_origin", "level"), &TerrainRenderer::get_clipmap_level_origin);
	ClassDB::bind_method(D_METHOD("request_shader_reload"), &TerrainRenderer::request_shader_reload);
	ClassDB::bind_method(D_METHOD("set_debug_view", "debug_view"), &TerrainRenderer::set_debug_view);
	ClassDB::bind_method(D_METHOD("get_debug_view"), &TerrainRenderer::get_debug_view);
}

TerrainRenderer::TerrainRenderer() = default;

TerrainRenderer::~TerrainRenderer() {
	cleanup();
}

void TerrainRenderer::initialize(Node3D *p_parent) {
	_parent_node = p_parent;
}

void TerrainRenderer::_free_mesh_instances() {
	RenderingServer *rs = RenderingServer::get_singleton();

	for (const auto &level : _clipmap_levels) {
		if (level.instance_rid.is_valid()) {
			rs->free_rid(level.instance_rid);
		}

		if (level.trim_instance_rid.is_valid()) {
			rs->free_rid(level.trim_instance_rid);
		}

		if (level.material_rid.is_valid()) {
			rs->free_rid(level.material_rid);
		}
	}

	_clipmap_levels.clear();

	if (_mesh_rid.is_valid()) {
		rs->free_rid(_mesh_rid);
		_mesh_rid = RID();
	}

	if (_mesh_ring_rid.is_valid()) {
		rs->free_rid(_mesh_ring_rid);
		_mesh_ring_rid = RID();
	}

	for (RID &trim_rid : _mesh_trim_rids) {
		free_rid_if_valid(rs, trim_rid);
	}
}

void TerrainRenderer::cleanup() {
	_free_mesh_instances();
	_free_biome_texture_arrays();
	_free_rock_texture_arrays();

	RenderingServer *rs = RenderingServer::get_singleton();
	if (_internal_shader_rid.is_valid()) {
		rs->free_rid(_internal_shader_rid);
		_internal_shader_rid = RID();
	}
}

void TerrainRenderer::_free_biome_texture_arrays() {
	RenderingServer *rs = RenderingServer::get_singleton();

	free_rid_if_valid(rs, _biome_albedo_array_rid);
	free_rid_if_valid(rs, _biome_normal_array_rid);
	free_rid_if_valid(rs, _biome_roughness_array_rid);
	free_rid_if_valid(rs, _biome_height_array_rid);
	free_rid_if_valid(rs, _biome_ao_array_rid);

	_biome_texture_signature.clear();
	_biome_texture_arrays_built = false;
	_biome_layer_count = 0;
}

std::vector<uint64_t> TerrainRenderer::_compute_biome_texture_signature(const TypedArray<TerrainBiomeLayer> &p_layers) const {
	std::vector<uint64_t> signature;
	signature.reserve(static_cast<size_t>(p_layers.size()) * 5);

	for (int i = 0; i < p_layers.size(); i++) {
		const Ref<TerrainBiomeLayer> layer = p_layers[i];
		if (!layer.is_valid()) {
			signature.push_back(0);
			signature.push_back(0);
			signature.push_back(0);
			signature.push_back(0);
			signature.push_back(0);
			continue;
		}

		const Ref<Texture2D> albedo = layer->get_albedo_texture();
		const Ref<Texture2D> normal = layer->get_normal_texture();
		const Ref<Texture2D> roughness = layer->get_roughness_texture();
		const Ref<Texture2D> height = layer->get_height_texture();
		const Ref<Texture2D> ao = layer->get_ao_texture();
		signature.push_back(albedo.is_valid() ? albedo->get_instance_id() : 0);
		signature.push_back(normal.is_valid() ? normal->get_instance_id() : 0);
		signature.push_back(roughness.is_valid() ? roughness->get_instance_id() : 0);
		signature.push_back(height.is_valid() ? height->get_instance_id() : 0);
		signature.push_back(ao.is_valid() ? ao->get_instance_id() : 0);
	}

	return signature;
}

void TerrainRenderer::_rebuild_biome_texture_arrays_if_dirty(const TypedArray<TerrainBiomeLayer> &p_layers) {
	std::vector<uint64_t> new_signature = _compute_biome_texture_signature(p_layers);
	if (_biome_texture_arrays_built && new_signature == _biome_texture_signature) {
		return;
	}

	_biome_layer_count = _build_biome_texture_arrays(p_layers);
	_biome_texture_signature = std::move(new_signature);
	_biome_texture_arrays_built = true;
}

Ref<Image> TerrainRenderer::_prepare_layer_image(const Ref<Texture2D> &p_texture, const int p_expected_width, const int p_expected_height,
		const Color &p_placeholder_color, const char *p_debug_kind, const int p_layer_index, bool &r_size_mismatch) {
	Ref<Image> image = p_texture.is_valid() ? p_texture->get_image() : Ref<Image>();

	if (!image.is_valid()) {
		image = Image::create(p_expected_width, p_expected_height, false, Image::FORMAT_RGBA8);
		image->fill(p_placeholder_color);
		image->generate_mipmaps();
		return image;
	}

	image = image->duplicate();

	if (image->get_width() != p_expected_width || image->get_height() != p_expected_height) {
		UtilityFunctions::push_warning("TerrainRenderer: biome layer ", p_layer_index, "'s ", p_debug_kind,
				" texture (", image->get_width(), "x", image->get_height(), ") does not match the reference size (",
				p_expected_width, "x", p_expected_height, ") for this texture type.");
		r_size_mismatch = true;

		image->resize(p_expected_width, p_expected_height, Image::INTERPOLATE_LANCZOS);
	}

	if (image->is_compressed()) {
		image->decompress();
	}
	if (image->get_format() != Image::FORMAT_RGBA8) {
		image->convert(Image::FORMAT_RGBA8);
	}
	if (image->has_mipmaps()) {
		image->clear_mipmaps();
	}
	image->generate_mipmaps();

	return image;
}

int TerrainRenderer::_build_biome_texture_arrays(const TypedArray<TerrainBiomeLayer> &p_layers) {
	RenderingServer *rs = RenderingServer::get_singleton();

	CRASH_COND_MSG(p_layers.size() > TerrainConfiguration::MAX_BIOME_LAYERS,
			"TerrainRenderer: biome_layers exceeds MAX_BIOME_LAYERS; TerrainConfiguration::set_biome_layers should have truncated this already.");

	const int num_layers = p_layers.size();
	if (num_layers == 0) {
		UtilityFunctions::push_warning("TerrainRenderer: no biome layers configured; rendering with a solid debug color until at least one TerrainBiomeLayer is assigned.");
		free_rid_if_valid(rs, _biome_albedo_array_rid);
		free_rid_if_valid(rs, _biome_normal_array_rid);
		free_rid_if_valid(rs, _biome_roughness_array_rid);
		free_rid_if_valid(rs, _biome_height_array_rid);
		free_rid_if_valid(rs, _biome_ao_array_rid);
		return 0;
	}

	int albedo_w = 0, albedo_h = 0;
	int normal_w = 0, normal_h = 0;
	int roughness_w = 0, roughness_h = 0;
	int height_w = 0, height_h = 0;
	int ao_w = 0, ao_h = 0;

	for (int i = 0; i < num_layers; i++) {
		const Ref<TerrainBiomeLayer> layer = p_layers[i];
		if (!layer.is_valid()) {
			continue;
		}
		if (albedo_w == 0) {
			if (const Ref<Texture2D> t = layer->get_albedo_texture(); t.is_valid()) {
				albedo_w = t->get_width();
				albedo_h = t->get_height();
			}
		}
		if (normal_w == 0) {
			if (const Ref<Texture2D> t = layer->get_normal_texture(); t.is_valid()) {
				normal_w = t->get_width();
				normal_h = t->get_height();
			}
		}
		if (roughness_w == 0) {
			if (const Ref<Texture2D> t = layer->get_roughness_texture(); t.is_valid()) {
				roughness_w = t->get_width();
				roughness_h = t->get_height();
			}
		}
		if (height_w == 0) {
			if (const Ref<Texture2D> t = layer->get_height_texture(); t.is_valid()) {
				height_w = t->get_width();
				height_h = t->get_height();
			}
		}
		if (ao_w == 0) {
			if (const Ref<Texture2D> t = layer->get_ao_texture(); t.is_valid()) {
				ao_w = t->get_width();
				ao_h = t->get_height();
			}
		}
	}

	if (albedo_w == 0) {
		albedo_w = albedo_h = DEFAULT_DIM;
	}
	if (normal_w == 0) {
		normal_w = normal_h = DEFAULT_DIM;
	}
	if (roughness_w == 0) {
		roughness_w = roughness_h = DEFAULT_DIM;
	}
	if (height_w == 0) {
		height_w = height_h = DEFAULT_DIM;
	}
	if (ao_w == 0) {
		ao_w = ao_h = DEFAULT_DIM;
	}

	// Already enforced upstream by TerrainBiomeLayer's setters (reject textures larger than
	// MAX_TEXTURE_DIMENSION) -- reaching this point with an oversized reference is a bug.
	CRASH_COND_MSG(albedo_w > TerrainBiomeLayer::MAX_TEXTURE_DIMENSION || albedo_h > TerrainBiomeLayer::MAX_TEXTURE_DIMENSION ||
					normal_w > TerrainBiomeLayer::MAX_TEXTURE_DIMENSION || normal_h > TerrainBiomeLayer::MAX_TEXTURE_DIMENSION ||
					roughness_w > TerrainBiomeLayer::MAX_TEXTURE_DIMENSION || roughness_h > TerrainBiomeLayer::MAX_TEXTURE_DIMENSION ||
					height_w > TerrainBiomeLayer::MAX_TEXTURE_DIMENSION || height_h > TerrainBiomeLayer::MAX_TEXTURE_DIMENSION ||
					ao_w > TerrainBiomeLayer::MAX_TEXTURE_DIMENSION || ao_h > TerrainBiomeLayer::MAX_TEXTURE_DIMENSION,
			"TerrainRenderer: a biome layer texture exceeds MAX_TEXTURE_DIMENSION; TerrainBiomeLayer's setters should have rejected this already.");

	TypedArray<Ref<Image>> albedo_images, normal_images, roughness_images, height_images, ao_images;
	bool mismatch = false;

	for (int i = 0; i < num_layers; i++) {
		const Ref<TerrainBiomeLayer> layer = p_layers[i];
		const Ref<Texture2D> albedo_tex = layer.is_valid() ? layer->get_albedo_texture() : Ref<Texture2D>();
		const Ref<Texture2D> normal_tex = layer.is_valid() ? layer->get_normal_texture() : Ref<Texture2D>();
		const Ref<Texture2D> roughness_tex = layer.is_valid() ? layer->get_roughness_texture() : Ref<Texture2D>();
		const Ref<Texture2D> height_tex = layer.is_valid() ? layer->get_height_texture() : Ref<Texture2D>();
		const Ref<Texture2D> ao_tex = layer.is_valid() ? layer->get_ao_texture() : Ref<Texture2D>();

		bool layer_mismatch = false;
		Ref<Image> a = _prepare_layer_image(albedo_tex, albedo_w, albedo_h, PLACEHOLDER_ALBEDO, "albedo", i, layer_mismatch);
		Ref<Image> n = _prepare_layer_image(normal_tex, normal_w, normal_h, PLACEHOLDER_NORMAL, "normal", i, layer_mismatch);
		Ref<Image> r = _prepare_layer_image(roughness_tex, roughness_w, roughness_h, PLACEHOLDER_ROUGHNESS, "roughness", i, layer_mismatch);
		Ref<Image> h = _prepare_layer_image(height_tex, height_w, height_h, PLACEHOLDER_HEIGHT, "height", i, layer_mismatch);
		Ref<Image> o = _prepare_layer_image(ao_tex, ao_w, ao_h, PLACEHOLDER_AO, "ao", i, layer_mismatch);

		mismatch = mismatch || layer_mismatch;
		albedo_images.push_back(a);
		normal_images.push_back(n);
		roughness_images.push_back(r);
		height_images.push_back(h);
		ao_images.push_back(o);
	}

	if (mismatch) {
		UtilityFunctions::push_warning("TerrainRenderer: one or more biome layer textures do not match the first configured layer's dimensions for their texture type; falling back to the zero-layers debug render until sizes are made consistent.");
		free_rid_if_valid(rs, _biome_albedo_array_rid);
		free_rid_if_valid(rs, _biome_normal_array_rid);
		free_rid_if_valid(rs, _biome_roughness_array_rid);
		free_rid_if_valid(rs, _biome_height_array_rid);
		free_rid_if_valid(rs, _biome_ao_array_rid);
		return 0;
	}

	const RID new_albedo_rid = rs->texture_2d_layered_create(albedo_images, RenderingServer::TEXTURE_LAYERED_2D_ARRAY);
	const RID new_normal_rid = rs->texture_2d_layered_create(normal_images, RenderingServer::TEXTURE_LAYERED_2D_ARRAY);
	const RID new_roughness_rid = rs->texture_2d_layered_create(roughness_images, RenderingServer::TEXTURE_LAYERED_2D_ARRAY);
	const RID new_height_rid = rs->texture_2d_layered_create(height_images, RenderingServer::TEXTURE_LAYERED_2D_ARRAY);
	const RID new_ao_rid = rs->texture_2d_layered_create(ao_images, RenderingServer::TEXTURE_LAYERED_2D_ARRAY);

	free_rid_if_valid(rs, _biome_albedo_array_rid);
	free_rid_if_valid(rs, _biome_normal_array_rid);
	free_rid_if_valid(rs, _biome_roughness_array_rid);
	free_rid_if_valid(rs, _biome_height_array_rid);
	free_rid_if_valid(rs, _biome_ao_array_rid);

	_biome_albedo_array_rid = new_albedo_rid;
	_biome_normal_array_rid = new_normal_rid;
	_biome_roughness_array_rid = new_roughness_rid;
	_biome_height_array_rid = new_height_rid;
	_biome_ao_array_rid = new_ao_rid;

	return num_layers;
}

void TerrainRenderer::_free_rock_texture_arrays() {
	RenderingServer *rs = RenderingServer::get_singleton();

	free_rid_if_valid(rs, _rock_albedo_array_rid);
	free_rid_if_valid(rs, _rock_normal_array_rid);
	free_rid_if_valid(rs, _rock_roughness_array_rid);
	free_rid_if_valid(rs, _rock_height_array_rid);
	free_rid_if_valid(rs, _rock_ao_array_rid);

	_rock_texture_signature.clear();
	_rock_textures_built = false;
	_rock_biome_slice.clear();
	_rock_fallback_slice = ROCK_SLICE_NONE;
	_rock_slice_count = 0;
}

Ref<TerrainSlopeLayer> TerrainRenderer::_effective_slope_layer(const Ref<TerrainBiomeLayer> &p_biome,
		const Ref<TerrainSlopeLayer> &p_fallback) {
	if (p_biome.is_valid()) {
		const Ref<TerrainSlopeLayer> own = p_biome->get_slope_layer();
		if (own.is_valid()) {
			return own;
		}
	}
	return p_fallback;
}

static std::vector<uint64_t> slope_texture_key(const Ref<TerrainSlopeLayer> &p_layer) {
	if (!p_layer.is_valid()) {
		return {};
	}

	const Ref<Texture2D> albedo = p_layer->get_albedo_texture();
	const Ref<Texture2D> normal = p_layer->get_normal_texture();
	const Ref<Texture2D> roughness = p_layer->get_roughness_texture();
	const Ref<Texture2D> height = p_layer->get_height_texture();
	const Ref<Texture2D> ao = p_layer->get_ao_texture();

	return {
		albedo.is_valid() ? albedo->get_instance_id() : 0,
		normal.is_valid() ? normal->get_instance_id() : 0,
		roughness.is_valid() ? roughness->get_instance_id() : 0,
		height.is_valid() ? height->get_instance_id() : 0,
		ao.is_valid() ? ao->get_instance_id() : 0,
	};
}

std::vector<uint64_t> TerrainRenderer::_compute_rock_texture_signature(const TypedArray<TerrainBiomeLayer> &p_biomes,
		const Ref<TerrainSlopeLayer> &p_fallback) const {
	std::vector<uint64_t> signature;

	const std::vector<uint64_t> fallback_key = slope_texture_key(p_fallback);
	signature.push_back(fallback_key.size());
	signature.insert(signature.end(), fallback_key.begin(), fallback_key.end());

	for (int i = 0; i < p_biomes.size(); i++) {
		const Ref<TerrainBiomeLayer> biome = p_biomes[i];
		const std::vector<uint64_t> key = slope_texture_key(_effective_slope_layer(biome, p_fallback));
		signature.push_back(key.size());
		signature.insert(signature.end(), key.begin(), key.end());
	}

	return signature;
}

void TerrainRenderer::_rebuild_rock_texture_arrays_if_dirty(const TypedArray<TerrainBiomeLayer> &p_biomes,
		const Ref<TerrainSlopeLayer> &p_fallback) {
	std::vector<uint64_t> new_signature = _compute_rock_texture_signature(p_biomes, p_fallback);
	if (_rock_textures_built && new_signature == _rock_texture_signature) {
		return;
	}

	_free_rock_texture_arrays();
	_build_rock_texture_arrays(p_biomes, p_fallback);

	_rock_texture_signature = std::move(new_signature);
	_rock_textures_built = true;
}

void TerrainRenderer::_build_rock_texture_arrays(const TypedArray<TerrainBiomeLayer> &p_biomes,
		const Ref<TerrainSlopeLayer> &p_fallback) {
	RenderingServer *rs = RenderingServer::get_singleton();

	const int num_biomes = p_biomes.size();
	_rock_biome_slice.assign(static_cast<size_t>(num_biomes), ROCK_SLICE_NONE);

	std::vector<Ref<TerrainSlopeLayer>> slices;
	std::vector<std::vector<uint64_t>> slice_keys;

	auto slice_for = [&](const Ref<TerrainSlopeLayer> &p_layer) -> int {
		if (!p_layer.is_valid()) {
			return ROCK_SLICE_NONE;
		}
		const std::vector<uint64_t> key = slope_texture_key(p_layer);
		for (size_t i = 0; i < slice_keys.size(); i++) {
			if (slice_keys[i] == key) {
				return static_cast<int>(i);
			}
		}
		slices.push_back(p_layer);
		slice_keys.push_back(key);
		return static_cast<int>(slices.size()) - 1;
	};

	_rock_fallback_slice = slice_for(p_fallback);
	for (int i = 0; i < num_biomes; i++) {
		const Ref<TerrainBiomeLayer> biome = p_biomes[i];
		_rock_biome_slice[static_cast<size_t>(i)] = slice_for(_effective_slope_layer(biome, p_fallback));
	}

	const int num_slices = static_cast<int>(slices.size());
	if (num_slices == 0) {
		return;
	}

	int albedo_w = 0, albedo_h = 0;
	int normal_w = 0, normal_h = 0;
	int roughness_w = 0, roughness_h = 0;
	int height_w = 0, height_h = 0;
	int ao_w = 0, ao_h = 0;

	for (const Ref<TerrainSlopeLayer> &layer : slices) {
		if (albedo_w == 0) {
			if (const Ref<Texture2D> t = layer->get_albedo_texture(); t.is_valid()) {
				albedo_w = t->get_width();
				albedo_h = t->get_height();
			}
		}
		if (normal_w == 0) {
			if (const Ref<Texture2D> t = layer->get_normal_texture(); t.is_valid()) {
				normal_w = t->get_width();
				normal_h = t->get_height();
			}
		}
		if (roughness_w == 0) {
			if (const Ref<Texture2D> t = layer->get_roughness_texture(); t.is_valid()) {
				roughness_w = t->get_width();
				roughness_h = t->get_height();
			}
		}
		if (height_w == 0) {
			if (const Ref<Texture2D> t = layer->get_height_texture(); t.is_valid()) {
				height_w = t->get_width();
				height_h = t->get_height();
			}
		}
		if (ao_w == 0) {
			if (const Ref<Texture2D> t = layer->get_ao_texture(); t.is_valid()) {
				ao_w = t->get_width();
				ao_h = t->get_height();
			}
		}
	}

	if (albedo_w == 0) {
		albedo_w = albedo_h = DEFAULT_DIM;
	}
	if (normal_w == 0) {
		normal_w = normal_h = DEFAULT_DIM;
	}
	if (roughness_w == 0) {
		roughness_w = roughness_h = DEFAULT_DIM;
	}
	if (height_w == 0) {
		height_w = height_h = DEFAULT_DIM;
	}
	if (ao_w == 0) {
		ao_w = ao_h = DEFAULT_DIM;
	}

	CRASH_COND_MSG(albedo_w > TerrainSlopeLayer::MAX_TEXTURE_DIMENSION || albedo_h > TerrainSlopeLayer::MAX_TEXTURE_DIMENSION ||
					normal_w > TerrainSlopeLayer::MAX_TEXTURE_DIMENSION || normal_h > TerrainSlopeLayer::MAX_TEXTURE_DIMENSION ||
					roughness_w > TerrainSlopeLayer::MAX_TEXTURE_DIMENSION || roughness_h > TerrainSlopeLayer::MAX_TEXTURE_DIMENSION ||
					height_w > TerrainSlopeLayer::MAX_TEXTURE_DIMENSION || height_h > TerrainSlopeLayer::MAX_TEXTURE_DIMENSION ||
					ao_w > TerrainSlopeLayer::MAX_TEXTURE_DIMENSION || ao_h > TerrainSlopeLayer::MAX_TEXTURE_DIMENSION,
			"TerrainRenderer: a slope layer texture exceeds MAX_TEXTURE_DIMENSION; TerrainSlopeLayer's setters should have rejected this already.");

	TypedArray<Ref<Image>> albedo_images, normal_images, roughness_images, height_images, ao_images;
	bool mismatch = false;

	for (int i = 0; i < num_slices; i++) {
		const Ref<TerrainSlopeLayer> &layer = slices[static_cast<size_t>(i)];

		bool slice_mismatch = false;
		albedo_images.push_back(_prepare_layer_image(layer->get_albedo_texture(), albedo_w, albedo_h, PLACEHOLDER_ALBEDO, "rock albedo", i, slice_mismatch));
		normal_images.push_back(_prepare_layer_image(layer->get_normal_texture(), normal_w, normal_h, PLACEHOLDER_NORMAL, "rock normal", i, slice_mismatch));
		roughness_images.push_back(_prepare_layer_image(layer->get_roughness_texture(), roughness_w, roughness_h, PLACEHOLDER_ROUGHNESS, "rock roughness", i, slice_mismatch));
		height_images.push_back(_prepare_layer_image(layer->get_height_texture(), height_w, height_h, PLACEHOLDER_HEIGHT, "rock height", i, slice_mismatch));
		ao_images.push_back(_prepare_layer_image(layer->get_ao_texture(), ao_w, ao_h, PLACEHOLDER_AO, "rock ao", i, slice_mismatch));

		mismatch = mismatch || slice_mismatch;
	}

	if (mismatch) {
		UtilityFunctions::push_warning("TerrainRenderer: the slope layers in use do not all share the same texture dimensions for a given texture type; disabling the rock overlay until sizes are made consistent.");
		_rock_biome_slice.assign(static_cast<size_t>(num_biomes), ROCK_SLICE_NONE);
		_rock_fallback_slice = ROCK_SLICE_NONE;
		return;
	}

	_rock_albedo_array_rid = rs->texture_2d_layered_create(albedo_images, RenderingServer::TEXTURE_LAYERED_2D_ARRAY);
	_rock_normal_array_rid = rs->texture_2d_layered_create(normal_images, RenderingServer::TEXTURE_LAYERED_2D_ARRAY);
	_rock_roughness_array_rid = rs->texture_2d_layered_create(roughness_images, RenderingServer::TEXTURE_LAYERED_2D_ARRAY);
	_rock_height_array_rid = rs->texture_2d_layered_create(height_images, RenderingServer::TEXTURE_LAYERED_2D_ARRAY);
	_rock_ao_array_rid = rs->texture_2d_layered_create(ao_images, RenderingServer::TEXTURE_LAYERED_2D_ARRAY);

	_rock_slice_count = num_slices;
}

void TerrainRenderer::rebuild_mesh(const float p_size, const int p_resolution) {
	RenderingServer *rs = RenderingServer::get_singleton();
	_free_mesh_instances();

	if (_shader_reload_pending) {
		_shader_reload_pending = false;

		if (_internal_shader_rid.is_valid()) {
			rs->free_rid(_internal_shader_rid);
			_internal_shader_rid = RID();
		}
	}

	if (!_config.is_valid()) {
		return;
	}

	if (!_internal_shader_rid.is_valid()) {
		const String shader_code = FileAccess::get_file_as_string(SHADER_PATH);
		if (shader_code.is_empty()) {
			UtilityFunctions::push_warning("TerrainServer: could not read the terrain shader at ", SHADER_PATH, "; the terrain will not be drawn.");
			return;
		}
		_internal_shader_rid = rs->shader_create();
		rs->shader_set_code(_internal_shader_rid, shader_code);
	}

	const Ref<ArrayMesh> block_mesh = TerrainGenerator::create_block_mesh(p_resolution);
	_mesh_rid = rs->mesh_create();
	rs->mesh_add_surface_from_arrays(_mesh_rid, RenderingServer::PRIMITIVE_TRIANGLES, block_mesh->surface_get_arrays(0));

	const Ref<ArrayMesh> ring_mesh = TerrainGenerator::create_ring_fixup_mesh(p_resolution);
	_mesh_ring_rid = rs->mesh_create();
	rs->mesh_add_surface_from_arrays(_mesh_ring_rid, RenderingServer::PRIMITIVE_TRIANGLES, ring_mesh->surface_get_arrays(0));

	for (int dz = 0; dz < 2; dz++) {
		for (int dx = 0; dx < 2; dx++) {
			const Ref<ArrayMesh> trim_mesh = TerrainGenerator::create_trim_mesh(p_resolution, dx, dz);
			RID &trim_rid = _mesh_trim_rids[dz * 2 + dx];
			trim_rid = rs->mesh_create();
			rs->mesh_add_surface_from_arrays(trim_rid, RenderingServer::PRIMITIVE_TRIANGLES, trim_mesh->surface_get_arrays(0));
		}
	}

	const TypedArray<TerrainBiomeLayer> biome_layers = _config->get_biome_layers();
	_rebuild_biome_texture_arrays_if_dirty(biome_layers);

	const Ref<TerrainSlopeLayer> rock_layer = _config->get_rock_layer();
	_rebuild_rock_texture_arrays_if_dirty(biome_layers, rock_layer);

	PackedFloat32Array biome_min_temperature;
	PackedFloat32Array biome_max_temperature;
	PackedFloat32Array biome_min_moisture;
	PackedFloat32Array biome_max_moisture;
	PackedFloat32Array biome_uv_scale;
	PackedFloat32Array biome_pom_depth;
	PackedFloat32Array biome_blend_softness;
	PackedFloat32Array biome_slope_threshold;
	PackedFloat32Array biome_slope_blend_range;
	PackedInt32Array biome_rock_index;
	PackedFloat32Array biome_rock_uv_scale;
	PackedFloat32Array biome_rock_pom_depth;

	biome_min_temperature.resize(_biome_layer_count);
	biome_max_temperature.resize(_biome_layer_count);
	biome_min_moisture.resize(_biome_layer_count);
	biome_max_moisture.resize(_biome_layer_count);
	biome_uv_scale.resize(_biome_layer_count);
	biome_pom_depth.resize(_biome_layer_count);
	biome_blend_softness.resize(_biome_layer_count);
	biome_slope_threshold.resize(_biome_layer_count);
	biome_slope_blend_range.resize(_biome_layer_count);
	biome_rock_index.resize(_biome_layer_count);
	biome_rock_uv_scale.resize(_biome_layer_count);
	biome_rock_pom_depth.resize(_biome_layer_count);

	// Fallback slope values, for the no-biome-layers path in the shader.
	const float rock_slope_threshold = rock_layer.is_valid() ? rock_layer->get_slope_threshold() : ROCK_DISABLED_SLOPE_THRESHOLD;
	const float rock_slope_blend_range = rock_layer.is_valid() ? rock_layer->get_slope_blend_range() : ROCK_DISABLED_SLOPE_BLEND_RANGE;

	for (int i = 0; i < _biome_layer_count; i++) {
		const Ref<TerrainBiomeLayer> layer = biome_layers[i];
		biome_min_temperature[i] = layer.is_valid() ? layer->get_min_temperature() : 0.0f;
		biome_max_temperature[i] = layer.is_valid() ? layer->get_max_temperature() : 0.0f;
		biome_min_moisture[i] = layer.is_valid() ? layer->get_min_moisture() : 0.0f;
		biome_max_moisture[i] = layer.is_valid() ? layer->get_max_moisture() : 0.0f;
		biome_uv_scale[i] = layer.is_valid() ? layer->get_uv_scale() : 0.1f;
		biome_pom_depth[i] = layer.is_valid() ? layer->get_pom_depth() : 0.0f;
		biome_blend_softness[i] = layer.is_valid() ? layer->get_blend_softness() : 0.25f;

		const int slice = (i < static_cast<int>(_rock_biome_slice.size())) ? _rock_biome_slice[static_cast<size_t>(i)] : ROCK_SLICE_NONE;
		const Ref<TerrainSlopeLayer> slope = _effective_slope_layer(layer, rock_layer);
		const bool has_rock = slice != ROCK_SLICE_NONE && slope.is_valid();

		biome_rock_index[i] = has_rock ? slice : 0;
		biome_rock_uv_scale[i] = has_rock ? slope->get_uv_scale() : 0.1f;
		biome_rock_pom_depth[i] = has_rock ? slope->get_pom_depth() : 0.0f;

		const bool slope_override = layer.is_valid() && layer->get_slope_override();
		if (!has_rock) {
			biome_slope_threshold[i] = ROCK_DISABLED_SLOPE_THRESHOLD;
			biome_slope_blend_range[i] = ROCK_DISABLED_SLOPE_BLEND_RANGE;
		} else if (slope_override) {
			biome_slope_threshold[i] = layer->get_slope_threshold();
			biome_slope_blend_range[i] = layer->get_slope_blend_range();
		} else {
			biome_slope_threshold[i] = slope->get_slope_threshold();
			biome_slope_blend_range[i] = slope->get_slope_blend_range();
		}
	}

	int num_levels = _config->get_clipmap_levels();
	if (num_levels <= 0) {
		num_levels = 6;
	}

	const float half_height = static_cast<float>(_config->get_height_scale()) * HEIGHT_AABB_MARGIN;
	const AABB custom_aabb(Vector3(-0.5f, -half_height, -0.5f), Vector3(1.0f, half_height * 2.0f, 1.0f));

	for (int i = 0; i < num_levels; i++) {
		RID instance = rs->instance_create();
		RID mesh_to_use = (i == 0) ? _mesh_rid : _mesh_ring_rid;
		rs->instance_set_base(instance, mesh_to_use);
		rs->instance_set_custom_aabb(instance, custom_aabb);

		RID material = rs->material_create();
		rs->material_set_shader(material, _internal_shader_rid);

		// Physics parameters
		rs->material_set_param(material, "height_scale", static_cast<float>(_config->get_height_scale()));
		rs->material_set_param(material, "resolution", static_cast<float>(p_resolution));
		rs->material_set_param(material, "morph_band_start", MORPH_BAND_START);

		// Noise parameters
		rs->material_set_param(material, "octaves", _config->get_noise_octaves());
		rs->material_set_param(material, "lacunarity", _config->get_noise_lacunarity());
		rs->material_set_param(material, "gain", _config->get_noise_gain());
		rs->material_set_param(material, "base_frequency", _config->get_noise_base_frequency());
		rs->material_set_param(material, "ridge_amount", _config->get_noise_ridge_amount());
		rs->material_set_param(material, "ridge_offset", _config->get_noise_ridge_offset());
		rs->material_set_param(material, "ridge_weight_gain", _config->get_noise_ridge_weight_gain());
		rs->material_set_param(material, "ridge_crest_rounding", _config->get_noise_ridge_crest_rounding());
		rs->material_set_param(material, "warp_amount", _config->get_noise_warp_amount());
		rs->material_set_param(material, "warp_frequency", _config->get_noise_warp_frequency());
		rs->material_set_param(material, "continent_frequency", _config->get_noise_continent_frequency());
		rs->material_set_param(material, "continent_influence", _config->get_noise_continent_influence());
		rs->material_set_param(material, "continent_contrast", _config->get_noise_continent_contrast());
		rs->material_set_param(material, "continent_elevation", _config->get_noise_continent_elevation());
		rs->material_set_param(material, "continent_sea_level", _config->get_noise_continent_sea_level());
		rs->material_set_param(material, "relief_floor", _config->get_noise_relief_floor());
		rs->material_set_param(material, "redistribution", _config->get_noise_redistribution());

		// Biome texture arrays
		rs->material_set_param(material, "biome_albedo_textures", _biome_albedo_array_rid);
		rs->material_set_param(material, "biome_normal_textures", _biome_normal_array_rid);
		rs->material_set_param(material, "biome_roughness_textures", _biome_roughness_array_rid);
		rs->material_set_param(material, "biome_height_textures", _biome_height_array_rid);
		rs->material_set_param(material, "biome_ao_textures", _biome_ao_array_rid);
		rs->material_set_param(material, "biome_layer_count", _biome_layer_count);

		// Biome thresholds
		rs->material_set_param(material, "biome_min_temperature", biome_min_temperature);
		rs->material_set_param(material, "biome_max_temperature", biome_max_temperature);
		rs->material_set_param(material, "biome_min_moisture", biome_min_moisture);
		rs->material_set_param(material, "biome_max_moisture", biome_max_moisture);
		rs->material_set_param(material, "biome_uv_scale", biome_uv_scale);
		rs->material_set_param(material, "biome_pom_depth", biome_pom_depth);
		rs->material_set_param(material, "biome_blend_softness", biome_blend_softness);
		rs->material_set_param(material, "biome_slope_threshold", biome_slope_threshold);
		rs->material_set_param(material, "biome_slope_blend_range", biome_slope_blend_range);
		rs->material_set_param(material, "biome_rock_index", biome_rock_index);
		rs->material_set_param(material, "biome_rock_uv_scale", biome_rock_uv_scale);
		rs->material_set_param(material, "biome_rock_pom_depth", biome_rock_pom_depth);

		// Temperature/moisture noise parameters
		rs->material_set_param(material, "temperature_frequency", _config->get_temperature_frequency());
		rs->material_set_param(material, "temperature_offset", _config->get_temperature_offset());
		rs->material_set_param(material, "temperature_noise_influence", _config->get_temperature_noise_influence());
		rs->material_set_param(material, "temperature_altitude_reference", _config->get_temperature_altitude_reference());
		rs->material_set_param(material, "moisture_frequency", _config->get_moisture_frequency());
		rs->material_set_param(material, "moisture_offset", _config->get_moisture_offset());

		// Rock/cliff slope overlay
		rs->material_set_param(material, "rock_albedo_textures", _rock_albedo_array_rid);
		rs->material_set_param(material, "rock_normal_textures", _rock_normal_array_rid);
		rs->material_set_param(material, "rock_roughness_textures", _rock_roughness_array_rid);
		rs->material_set_param(material, "rock_height_textures", _rock_height_array_rid);
		rs->material_set_param(material, "rock_ao_textures", _rock_ao_array_rid);
		rs->material_set_param(material, "rock_layer_enabled", _rock_slice_count > 0);
		rs->material_set_param(material, "rock_index", _rock_fallback_slice != ROCK_SLICE_NONE ? _rock_fallback_slice : 0);
		rs->material_set_param(material, "rock_slope_threshold", _rock_fallback_slice != ROCK_SLICE_NONE ? rock_slope_threshold : ROCK_DISABLED_SLOPE_THRESHOLD);
		rs->material_set_param(material, "rock_slope_blend_range", _rock_fallback_slice != ROCK_SLICE_NONE ? rock_slope_blend_range : ROCK_DISABLED_SLOPE_BLEND_RANGE);
		rs->material_set_param(material, "rock_uv_scale", rock_layer.is_valid() ? rock_layer->get_uv_scale() : 0.1f);
		rs->material_set_param(material, "rock_pom_depth", rock_layer.is_valid() ? rock_layer->get_pom_depth() : 0.0f);

		// Parallax cost budget (shared by the single raymarch over the blended height field)
		rs->material_set_param(material, "pom_min_steps", _config->get_pom_min_steps());
		rs->material_set_param(material, "pom_max_steps", _config->get_pom_max_steps());
		rs->material_set_param(material, "pom_fade_start", _config->get_pom_fade_start());
		rs->material_set_param(material, "pom_fade_end", _config->get_pom_fade_end());
		rs->material_set_param(material, "triplanar_sharpness", _config->get_triplanar_sharpness());

		rs->material_set_param(material, "debug_view", _debug_view);

		const float level_scale = p_size * powf(2.0f, static_cast<float>(i));
		rs->instance_geometry_set_material_override(instance, material);

		if (_parent_node && _parent_node->is_inside_tree()) {
			rs->instance_set_scenario(instance, _parent_node->get_world_3d()->get_scenario());
		}

		ClipmapLevel level;
		level.instance_rid = instance;
		level.material_rid = material;
		level.scale = level_scale;

		if (i > 0) {
			level.trim_instance_rid = rs->instance_create();
			rs->instance_set_custom_aabb(level.trim_instance_rid, custom_aabb);
			rs->instance_geometry_set_material_override(level.trim_instance_rid, material);

			if (_parent_node && _parent_node->is_inside_tree()) {
				rs->instance_set_scenario(level.trim_instance_rid, _parent_node->get_world_3d()->get_scenario());
			}
		}

		_clipmap_levels.push_back(level);
	}
}

void TerrainRenderer::update_focus_position(const Vector3 p_focus_pos) {
	RenderingServer *rs = RenderingServer::get_singleton();

	if (!_config.is_valid() || _clipmap_levels.empty()) {
		return;
	}

	int resolution = _config->get_mesh_resolution();
	if (resolution <= 0) {
		resolution = 64;
	}

	for (size_t i = 0; i < _clipmap_levels.size(); i++) {
		ClipmapLevel &level = _clipmap_levels[i];

		const float cell = level.scale / static_cast<float>(resolution);
		const float step = 2.0f * cell;
		level.origin = Vector2(floorf(p_focus_pos.x / step) * step, floorf(p_focus_pos.z / step) * step);

		Transform3D xform;
		xform.basis = xform.basis.scaled(Vector3(level.scale, 1.0, level.scale));
		xform.origin = Vector3(level.origin.x, 0.0f, level.origin.y);

		rs->instance_set_transform(level.instance_rid, xform);

		if (!level.trim_instance_rid.is_valid()) {
			continue;
		}

		const Vector2 delta = _clipmap_levels[i - 1].origin - level.origin;
		const int dx = static_cast<int>(roundf(delta.x / cell)) & 1;
		const int dz = static_cast<int>(roundf(delta.y / cell)) & 1;
		const int variant = dz * 2 + dx;

		if (level.trim_variant != variant) {
			level.trim_variant = variant;
			rs->instance_set_base(level.trim_instance_rid, _mesh_trim_rids[variant]);
		}

		rs->instance_set_transform(level.trim_instance_rid, xform);
	}
}

// ============== Getters and setters ==============

void TerrainRenderer::set_configuration(const Ref<TerrainConfiguration> &p_config) {
	_config = p_config;
}

void TerrainRenderer::request_shader_reload() {
	_shader_reload_pending = true;
}

void TerrainRenderer::set_debug_view(const int p_debug_view) {
	_debug_view = p_debug_view;

	RenderingServer *rs = RenderingServer::get_singleton();
	for (const auto &level : _clipmap_levels) {
		rs->material_set_param(level.material_rid, "debug_view", _debug_view);
	}
}

int TerrainRenderer::get_debug_view() const {
	return _debug_view;
}

int TerrainRenderer::get_clipmap_level_count() const {
	return static_cast<int>(_clipmap_levels.size());
}

float TerrainRenderer::get_clipmap_level_scale(const int p_level) const {
	ERR_FAIL_INDEX_V(p_level, static_cast<int>(_clipmap_levels.size()), 0.0f);

	return _clipmap_levels[p_level].scale;
}

Vector2 TerrainRenderer::get_snapped_focus_xz() const {
	if (_clipmap_levels.empty()) {
		return {};
	}

	return _clipmap_levels[0].origin;
}

Vector2 TerrainRenderer::get_clipmap_level_origin(const int p_level) const {
	ERR_FAIL_INDEX_V(p_level, static_cast<int>(_clipmap_levels.size()), Vector2());

	return _clipmap_levels[p_level].origin;
}

} //namespace ts