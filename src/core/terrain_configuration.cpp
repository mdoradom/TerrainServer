#include "terrain_configuration.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace ts {

void TerrainConfiguration::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_height_scale", "height_scale"), &TerrainConfiguration::set_height_scale);
	ClassDB::bind_method(D_METHOD("get_height_scale"), &TerrainConfiguration::get_height_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height_scale", PROPERTY_HINT_RANGE, "0.1,100.0"), "set_height_scale", "get_height_scale");

	ClassDB::bind_method(D_METHOD("set_mesh_resolution", "resolution"), &TerrainConfiguration::set_mesh_resolution);
	ClassDB::bind_method(D_METHOD("get_mesh_resolution"), &TerrainConfiguration::get_mesh_resolution);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_resolution", PROPERTY_HINT_RANGE, "1,512"), "set_mesh_resolution", "get_mesh_resolution");

	ClassDB::bind_method(D_METHOD("set_noise", "noise"), &TerrainConfiguration::set_noise);
	ClassDB::bind_method(D_METHOD("get_noise"), &TerrainConfiguration::get_noise);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise", PROPERTY_HINT_RESOURCE_TYPE, "FastNoiseLite"), "set_noise", "get_noise");

	ClassDB::bind_method(D_METHOD("set_terrain_size", "size"), &TerrainConfiguration::set_terrain_size);
	ClassDB::bind_method(D_METHOD("get_terrain_size"), &TerrainConfiguration::get_terrain_size);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "terrain_size", PROPERTY_HINT_RANGE, "1.0,10000.0"), "set_terrain_size", "get_terrain_size");

	ClassDB::bind_method(D_METHOD("set_clipmap_levels", "levels"), &TerrainConfiguration::set_clipmap_levels);
	ClassDB::bind_method(D_METHOD("get_clipmap_levels"), &TerrainConfiguration::get_clipmap_levels);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "clipmap_levels", PROPERTY_HINT_RANGE, "1,10"), "set_clipmap_levels", "get_clipmap_levels");

	ClassDB::bind_method(D_METHOD("set_noise_texture_size", "size"), &TerrainConfiguration::set_noise_texture_size);
	ClassDB::bind_method(D_METHOD("get_noise_texture_size"), &TerrainConfiguration::get_noise_texture_size);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "noise_texture_size", PROPERTY_HINT_RANGE, "64,4096"), "set_noise_texture_size", "get_noise_texture_size");

	ClassDB::bind_method(D_METHOD("get_noise_texture"), &TerrainConfiguration::get_noise_texture);
}

TerrainConfiguration::TerrainConfiguration() : _height_scale(10.0), _mesh_resolution(256), _terrain_size(256.0f), _noise_texture_size(256), _clipmap_levels(6) {
	_noise_texture.instantiate();
}

TerrainConfiguration::~TerrainConfiguration() {
	if (_noise.is_valid() && _noise->is_connected("changed", Callable(this, "emit_changed"))) {
		_noise->disconnect("changed", Callable(this, "emit_changed"));
	}
}

void TerrainConfiguration::_generate_noise_texture() const {
	if (!_noise.is_valid()) {
		return;
	}

	const Ref<Image> noise_image = _noise->get_seamless_image(_noise_texture_size, _noise_texture_size);

	if (noise_image.is_valid()) {
		_noise_texture->set_image(noise_image);
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

Ref<FastNoiseLite> TerrainConfiguration::get_noise() const {
	return _noise;
}

void TerrainConfiguration::set_noise(const Ref<FastNoiseLite> &p_noise) {
	if (_noise != p_noise) {
		if (_noise.is_valid()) {
			_noise->disconnect("changed", Callable(this, "emit_changed"));
		}

		_noise = p_noise;

		if (_noise.is_valid()) {
			_noise->connect("changed", Callable(this, "emit_changed"));
		}

		_generate_noise_texture();
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

Ref<ImageTexture> TerrainConfiguration::get_noise_texture() const {
	return _noise_texture;
}

int TerrainConfiguration::get_noise_texture_size() const {
	return _noise_texture_size;
}

void TerrainConfiguration::set_noise_texture_size(const int p_size) {
	if (_noise_texture_size != p_size) {
		_noise_texture_size = CLAMP(p_size, 64, 4096);
		_generate_noise_texture();
		emit_changed();
	}
}

} //namespace ts