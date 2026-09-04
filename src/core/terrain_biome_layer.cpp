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

	ClassDB::bind_method(D_METHOD("set_height_texture", "texture"), &TerrainBiomeLayer::set_height_texture);
	ClassDB::bind_method(D_METHOD("get_height_texture"), &TerrainBiomeLayer::get_height_texture);

	ClassDB::bind_method(D_METHOD("set_ao_texture", "texture"), &TerrainBiomeLayer::set_ao_texture);
	ClassDB::bind_method(D_METHOD("get_ao_texture"), &TerrainBiomeLayer::get_ao_texture);

	ClassDB::bind_method(D_METHOD("set_uv_scale", "scale"), &TerrainBiomeLayer::set_uv_scale);
	ClassDB::bind_method(D_METHOD("get_uv_scale"), &TerrainBiomeLayer::get_uv_scale);

	ClassDB::bind_method(D_METHOD("set_pom_depth", "depth"), &TerrainBiomeLayer::set_pom_depth);
	ClassDB::bind_method(D_METHOD("get_pom_depth"), &TerrainBiomeLayer::get_pom_depth);

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
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "height_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_height_texture", "get_height_texture");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "ao_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_ao_texture", "get_ao_texture");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "uv_scale", PROPERTY_HINT_RANGE, "0.001,2.0,0.001,or_greater"), "set_uv_scale", "get_uv_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pom_depth", PROPERTY_HINT_RANGE, "0.0,0.25,0.001,or_greater"), "set_pom_depth", "get_pom_depth");

	ADD_GROUP("Classification", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_temperature", PROPERTY_HINT_RANGE, "-10.0,30.0"), "set_min_temperature", "get_min_temperature");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_temperature", PROPERTY_HINT_RANGE, "-10.0,30.0"), "set_max_temperature", "get_max_temperature");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_moisture", PROPERTY_HINT_RANGE, "0.0,1.0"), "set_min_moisture", "get_min_moisture");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_moisture", PROPERTY_HINT_RANGE, "0.0,1.0"), "set_max_moisture", "get_max_moisture");
}

TerrainBiomeLayer::TerrainBiomeLayer() :
		_uv_scale(0.1f), _pom_depth(0.0f), _min_temperature(-10.0f), _max_temperature(30.0f), _min_moisture(0.0f), _max_moisture(1.0f) {
}

TerrainBiomeLayer::~TerrainBiomeLayer() = default;

// ============== Getters and setters ==============

Ref<Texture2D> TerrainBiomeLayer::get_albedo_texture() const {
	return _albedo_texture;
}

void TerrainBiomeLayer::set_albedo_texture(const Ref<Texture2D> &p_texture) {
	set_texture_slot(this, _albedo_texture, p_texture, "albedo_texture", "TerrainBiomeLayer");
}

Ref<Texture2D> TerrainBiomeLayer::get_normal_texture() const {
	return _normal_texture;
}

void TerrainBiomeLayer::set_normal_texture(const Ref<Texture2D> &p_texture) {
	set_texture_slot(this, _normal_texture, p_texture, "normal_texture", "TerrainBiomeLayer");
}

Ref<Texture2D> TerrainBiomeLayer::get_roughness_texture() const {
	return _roughness_texture;
}

void TerrainBiomeLayer::set_roughness_texture(const Ref<Texture2D> &p_texture) {
	set_texture_slot(this, _roughness_texture, p_texture, "roughness_texture", "TerrainBiomeLayer");
}

Ref<Texture2D> TerrainBiomeLayer::get_height_texture() const {
	return _height_texture;
}

void TerrainBiomeLayer::set_height_texture(const Ref<Texture2D> &p_texture) {
	set_texture_slot(this, _height_texture, p_texture, "height_texture", "TerrainBiomeLayer");
}

Ref<Texture2D> TerrainBiomeLayer::get_ao_texture() const {
	return _ao_texture;
}

void TerrainBiomeLayer::set_ao_texture(const Ref<Texture2D> &p_texture) {
	set_texture_slot(this, _ao_texture, p_texture, "ao_texture", "TerrainBiomeLayer");
}

float TerrainBiomeLayer::get_uv_scale() const {
	return _uv_scale;
}

void TerrainBiomeLayer::set_uv_scale(const float p_scale) {
	if (_uv_scale != p_scale) {
		_uv_scale = p_scale;
		emit_changed();
	}
}

float TerrainBiomeLayer::get_pom_depth() const {
	return _pom_depth;
}

void TerrainBiomeLayer::set_pom_depth(const float p_depth) {
	if (_pom_depth != p_depth) {
		_pom_depth = p_depth;
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
