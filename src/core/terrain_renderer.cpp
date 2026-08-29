#include "terrain_renderer.h"

#include "terrain_generator.h"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector3.hpp>

using namespace godot;

namespace ts {

constexpr float HEIGHT_AABB_MARGIN = 1.25f;

namespace {
void free_rid_if_valid(RenderingServer *p_rs, RID &p_rid) {
	if (p_rid.is_valid()) {
		p_rs->free_rid(p_rid);
		p_rid = RID();
	}
}
} //namespace

void TerrainRenderer::_bind_methods() {}

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
}

void TerrainRenderer::cleanup() {
	_free_mesh_instances();
	_free_biome_texture_arrays();

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

	_biome_texture_signature.clear();
	_biome_texture_arrays_built = false;
	_biome_layer_count = 0;
}

std::vector<uint64_t> TerrainRenderer::_compute_biome_texture_signature(const TypedArray<TerrainBiomeLayer> &p_layers) const {
	std::vector<uint64_t> signature;
	signature.reserve(static_cast<size_t>(p_layers.size()) * 3);

	for (int i = 0; i < p_layers.size(); i++) {
		const Ref<TerrainBiomeLayer> layer = p_layers[i];
		if (!layer.is_valid()) {
			signature.push_back(0);
			signature.push_back(0);
			signature.push_back(0);
			continue;
		}

		const Ref<Texture2D> albedo = layer->get_albedo_texture();
		const Ref<Texture2D> normal = layer->get_normal_texture();
		const Ref<Texture2D> roughness = layer->get_roughness_texture();
		signature.push_back(albedo.is_valid() ? albedo->get_instance_id() : 0);
		signature.push_back(normal.is_valid() ? normal->get_instance_id() : 0);
		signature.push_back(roughness.is_valid() ? roughness->get_instance_id() : 0);
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
		return 0;
	}

	int albedo_w = 0, albedo_h = 0;
	int normal_w = 0, normal_h = 0;
	int roughness_w = 0, roughness_h = 0;

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
	}

	constexpr int DEFAULT_DIM = 4;
	if (albedo_w == 0) {
		albedo_w = albedo_h = DEFAULT_DIM;
	}
	if (normal_w == 0) {
		normal_w = normal_h = DEFAULT_DIM;
	}
	if (roughness_w == 0) {
		roughness_w = roughness_h = DEFAULT_DIM;
	}

	// Already enforced upstream by TerrainBiomeLayer's setters (reject textures larger than
	// MAX_TEXTURE_DIMENSION) -- reaching this point with an oversized reference is a bug.
	CRASH_COND_MSG(albedo_w > TerrainBiomeLayer::MAX_TEXTURE_DIMENSION || albedo_h > TerrainBiomeLayer::MAX_TEXTURE_DIMENSION ||
					normal_w > TerrainBiomeLayer::MAX_TEXTURE_DIMENSION || normal_h > TerrainBiomeLayer::MAX_TEXTURE_DIMENSION ||
					roughness_w > TerrainBiomeLayer::MAX_TEXTURE_DIMENSION || roughness_h > TerrainBiomeLayer::MAX_TEXTURE_DIMENSION,
			"TerrainRenderer: a biome layer texture exceeds MAX_TEXTURE_DIMENSION; TerrainBiomeLayer's setters should have rejected this already.");

	TypedArray<Ref<Image>> albedo_images, normal_images, roughness_images;
	bool mismatch = false;

	for (int i = 0; i < num_layers; i++) {
		const Ref<TerrainBiomeLayer> layer = p_layers[i];
		const Ref<Texture2D> albedo_tex = layer.is_valid() ? layer->get_albedo_texture() : Ref<Texture2D>();
		const Ref<Texture2D> normal_tex = layer.is_valid() ? layer->get_normal_texture() : Ref<Texture2D>();
		const Ref<Texture2D> roughness_tex = layer.is_valid() ? layer->get_roughness_texture() : Ref<Texture2D>();

		bool layer_mismatch = false;
		Ref<Image> a = _prepare_layer_image(albedo_tex, albedo_w, albedo_h, Color(0.5f, 0.5f, 0.5f, 1.0f), "albedo", i, layer_mismatch);
		Ref<Image> n = _prepare_layer_image(normal_tex, normal_w, normal_h, Color(0.5f, 0.5f, 1.0f, 1.0f), "normal", i, layer_mismatch);
		Ref<Image> r = _prepare_layer_image(roughness_tex, roughness_w, roughness_h, Color(0.5f, 0.5f, 0.5f, 1.0f), "roughness", i, layer_mismatch);

		mismatch = mismatch || layer_mismatch;
		albedo_images.push_back(a);
		normal_images.push_back(n);
		roughness_images.push_back(r);
	}

	if (mismatch) {
		UtilityFunctions::push_warning("TerrainRenderer: one or more biome layer textures do not match the first configured layer's dimensions for their texture type; falling back to the zero-layers debug render until sizes are made consistent.");
		free_rid_if_valid(rs, _biome_albedo_array_rid);
		free_rid_if_valid(rs, _biome_normal_array_rid);
		free_rid_if_valid(rs, _biome_roughness_array_rid);
		return 0;
	}

	const RID new_albedo_rid = rs->texture_2d_layered_create(albedo_images, RenderingServer::TEXTURE_LAYERED_2D_ARRAY);
	const RID new_normal_rid = rs->texture_2d_layered_create(normal_images, RenderingServer::TEXTURE_LAYERED_2D_ARRAY);
	const RID new_roughness_rid = rs->texture_2d_layered_create(roughness_images, RenderingServer::TEXTURE_LAYERED_2D_ARRAY);

	free_rid_if_valid(rs, _biome_albedo_array_rid);
	free_rid_if_valid(rs, _biome_normal_array_rid);
	free_rid_if_valid(rs, _biome_roughness_array_rid);

	_biome_albedo_array_rid = new_albedo_rid;
	_biome_normal_array_rid = new_normal_rid;
	_biome_roughness_array_rid = new_roughness_rid;

	return num_layers;
}

void TerrainRenderer::rebuild_mesh(const float p_size, const int p_resolution) {
	RenderingServer *rs = RenderingServer::get_singleton();
	_free_mesh_instances();

	if (!_config.is_valid()) {
		return;
	}

	if (!_internal_shader_rid.is_valid()) {
		const String shader_code = FileAccess::get_file_as_string("res://addons/terrain_server/shaders/terrain.gdshader");
		if (shader_code.is_empty()) {
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

	const TypedArray<TerrainBiomeLayer> biome_layers = _config->get_biome_layers();
	_rebuild_biome_texture_arrays_if_dirty(biome_layers);

	PackedFloat32Array biome_min_temperature;
	PackedFloat32Array biome_max_temperature;
	PackedFloat32Array biome_min_moisture;
	PackedFloat32Array biome_max_moisture;

	biome_min_temperature.resize(_biome_layer_count);
	biome_max_temperature.resize(_biome_layer_count);
	biome_min_moisture.resize(_biome_layer_count);
	biome_max_moisture.resize(_biome_layer_count);

	for (int i = 0; i < _biome_layer_count; i++) {
		const Ref<TerrainBiomeLayer> layer = biome_layers[i];
		biome_min_temperature[i] = layer.is_valid() ? layer->get_min_temperature() : 0.0f;
		biome_max_temperature[i] = layer.is_valid() ? layer->get_max_temperature() : 0.0f;
		biome_min_moisture[i] = layer.is_valid() ? layer->get_min_moisture() : 0.0f;
		biome_max_moisture[i] = layer.is_valid() ? layer->get_max_moisture() : 0.0f;
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

		// Noise parameters
		rs->material_set_param(material, "octaves", _config->get_noise_octaves());
		rs->material_set_param(material, "lacunarity", _config->get_noise_lacunarity());
		rs->material_set_param(material, "gain", _config->get_noise_gain());
		rs->material_set_param(material, "base_frequency", _config->get_noise_base_frequency());

		// Biome texture arrays
		rs->material_set_param(material, "biome_albedo_textures", _biome_albedo_array_rid);
		rs->material_set_param(material, "biome_normal_textures", _biome_normal_array_rid);
		rs->material_set_param(material, "biome_roughness_textures", _biome_roughness_array_rid);
		rs->material_set_param(material, "biome_layer_count", _biome_layer_count);

		// Biome thresholds
		rs->material_set_param(material, "biome_min_temperature", biome_min_temperature);
		rs->material_set_param(material, "biome_max_temperature", biome_max_temperature);
		rs->material_set_param(material, "biome_min_moisture", biome_min_moisture);
		rs->material_set_param(material, "biome_max_moisture", biome_max_moisture);

		// Temperature/moisture noise parameters
		rs->material_set_param(material, "temperature_frequency", _config->get_temperature_frequency());
		rs->material_set_param(material, "temperature_offset", _config->get_temperature_offset());
		rs->material_set_param(material, "temperature_noise_influence", _config->get_temperature_noise_influence());
		rs->material_set_param(material, "temperature_altitude_reference", _config->get_temperature_altitude_reference());
		rs->material_set_param(material, "moisture_frequency", _config->get_moisture_frequency());
		rs->material_set_param(material, "moisture_offset", _config->get_moisture_offset());

		const float level_scale = p_size * powf(2.0f, static_cast<float>(i));
		rs->instance_geometry_set_material_override(instance, material);

		if (_parent_node && _parent_node->is_inside_tree()) {
			rs->instance_set_scenario(instance, _parent_node->get_world_3d()->get_scenario());
		}

		ClipmapLevel level;
		level.instance_rid = instance;
		level.material_rid = material;
		level.scale = level_scale;

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

	// Shared snapping
	// We use the finest grid's resolution to snap ALL levels in unison.
	// This prevents the rings from sliding and misaligning.
	const float base_cell_size = _clipmap_levels[0].scale / static_cast<float>(resolution);
	const float snapped_x = floorf(p_focus_pos.x / base_cell_size) * base_cell_size;
	const float snapped_z = floorf(p_focus_pos.z / base_cell_size) * base_cell_size;

	for (const auto &level : _clipmap_levels) {
		Transform3D xform;
		xform.basis = xform.basis.scaled(Vector3(level.scale, 1.0, level.scale));
		xform.origin = Vector3(snapped_x, 0.0f, snapped_z);

		rs->instance_set_transform(level.instance_rid, xform);
	}
}

// ============== Getters and setters ==============

void TerrainRenderer::set_configuration(const Ref<TerrainConfiguration> &p_config) {
	_config = p_config;
}

} //namespace ts