#include "terrain_configuration.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <limits>
#include <vector>

using namespace godot;

namespace ts {

void TerrainConfiguration::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_terrain_size", "size"), &TerrainConfiguration::set_terrain_size);
	ClassDB::bind_method(D_METHOD("get_terrain_size"), &TerrainConfiguration::get_terrain_size);

	ClassDB::bind_method(D_METHOD("set_height_scale", "height_scale"), &TerrainConfiguration::set_height_scale);
	ClassDB::bind_method(D_METHOD("get_height_scale"), &TerrainConfiguration::get_height_scale);

	ClassDB::bind_method(D_METHOD("set_mesh_resolution", "resolution"), &TerrainConfiguration::set_mesh_resolution);
	ClassDB::bind_method(D_METHOD("get_mesh_resolution"), &TerrainConfiguration::get_mesh_resolution);

	ClassDB::bind_method(D_METHOD("set_clipmap_levels", "levels"), &TerrainConfiguration::set_clipmap_levels);
	ClassDB::bind_method(D_METHOD("get_clipmap_levels"), &TerrainConfiguration::get_clipmap_levels);

	ClassDB::bind_method(D_METHOD("set_noise_octaves", "octaves"), &TerrainConfiguration::set_noise_octaves);
	ClassDB::bind_method(D_METHOD("get_noise_octaves"), &TerrainConfiguration::get_noise_octaves);

	ClassDB::bind_method(D_METHOD("set_noise_base_frequency", "base_frequency"), &TerrainConfiguration::set_noise_base_frequency);
	ClassDB::bind_method(D_METHOD("get_noise_base_frequency"), &TerrainConfiguration::get_noise_base_frequency);

	ClassDB::bind_method(D_METHOD("set_noise_lacunarity", "lacunarity"), &TerrainConfiguration::set_noise_lacunarity);
	ClassDB::bind_method(D_METHOD("get_noise_lacunarity"), &TerrainConfiguration::get_noise_lacunarity);

	ClassDB::bind_method(D_METHOD("set_noise_gain", "gain"), &TerrainConfiguration::set_noise_gain);
	ClassDB::bind_method(D_METHOD("get_noise_gain"), &TerrainConfiguration::get_noise_gain);

	ClassDB::bind_method(D_METHOD("set_noise_ridge_amount", "amount"), &TerrainConfiguration::set_noise_ridge_amount);
	ClassDB::bind_method(D_METHOD("get_noise_ridge_amount"), &TerrainConfiguration::get_noise_ridge_amount);

	ClassDB::bind_method(D_METHOD("set_noise_ridge_offset", "offset"), &TerrainConfiguration::set_noise_ridge_offset);
	ClassDB::bind_method(D_METHOD("get_noise_ridge_offset"), &TerrainConfiguration::get_noise_ridge_offset);

	ClassDB::bind_method(D_METHOD("set_noise_ridge_weight_gain", "weight_gain"), &TerrainConfiguration::set_noise_ridge_weight_gain);
	ClassDB::bind_method(D_METHOD("get_noise_ridge_weight_gain"), &TerrainConfiguration::get_noise_ridge_weight_gain);

	ClassDB::bind_method(D_METHOD("set_noise_warp_amount", "amount"), &TerrainConfiguration::set_noise_warp_amount);
	ClassDB::bind_method(D_METHOD("get_noise_warp_amount"), &TerrainConfiguration::get_noise_warp_amount);

	ClassDB::bind_method(D_METHOD("set_noise_warp_frequency", "frequency"), &TerrainConfiguration::set_noise_warp_frequency);
	ClassDB::bind_method(D_METHOD("get_noise_warp_frequency"), &TerrainConfiguration::get_noise_warp_frequency);

	ClassDB::bind_method(D_METHOD("set_noise_continent_frequency", "frequency"), &TerrainConfiguration::set_noise_continent_frequency);
	ClassDB::bind_method(D_METHOD("get_noise_continent_frequency"), &TerrainConfiguration::get_noise_continent_frequency);

	ClassDB::bind_method(D_METHOD("set_noise_continent_influence", "influence"), &TerrainConfiguration::set_noise_continent_influence);
	ClassDB::bind_method(D_METHOD("get_noise_continent_influence"), &TerrainConfiguration::get_noise_continent_influence);

	ClassDB::bind_method(D_METHOD("set_noise_continent_contrast", "contrast"), &TerrainConfiguration::set_noise_continent_contrast);
	ClassDB::bind_method(D_METHOD("get_noise_continent_contrast"), &TerrainConfiguration::get_noise_continent_contrast);

	ClassDB::bind_method(D_METHOD("set_noise_continent_elevation", "elevation"), &TerrainConfiguration::set_noise_continent_elevation);
	ClassDB::bind_method(D_METHOD("get_noise_continent_elevation"), &TerrainConfiguration::get_noise_continent_elevation);

	ClassDB::bind_method(D_METHOD("set_noise_continent_sea_level", "sea_level"), &TerrainConfiguration::set_noise_continent_sea_level);
	ClassDB::bind_method(D_METHOD("get_noise_continent_sea_level"), &TerrainConfiguration::get_noise_continent_sea_level);

	ClassDB::bind_method(D_METHOD("set_noise_relief_floor", "floor"), &TerrainConfiguration::set_noise_relief_floor);
	ClassDB::bind_method(D_METHOD("get_noise_relief_floor"), &TerrainConfiguration::get_noise_relief_floor);

	ClassDB::bind_method(D_METHOD("set_noise_redistribution", "redistribution"), &TerrainConfiguration::set_noise_redistribution);
	ClassDB::bind_method(D_METHOD("get_noise_redistribution"), &TerrainConfiguration::get_noise_redistribution);

	ClassDB::bind_method(D_METHOD("get_noise_preview"), &TerrainConfiguration::get_noise_preview);

	ClassDB::bind_method(D_METHOD("set_physics_range", "range"), &TerrainConfiguration::set_physics_range);
	ClassDB::bind_method(D_METHOD("get_physics_range"), &TerrainConfiguration::get_physics_range);

	ClassDB::bind_method(D_METHOD("set_physics_collision_layer", "layer"), &TerrainConfiguration::set_physics_collision_layer);
	ClassDB::bind_method(D_METHOD("get_physics_collision_layer"), &TerrainConfiguration::get_physics_collision_layer);

	ClassDB::bind_method(D_METHOD("set_physics_collision_mask", "mask"), &TerrainConfiguration::set_physics_collision_mask);
	ClassDB::bind_method(D_METHOD("get_physics_collision_mask"), &TerrainConfiguration::get_physics_collision_mask);

	ClassDB::bind_method(D_METHOD("set_biome_layers", "layers"), &TerrainConfiguration::set_biome_layers);
	ClassDB::bind_method(D_METHOD("get_biome_layers"), &TerrainConfiguration::get_biome_layers);

	ClassDB::bind_method(D_METHOD("set_rock_layer", "layer"), &TerrainConfiguration::set_rock_layer);
	ClassDB::bind_method(D_METHOD("get_rock_layer"), &TerrainConfiguration::get_rock_layer);

	ClassDB::bind_method(D_METHOD("set_temperature_frequency", "frequency"), &TerrainConfiguration::set_temperature_frequency);
	ClassDB::bind_method(D_METHOD("get_temperature_frequency"), &TerrainConfiguration::get_temperature_frequency);

	ClassDB::bind_method(D_METHOD("set_temperature_offset", "offset"), &TerrainConfiguration::set_temperature_offset);
	ClassDB::bind_method(D_METHOD("get_temperature_offset"), &TerrainConfiguration::get_temperature_offset);

	ClassDB::bind_method(D_METHOD("set_temperature_noise_influence", "influence"), &TerrainConfiguration::set_temperature_noise_influence);
	ClassDB::bind_method(D_METHOD("get_temperature_noise_influence"), &TerrainConfiguration::get_temperature_noise_influence);

	ClassDB::bind_method(D_METHOD("set_temperature_altitude_reference", "reference"), &TerrainConfiguration::set_temperature_altitude_reference);
	ClassDB::bind_method(D_METHOD("get_temperature_altitude_reference"), &TerrainConfiguration::get_temperature_altitude_reference);

	ClassDB::bind_method(D_METHOD("set_moisture_frequency", "frequency"), &TerrainConfiguration::set_moisture_frequency);
	ClassDB::bind_method(D_METHOD("get_moisture_frequency"), &TerrainConfiguration::get_moisture_frequency);

	ClassDB::bind_method(D_METHOD("set_moisture_offset", "offset"), &TerrainConfiguration::set_moisture_offset);
	ClassDB::bind_method(D_METHOD("get_moisture_offset"), &TerrainConfiguration::get_moisture_offset);

	ClassDB::bind_method(D_METHOD("set_pom_min_steps", "steps"), &TerrainConfiguration::set_pom_min_steps);
	ClassDB::bind_method(D_METHOD("get_pom_min_steps"), &TerrainConfiguration::get_pom_min_steps);

	ClassDB::bind_method(D_METHOD("set_pom_max_steps", "steps"), &TerrainConfiguration::set_pom_max_steps);
	ClassDB::bind_method(D_METHOD("get_pom_max_steps"), &TerrainConfiguration::get_pom_max_steps);

	ClassDB::bind_method(D_METHOD("set_pom_fade_start", "distance"), &TerrainConfiguration::set_pom_fade_start);
	ClassDB::bind_method(D_METHOD("get_pom_fade_start"), &TerrainConfiguration::get_pom_fade_start);

	ClassDB::bind_method(D_METHOD("set_pom_fade_end", "distance"), &TerrainConfiguration::set_pom_fade_end);
	ClassDB::bind_method(D_METHOD("get_pom_fade_end"), &TerrainConfiguration::get_pom_fade_end);

	ClassDB::bind_method(D_METHOD("set_triplanar_sharpness", "sharpness"), &TerrainConfiguration::set_triplanar_sharpness);
	ClassDB::bind_method(D_METHOD("get_triplanar_sharpness"), &TerrainConfiguration::get_triplanar_sharpness);

	ADD_GROUP("Terrain", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "terrain_size", PROPERTY_HINT_RANGE, "1.0,10000.0"), "set_terrain_size", "get_terrain_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height_scale", PROPERTY_HINT_RANGE, "0.1,1000.0"), "set_height_scale", "get_height_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_resolution", PROPERTY_HINT_RANGE, "1,512"), "set_mesh_resolution", "get_mesh_resolution");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "clipmap_levels", PROPERTY_HINT_RANGE, "1,10"), "set_clipmap_levels", "get_clipmap_levels");

	ADD_GROUP("Noise", "noise_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "noise_octaves", PROPERTY_HINT_RANGE, "1,10"), "set_noise_octaves", "get_noise_octaves");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_base_frequency", PROPERTY_HINT_RANGE, "0.0001,0.1"), "set_noise_base_frequency", "get_noise_base_frequency");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_lacunarity", PROPERTY_HINT_RANGE, "1.0,5.0"), "set_noise_lacunarity", "get_noise_lacunarity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_gain", PROPERTY_HINT_RANGE, "0.0,1.0"), "set_noise_gain", "get_noise_gain");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_ridge_amount", PROPERTY_HINT_RANGE, "0.0,1.0"), "set_noise_ridge_amount", "get_noise_ridge_amount");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_ridge_offset", PROPERTY_HINT_RANGE, "0.5,1.5"), "set_noise_ridge_offset", "get_noise_ridge_offset");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_ridge_weight_gain", PROPERTY_HINT_RANGE, "0.0,3.0"), "set_noise_ridge_weight_gain", "get_noise_ridge_weight_gain");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_warp_amount", PROPERTY_HINT_RANGE, "0.0,500.0"), "set_noise_warp_amount", "get_noise_warp_amount");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_warp_frequency", PROPERTY_HINT_RANGE, "0.0001,0.01"), "set_noise_warp_frequency", "get_noise_warp_frequency");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_continent_frequency", PROPERTY_HINT_RANGE, "0.00005,0.005"), "set_noise_continent_frequency", "get_noise_continent_frequency");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_continent_influence", PROPERTY_HINT_RANGE, "0.0,1.0"), "set_noise_continent_influence", "get_noise_continent_influence");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_continent_contrast", PROPERTY_HINT_RANGE, "0.01,0.5"), "set_noise_continent_contrast", "get_noise_continent_contrast");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_continent_elevation", PROPERTY_HINT_RANGE, "0.0,1.0"), "set_noise_continent_elevation", "get_noise_continent_elevation");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_continent_sea_level", PROPERTY_HINT_RANGE, "0.0,1.0"), "set_noise_continent_sea_level", "get_noise_continent_sea_level");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_relief_floor", PROPERTY_HINT_RANGE, "0.0,1.0"), "set_noise_relief_floor", "get_noise_relief_floor");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_redistribution", PROPERTY_HINT_RANGE, "0.5,3.0"), "set_noise_redistribution", "get_noise_redistribution");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise_preview", PROPERTY_HINT_RESOURCE_TYPE, "ImageTexture", PROPERTY_USAGE_EDITOR), "", "get_noise_preview");

	ADD_GROUP("Physics", "physics_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "physics_range", PROPERTY_HINT_RANGE, "8.0,512.0"), "set_physics_range", "get_physics_range");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "physics_collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_physics_collision_layer", "get_physics_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "physics_collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_physics_collision_mask", "get_physics_collision_mask");

	ADD_GROUP("Biome", "");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "biome_layers", PROPERTY_HINT_TYPE_STRING,
						 vformat("%d/%d:%s", Variant::OBJECT, PROPERTY_HINT_RESOURCE_TYPE, "TerrainBiomeLayer")),
			"set_biome_layers", "get_biome_layers");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "rock_layer", PROPERTY_HINT_RESOURCE_TYPE, "TerrainSlopeLayer"), "set_rock_layer", "get_rock_layer");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "temperature_frequency", PROPERTY_HINT_RANGE, "0.0001,0.01"), "set_temperature_frequency", "get_temperature_frequency");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "temperature_offset"), "set_temperature_offset", "get_temperature_offset");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "temperature_noise_influence", PROPERTY_HINT_RANGE, "0.0,1.0"), "set_temperature_noise_influence", "get_temperature_noise_influence");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "temperature_altitude_reference", PROPERTY_HINT_RANGE, "0.1,100.0"), "set_temperature_altitude_reference", "get_temperature_altitude_reference");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "moisture_frequency", PROPERTY_HINT_RANGE, "0.0001,0.01"), "set_moisture_frequency", "get_moisture_frequency");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "moisture_offset"), "set_moisture_offset", "get_moisture_offset");

	ADD_GROUP("Parallax", "pom_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "pom_min_steps", PROPERTY_HINT_RANGE, "1,64,1"), "set_pom_min_steps", "get_pom_min_steps");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "pom_max_steps", PROPERTY_HINT_RANGE, "1,128,1"), "set_pom_max_steps", "get_pom_max_steps");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pom_fade_start", PROPERTY_HINT_RANGE, "0.0,1000.0,0.1,or_greater"), "set_pom_fade_start", "get_pom_fade_start");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pom_fade_end", PROPERTY_HINT_RANGE, "0.0,1000.0,0.1,or_greater"), "set_pom_fade_end", "get_pom_fade_end");

	ADD_GROUP("Triplanar", "triplanar_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "triplanar_sharpness", PROPERTY_HINT_RANGE, "1.0,16.0,0.1"), "set_triplanar_sharpness", "get_triplanar_sharpness");
}

TerrainConfiguration::TerrainConfiguration() : _height_scale(10.0), _mesh_resolution(256), _terrain_size(256.0f), _noise_octaves(5), _noise_base_frequency(0.002f), _noise_lacunarity(2.0f), _noise_gain(0.5f), _noise_ridge_amount(0.0f), _noise_ridge_offset(1.0f), _noise_ridge_weight_gain(1.5f), _noise_warp_amount(0.0f), _noise_warp_frequency(0.001f), _noise_continent_frequency(0.00025f), _noise_continent_influence(0.0f), _noise_continent_contrast(0.2f), _noise_continent_elevation(0.0f), _noise_continent_sea_level(0.4f), _noise_relief_floor(1.0f), _noise_redistribution(1.0f), _clipmap_levels(6), _physics_range(64.0f), _physics_collision_layer(1), _physics_collision_mask(0), _temperature_frequency(0.0004f), _temperature_offset(10000.0f, -6000.0f), _temperature_noise_influence(0.4f), _temperature_altitude_reference(10.0f), _moisture_frequency(0.0006f), _moisture_offset(-4000.0f, 9000.0f), _pom_min_steps(8), _pom_max_steps(32), _pom_fade_start(40.0f), _pom_fade_end(120.0f), _triplanar_sharpness(4.0f) {
	_update_preview();
}

TerrainConfiguration::~TerrainConfiguration() = default;

TerrainNoise::FbmParams TerrainConfiguration::build_noise_params() const {
	TerrainNoise::FbmParams params;
	params.octaves = _noise_octaves;
	params.base_frequency = _noise_base_frequency;
	params.lacunarity = _noise_lacunarity;
	params.gain = _noise_gain;
	params.height_scale = static_cast<float>(_height_scale);
	params.ridge_amount = _noise_ridge_amount;
	params.ridge_offset = _noise_ridge_offset;
	params.ridge_weight_gain = _noise_ridge_weight_gain;
	params.warp_amount = _noise_warp_amount;
	params.warp_frequency = _noise_warp_frequency;
	params.continent_frequency = _noise_continent_frequency;
	params.continent_influence = _noise_continent_influence;
	params.continent_contrast = _noise_continent_contrast;
	params.continent_elevation = _noise_continent_elevation;
	params.continent_sea_level = _noise_continent_sea_level;
	params.relief_floor = _noise_relief_floor;
	params.redistribution = _noise_redistribution;
	return params;
}

void TerrainConfiguration::_update_preview() {
	TerrainNoise::FbmParams params = build_noise_params();
	params.height_scale = 1.0f; // Normalized below, so the vertical scale is irrelevant here.

	float extent = PREVIEW_BASE_WAVELENGTHS / std::max(_noise_base_frequency, 1e-6f);
	if (_noise_continent_influence > 0.0f) {
		extent = std::max(extent, PREVIEW_CONTINENT_WAVELENGTHS / std::max(_noise_continent_frequency, 1e-6f));
	}

	PackedByteArray pixels;
	pixels.resize(PREVIEW_RESOLUTION * PREVIEW_RESOLUTION);
	uint8_t *pixel_write = pixels.ptrw();

	std::vector<float> heights(static_cast<size_t>(PREVIEW_RESOLUTION) * PREVIEW_RESOLUTION);
	const float step = extent / static_cast<float>(PREVIEW_RESOLUTION - 1);
	const float origin = -extent * 0.5f;

	float lowest = std::numeric_limits<float>::max();
	float highest = std::numeric_limits<float>::lowest();

	for (int y = 0; y < PREVIEW_RESOLUTION; y++) {
		for (int x = 0; x < PREVIEW_RESOLUTION; x++) {
			const Vector2 world_xz(origin + static_cast<float>(x) * step, origin + static_cast<float>(y) * step);
			const float h = TerrainNoise::get_height_at(world_xz, params);
			heights[static_cast<size_t>(y) * PREVIEW_RESOLUTION + x] = h;
			lowest = std::min(lowest, h);
			highest = std::max(highest, h);
		}
	}

	const float span = highest - lowest;
	const float inv_span = span > 1e-6f ? 1.0f / span : 0.0f;
	for (size_t i = 0; i < heights.size(); i++) {
		const float normalized = (heights[i] - lowest) * inv_span;
		pixel_write[i] = static_cast<uint8_t>(std::clamp(normalized, 0.0f, 1.0f) * 255.0f + 0.5f);
	}

	const Ref<Image> image = Image::create_from_data(PREVIEW_RESOLUTION, PREVIEW_RESOLUTION, false, Image::FORMAT_L8, pixels);
	if (!image.is_valid()) {
		return;
	}
	_noise_preview = ImageTexture::create_from_image(image);
}

// ============== Getters and setters ==============

double TerrainConfiguration::get_height_scale() const {
	return _height_scale;
}

void TerrainConfiguration::set_height_scale(const double p_scale) {
	if (_height_scale != p_scale) {
		_height_scale = p_scale;
		emit_changed();
	}
}

int TerrainConfiguration::get_mesh_resolution() const {
	return _mesh_resolution;
}

void TerrainConfiguration::set_mesh_resolution(int p_resolution) {
	if (p_resolution <= 0) {
		p_resolution = 64;
	}

	if (_mesh_resolution != p_resolution) {
		_mesh_resolution = p_resolution;
		emit_changed();
	}
}

int TerrainConfiguration::get_noise_octaves() const {
	return _noise_octaves;
}

void TerrainConfiguration::set_noise_octaves(const int p_octaves) {
	if (_noise_octaves != p_octaves) {
		_noise_octaves = p_octaves;
		_update_preview();
		emit_changed();
	}
}

float TerrainConfiguration::get_noise_base_frequency() const {
	return _noise_base_frequency;
}

void TerrainConfiguration::set_noise_base_frequency(const float p_base_frequency) {
	if (_noise_base_frequency != p_base_frequency) {
		_noise_base_frequency = p_base_frequency;
		_update_preview();
		emit_changed();
	}
}

float TerrainConfiguration::get_noise_lacunarity() const {
	return _noise_lacunarity;
}

void TerrainConfiguration::set_noise_lacunarity(const float p_lacunarity) {
	if (_noise_lacunarity != p_lacunarity) {
		_noise_lacunarity = p_lacunarity;
		_update_preview();
		emit_changed();
	}
}

float TerrainConfiguration::get_noise_gain() const {
	return _noise_gain;
}

void TerrainConfiguration::set_noise_gain(const float p_gain) {
	if (_noise_gain != p_gain) {
		_noise_gain = p_gain;
		_update_preview();
		emit_changed();
	}
}

float TerrainConfiguration::get_noise_ridge_amount() const {
	return _noise_ridge_amount;
}

void TerrainConfiguration::set_noise_ridge_amount(const float p_amount) {
	if (_noise_ridge_amount != p_amount) {
		_noise_ridge_amount = p_amount;
		_update_preview();
		emit_changed();
	}
}

float TerrainConfiguration::get_noise_ridge_offset() const {
	return _noise_ridge_offset;
}

void TerrainConfiguration::set_noise_ridge_offset(const float p_offset) {
	if (_noise_ridge_offset != p_offset) {
		_noise_ridge_offset = p_offset;
		_update_preview();
		emit_changed();
	}
}

float TerrainConfiguration::get_noise_ridge_weight_gain() const {
	return _noise_ridge_weight_gain;
}

void TerrainConfiguration::set_noise_ridge_weight_gain(const float p_weight_gain) {
	if (_noise_ridge_weight_gain != p_weight_gain) {
		_noise_ridge_weight_gain = p_weight_gain;
		_update_preview();
		emit_changed();
	}
}

float TerrainConfiguration::get_noise_warp_amount() const {
	return _noise_warp_amount;
}

void TerrainConfiguration::set_noise_warp_amount(const float p_amount) {
	if (_noise_warp_amount != p_amount) {
		_noise_warp_amount = p_amount;
		_update_preview();
		emit_changed();
	}
}

float TerrainConfiguration::get_noise_warp_frequency() const {
	return _noise_warp_frequency;
}

void TerrainConfiguration::set_noise_warp_frequency(const float p_frequency) {
	if (_noise_warp_frequency != p_frequency) {
		_noise_warp_frequency = p_frequency;
		_update_preview();
		emit_changed();
	}
}

float TerrainConfiguration::get_noise_continent_frequency() const {
	return _noise_continent_frequency;
}

void TerrainConfiguration::set_noise_continent_frequency(const float p_frequency) {
	if (_noise_continent_frequency != p_frequency) {
		_noise_continent_frequency = p_frequency;
		_update_preview();
		emit_changed();
	}
}

float TerrainConfiguration::get_noise_continent_influence() const {
	return _noise_continent_influence;
}

void TerrainConfiguration::set_noise_continent_influence(const float p_influence) {
	if (_noise_continent_influence != p_influence) {
		_noise_continent_influence = p_influence;
		_update_preview();
		emit_changed();
	}
}

float TerrainConfiguration::get_noise_continent_contrast() const {
	return _noise_continent_contrast;
}

void TerrainConfiguration::set_noise_continent_contrast(const float p_contrast) {
	if (_noise_continent_contrast != p_contrast) {
		_noise_continent_contrast = p_contrast;
		_update_preview();
		emit_changed();
	}
}

float TerrainConfiguration::get_noise_continent_elevation() const {
	return _noise_continent_elevation;
}

void TerrainConfiguration::set_noise_continent_elevation(const float p_elevation) {
	if (_noise_continent_elevation != p_elevation) {
		_noise_continent_elevation = p_elevation;
		_update_preview();
		emit_changed();
	}
}

float TerrainConfiguration::get_noise_continent_sea_level() const {
	return _noise_continent_sea_level;
}

void TerrainConfiguration::set_noise_continent_sea_level(const float p_sea_level) {
	if (_noise_continent_sea_level != p_sea_level) {
		_noise_continent_sea_level = p_sea_level;
		_update_preview();
		emit_changed();
	}
}

float TerrainConfiguration::get_noise_relief_floor() const {
	return _noise_relief_floor;
}

void TerrainConfiguration::set_noise_relief_floor(const float p_floor) {
	if (_noise_relief_floor != p_floor) {
		_noise_relief_floor = p_floor;
		_update_preview();
		emit_changed();
	}
}

float TerrainConfiguration::get_noise_redistribution() const {
	return _noise_redistribution;
}

void TerrainConfiguration::set_noise_redistribution(const float p_redistribution) {
	if (_noise_redistribution != p_redistribution) {
		_noise_redistribution = p_redistribution;
		_update_preview();
		emit_changed();
	}
}

float TerrainConfiguration::get_terrain_size() const {
	return _terrain_size;
}

void TerrainConfiguration::set_terrain_size(const float p_size) {
	if (_terrain_size != p_size) {
		_terrain_size = p_size;
		emit_changed();
	}
}

int TerrainConfiguration::get_clipmap_levels() const {
	return _clipmap_levels;
}

void TerrainConfiguration::set_clipmap_levels(const int p_levels) {
	if (_clipmap_levels != p_levels) {
		_clipmap_levels = p_levels;
		emit_changed();
	}
}

float TerrainConfiguration::get_physics_range() const {
	return _physics_range;
}

void TerrainConfiguration::set_physics_range(const float p_range) {
	if (_physics_range != p_range) {
		_physics_range = p_range;
		emit_changed();
	}
}

int TerrainConfiguration::get_physics_collision_layer() const {
	return _physics_collision_layer;
}

void TerrainConfiguration::set_physics_collision_layer(const int p_layer) {
	if (_physics_collision_layer != p_layer) {
		_physics_collision_layer = p_layer;
		emit_changed();
	}
}

int TerrainConfiguration::get_physics_collision_mask() const {
	return _physics_collision_mask;
}

void TerrainConfiguration::set_physics_collision_mask(const int p_mask) {
	if (_physics_collision_mask != p_mask) {
		_physics_collision_mask = p_mask;
		emit_changed();
	}
}

TypedArray<TerrainBiomeLayer> TerrainConfiguration::get_biome_layers() const {
	return _biome_layers;
}

void TerrainConfiguration::set_biome_layers(const TypedArray<TerrainBiomeLayer> &p_layers) {
	TypedArray<TerrainBiomeLayer> new_layers = p_layers.duplicate();
	if (new_layers.size() > MAX_BIOME_LAYERS) {
		UtilityFunctions::push_warning(vformat("TerrainConfiguration: biome_layers has %d entries, exceeding the %d-layer limit; truncating.", new_layers.size(), MAX_BIOME_LAYERS));
		new_layers.resize(MAX_BIOME_LAYERS);
	}

	for (int i = 0; i < _biome_layers.size(); i++) {
		const Ref<TerrainBiomeLayer> layer = _biome_layers[i];
		if (layer.is_valid() && layer->is_connected("changed", Callable(this, "emit_changed"))) {
			layer->disconnect("changed", Callable(this, "emit_changed"));
		}
	}

	_biome_layers = new_layers;

	for (int i = 0; i < _biome_layers.size(); i++) {
		const Ref<TerrainBiomeLayer> layer = _biome_layers[i];
		if (layer.is_valid() && !layer->is_connected("changed", Callable(this, "emit_changed"))) {
			layer->connect("changed", Callable(this, "emit_changed"));
		}
	}

	emit_changed();
}

Ref<TerrainSlopeLayer> TerrainConfiguration::get_rock_layer() const {
	return _rock_layer;
}

void TerrainConfiguration::set_rock_layer(const Ref<TerrainSlopeLayer> &p_layer) {
	if (_rock_layer == p_layer) {
		return;
	}

	if (_rock_layer.is_valid() && _rock_layer->is_connected("changed", Callable(this, "emit_changed"))) {
		_rock_layer->disconnect("changed", Callable(this, "emit_changed"));
	}

	_rock_layer = p_layer;

	if (_rock_layer.is_valid() && !_rock_layer->is_connected("changed", Callable(this, "emit_changed"))) {
		_rock_layer->connect("changed", Callable(this, "emit_changed"));
	}

	emit_changed();
}

float TerrainConfiguration::get_temperature_frequency() const {
	return _temperature_frequency;
}

void TerrainConfiguration::set_temperature_frequency(const float p_frequency) {
	if (_temperature_frequency != p_frequency) {
		_temperature_frequency = p_frequency;
		emit_changed();
	}
}

Vector2 TerrainConfiguration::get_temperature_offset() const {
	return _temperature_offset;
}

void TerrainConfiguration::set_temperature_offset(const Vector2 p_offset) {
	if (_temperature_offset != p_offset) {
		_temperature_offset = p_offset;
		emit_changed();
	}
}

float TerrainConfiguration::get_temperature_noise_influence() const {
	return _temperature_noise_influence;
}

void TerrainConfiguration::set_temperature_noise_influence(const float p_influence) {
	if (_temperature_noise_influence != p_influence) {
		_temperature_noise_influence = p_influence;
		emit_changed();
	}
}

float TerrainConfiguration::get_temperature_altitude_reference() const {
	return _temperature_altitude_reference;
}

void TerrainConfiguration::set_temperature_altitude_reference(const float p_reference) {
	if (_temperature_altitude_reference != p_reference) {
		_temperature_altitude_reference = p_reference;
		emit_changed();
	}
}

float TerrainConfiguration::get_moisture_frequency() const {
	return _moisture_frequency;
}

void TerrainConfiguration::set_moisture_frequency(const float p_frequency) {
	if (_moisture_frequency != p_frequency) {
		_moisture_frequency = p_frequency;
		emit_changed();
	}
}

Vector2 TerrainConfiguration::get_moisture_offset() const {
	return _moisture_offset;
}

void TerrainConfiguration::set_moisture_offset(const Vector2 p_offset) {
	if (_moisture_offset != p_offset) {
		_moisture_offset = p_offset;
		emit_changed();
	}
}

int TerrainConfiguration::get_pom_min_steps() const {
	return _pom_min_steps;
}

void TerrainConfiguration::set_pom_min_steps(const int p_steps) {
	if (_pom_min_steps != p_steps) {
		_pom_min_steps = p_steps;
		emit_changed();
	}
}

int TerrainConfiguration::get_pom_max_steps() const {
	return _pom_max_steps;
}

void TerrainConfiguration::set_pom_max_steps(const int p_steps) {
	if (_pom_max_steps != p_steps) {
		_pom_max_steps = p_steps;
		emit_changed();
	}
}

float TerrainConfiguration::get_pom_fade_start() const {
	return _pom_fade_start;
}

void TerrainConfiguration::set_pom_fade_start(const float p_distance) {
	if (_pom_fade_start != p_distance) {
		_pom_fade_start = p_distance;
		emit_changed();
	}
}

float TerrainConfiguration::get_pom_fade_end() const {
	return _pom_fade_end;
}

void TerrainConfiguration::set_pom_fade_end(const float p_distance) {
	if (_pom_fade_end != p_distance) {
		_pom_fade_end = p_distance;
		emit_changed();
	}
}

float TerrainConfiguration::get_triplanar_sharpness() const {
	return _triplanar_sharpness;
}

void TerrainConfiguration::set_triplanar_sharpness(const float p_sharpness) {
	if (_triplanar_sharpness != p_sharpness) {
		_triplanar_sharpness = p_sharpness;
		emit_changed();
	}
}

Ref<ImageTexture> TerrainConfiguration::get_noise_preview() const {
	return _noise_preview;
}

} //namespace ts
