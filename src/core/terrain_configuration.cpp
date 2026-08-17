#include "terrain_configuration.h"
#include <godot_cpp/core/class_db.hpp>

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

	ClassDB::bind_method(D_METHOD("set_albedo_texture", "texture"), &TerrainConfiguration::set_albedo_texture);
	ClassDB::bind_method(D_METHOD("get_albedo_texture"), &TerrainConfiguration::get_albedo_texture);

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

	ADD_GROUP("Material", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "albedo_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_albedo_texture", "get_albedo_texture");
}

TerrainConfiguration::TerrainConfiguration() : _height_scale(10.0), _mesh_resolution(256), _terrain_size(256.0f), _noise_octaves(5), _noise_base_frequency(0.002f), _noise_lacunarity(2.0f), _noise_gain(0.5f), _clipmap_levels(6), _physics_range(64.0f) {
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

void TerrainConfiguration::set_mesh_resolution(const int p_resolution) {
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

Ref<Texture2D> TerrainConfiguration::get_albedo_texture() const {
	return _albedo_texture;
}

void TerrainConfiguration::set_albedo_texture(const Ref<Texture2D> &p_texture) {
	if (_albedo_texture != p_texture) {
		if (_albedo_texture.is_valid()) {
			_albedo_texture->disconnect("changed", Callable(this, "emit_changed"));
		}

		_albedo_texture = p_texture;

		if (_albedo_texture.is_valid()) {
			_albedo_texture->connect("changed", Callable(this, "emit_changed"));
		}

		emit_changed();
	}
}

Ref<ImageTexture> TerrainConfiguration::get_noise_preview() const {
	return _noise_preview;
}

} //namespace ts
