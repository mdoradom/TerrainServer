#include "terrain_configuration.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

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

	ADD_GROUP("Terrain", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "terrain_size", PROPERTY_HINT_RANGE, "1.0,10000.0"), "set_terrain_size", "get_terrain_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height_scale", PROPERTY_HINT_RANGE, "0.1,100.0"), "set_height_scale", "get_height_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_resolution", PROPERTY_HINT_RANGE, "1,512"), "set_mesh_resolution", "get_mesh_resolution");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "clipmap_levels", PROPERTY_HINT_RANGE, "1,10"), "set_clipmap_levels", "get_clipmap_levels");

	ADD_GROUP("Noise", "noise_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "noise_octaves", PROPERTY_HINT_RANGE, "1,10"), "set_noise_octaves", "get_noise_octaves");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_base_frequency", PROPERTY_HINT_RANGE, "0.0001,0.1"), "set_noise_base_frequency", "get_noise_base_frequency");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_lacunarity", PROPERTY_HINT_RANGE, "1.0,5.0"), "set_noise_lacunarity", "get_noise_lacunarity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_gain", PROPERTY_HINT_RANGE, "0.0,1.0"), "set_noise_gain", "get_noise_gain");
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
}

TerrainConfiguration::TerrainConfiguration() : _height_scale(10.0), _mesh_resolution(256), _terrain_size(256.0f), _noise_octaves(5), _noise_base_frequency(0.002f), _noise_lacunarity(2.0f), _noise_gain(0.5f), _clipmap_levels(6), _physics_range(64.0f), _physics_collision_layer(1), _physics_collision_mask(0), _temperature_frequency(0.0004f), _temperature_offset(10000.0f, -6000.0f), _temperature_noise_influence(0.4f), _temperature_altitude_reference(10.0f), _moisture_frequency(0.0006f), _moisture_offset(-4000.0f, 9000.0f) {
	_internal_noise.instantiate();
	_internal_noise->set_noise_type(FastNoiseLite::TYPE_PERLIN);
	_internal_noise->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
}

TerrainConfiguration::~TerrainConfiguration() = default;

void TerrainConfiguration::_update_preview() {
	if (!_internal_noise.is_valid()) {
		return;
	}

	if (!_noise_preview.is_valid()) {
		_noise_preview.instantiate();
	}

	_internal_noise->set_fractal_octaves(_noise_octaves);
	_internal_noise->set_frequency(_noise_base_frequency);
	_internal_noise->set_fractal_lacunarity(_noise_lacunarity);
	_internal_noise->set_fractal_gain(_noise_gain);

	const Ref<Image> noise_image = _internal_noise->get_seamless_image(512, 512);
	if (noise_image.is_valid()) {
		_noise_preview->set_image(noise_image);
	}
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

Ref<ImageTexture> TerrainConfiguration::get_noise_preview() const {
	return _noise_preview;
}

} //namespace ts
