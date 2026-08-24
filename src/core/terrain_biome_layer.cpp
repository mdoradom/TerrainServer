#include "terrain_biome_layer.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace ts {

void TerrainBiomeLayer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_albedo_texture", "texture"), &TerrainBiomeLayer::set_albedo_texture);
	ClassDB::bind_method(D_METHOD("get_albedo_texture"), &TerrainBiomeLayer::get_albedo_texture);

	ClassDB::bind_method(D_METHOD("set_normal_texture", "texture"), &TerrainBiomeLayer::set_normal_texture);
	ClassDB::bind_method(D_METHOD("get_normal_texture"), &TerrainBiomeLayer::get_normal_texture);

	ClassDB::bind_method(D_METHOD("set_roughness_texture", "texture"), &TerrainBiomeLayer::set_roughness_texture);
	ClassDB::bind_method(D_METHOD("get_roughness_texture"), &TerrainBiomeLayer::get_roughness_texture);

	ClassDB::bind_method(D_METHOD("set_min_temperature", "temperature"), &TerrainBiomeLayer::set_min_temperature);
	ClassDB::bind_method(D_METHOD("get_min_temperature"), &TerrainBiomeLayer::get_min_temperature);

	ClassDB::bind_method(D_METHOD("set_max_temperature", "temperature"), &TerrainBiomeLayer::set_max_temperature);
	ClassDB::bind_method(D_METHOD("get_max_temperature"), &TerrainBiomeLayer::get_max_temperature);

	ClassDB::bind_method(D_METHOD("set_min_moisture", "moisture"), &TerrainBiomeLayer::set_min_moisture);
	ClassDB::bind_method(D_METHOD("get_min_moisture"), &TerrainBiomeLayer::get_min_moisture);

	ClassDB::bind_method(D_METHOD("set_max_moisture", "moisture"), &TerrainBiomeLayer::set_max_moisture);
	ClassDB::bind_method(D_METHOD("get_max_moisture"), &TerrainBiomeLayer::get_max_moisture);

	ClassDB::bind_method(D_METHOD("set_biome_name", "name"), &TerrainBiomeLayer::set_biome_name);
	ClassDB::bind_method(D_METHOD("get_biome_name"), &TerrainBiomeLayer::get_biome_name);

	ADD_GROUP("Biome", "");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "biome_name"), "set_biome_name", "get_biome_name");

	ADD_GROUP("Textures", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "albedo_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_albedo_texture", "get_albedo_texture");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "normal_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_normal_texture", "get_normal_texture");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "roughness_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_roughness_texture", "get_roughness_texture");

	ADD_GROUP("Classification", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_temperature", PROPERTY_HINT_RANGE, "-10.0,30.0"), "set_min_temperature", "get_min_temperature");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_temperature", PROPERTY_HINT_RANGE, "-10.0,30.0"), "set_max_temperature", "get_max_temperature");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_moisture", PROPERTY_HINT_RANGE, "0.0,1.0"), "set_min_moisture", "get_min_moisture");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_moisture", PROPERTY_HINT_RANGE, "0.0,1.0"), "set_max_moisture", "get_max_moisture");
}

TerrainBiomeLayer::TerrainBiomeLayer() :
		_min_temperature(-10.0f), _max_temperature(30.0f), _min_moisture(0.0f), _max_moisture(1.0f) {
}

TerrainBiomeLayer::~TerrainBiomeLayer() = default;

// ============== Getters and setters ==============

Ref<Texture2D> TerrainBiomeLayer::get_albedo_texture() const {
	return _albedo_texture;
}

void TerrainBiomeLayer::set_albedo_texture(const Ref<Texture2D> &p_texture) {
	if (p_texture.is_valid() && (p_texture->get_width() > MAX_TEXTURE_DIMENSION || p_texture->get_height() > MAX_TEXTURE_DIMENSION)) {
		UtilityFunctions::push_warning("TerrainBiomeLayer: albedo_texture (", p_texture->get_width(), "x", p_texture->get_height(), ") exceeds the ", MAX_TEXTURE_DIMENSION, "x", MAX_TEXTURE_DIMENSION, " size cap; ignoring.");
		return;
	}

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

Ref<Texture2D> TerrainBiomeLayer::get_normal_texture() const {
	return _normal_texture;
}

void TerrainBiomeLayer::set_normal_texture(const Ref<Texture2D> &p_texture) {
	if (p_texture.is_valid() && (p_texture->get_width() > MAX_TEXTURE_DIMENSION || p_texture->get_height() > MAX_TEXTURE_DIMENSION)) {
		UtilityFunctions::push_warning("TerrainBiomeLayer: normal_texture (", p_texture->get_width(), "x", p_texture->get_height(), ") exceeds the ", MAX_TEXTURE_DIMENSION, "x", MAX_TEXTURE_DIMENSION, " size cap; ignoring.");
		return;
	}

	if (_normal_texture != p_texture) {
		if (_normal_texture.is_valid()) {
			_normal_texture->disconnect("changed", Callable(this, "emit_changed"));
		}

		_normal_texture = p_texture;

		if (_normal_texture.is_valid()) {
			_normal_texture->connect("changed", Callable(this, "emit_changed"));
		}

		emit_changed();
	}
}

Ref<Texture2D> TerrainBiomeLayer::get_roughness_texture() const {
	return _roughness_texture;
}

void TerrainBiomeLayer::set_roughness_texture(const Ref<Texture2D> &p_texture) {
	if (p_texture.is_valid() && (p_texture->get_width() > MAX_TEXTURE_DIMENSION || p_texture->get_height() > MAX_TEXTURE_DIMENSION)) {
		UtilityFunctions::push_warning("TerrainBiomeLayer: roughness_texture (", p_texture->get_width(), "x", p_texture->get_height(), ") exceeds the ", MAX_TEXTURE_DIMENSION, "x", MAX_TEXTURE_DIMENSION, " size cap; ignoring.");
		return;
	}

	if (_roughness_texture != p_texture) {
		if (_roughness_texture.is_valid()) {
			_roughness_texture->disconnect("changed", Callable(this, "emit_changed"));
		}

		_roughness_texture = p_texture;

		if (_roughness_texture.is_valid()) {
			_roughness_texture->connect("changed", Callable(this, "emit_changed"));
		}

		emit_changed();
	}
}

float TerrainBiomeLayer::get_min_temperature() const {
	return _min_temperature;
}

void TerrainBiomeLayer::set_min_temperature(const float p_temperature) {
	if (_min_temperature != p_temperature) {
		_min_temperature = p_temperature;
		emit_changed();
	}
}

float TerrainBiomeLayer::get_max_temperature() const {
	return _max_temperature;
}

void TerrainBiomeLayer::set_max_temperature(const float p_temperature) {
	if (_max_temperature != p_temperature) {
		_max_temperature = p_temperature;
		emit_changed();
	}
}

float TerrainBiomeLayer::get_min_moisture() const {
	return _min_moisture;
}

void TerrainBiomeLayer::set_min_moisture(const float p_moisture) {
	if (_min_moisture != p_moisture) {
		_min_moisture = p_moisture;
		emit_changed();
	}
}

float TerrainBiomeLayer::get_max_moisture() const {
	return _max_moisture;
}

void TerrainBiomeLayer::set_max_moisture(const float p_moisture) {
	if (_max_moisture != p_moisture) {
		_max_moisture = p_moisture;
		emit_changed();
	}
}

String TerrainBiomeLayer::get_biome_name() const {
	return _biome_name;
}

void TerrainBiomeLayer::set_biome_name(const String &p_name) {
	if (_biome_name != p_name) {
		_biome_name = p_name;
		emit_changed();
	}
}

} //namespace ts
