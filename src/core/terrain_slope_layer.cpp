#include "terrain_slope_layer.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace ts {

void TerrainSlopeLayer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_albedo_texture", "texture"), &TerrainSlopeLayer::set_albedo_texture);
	ClassDB::bind_method(D_METHOD("get_albedo_texture"), &TerrainSlopeLayer::get_albedo_texture);

	ClassDB::bind_method(D_METHOD("set_normal_texture", "texture"), &TerrainSlopeLayer::set_normal_texture);
	ClassDB::bind_method(D_METHOD("get_normal_texture"), &TerrainSlopeLayer::get_normal_texture);

	ClassDB::bind_method(D_METHOD("set_roughness_texture", "texture"), &TerrainSlopeLayer::set_roughness_texture);
	ClassDB::bind_method(D_METHOD("get_roughness_texture"), &TerrainSlopeLayer::get_roughness_texture);

	ClassDB::bind_method(D_METHOD("set_height_texture", "texture"), &TerrainSlopeLayer::set_height_texture);
	ClassDB::bind_method(D_METHOD("get_height_texture"), &TerrainSlopeLayer::get_height_texture);

	ClassDB::bind_method(D_METHOD("set_ao_texture", "texture"), &TerrainSlopeLayer::set_ao_texture);
	ClassDB::bind_method(D_METHOD("get_ao_texture"), &TerrainSlopeLayer::get_ao_texture);

	ClassDB::bind_method(D_METHOD("set_uv_scale", "scale"), &TerrainSlopeLayer::set_uv_scale);
	ClassDB::bind_method(D_METHOD("get_uv_scale"), &TerrainSlopeLayer::get_uv_scale);

	ClassDB::bind_method(D_METHOD("set_pom_depth", "depth"), &TerrainSlopeLayer::set_pom_depth);
	ClassDB::bind_method(D_METHOD("get_pom_depth"), &TerrainSlopeLayer::get_pom_depth);

	ClassDB::bind_method(D_METHOD("set_slope_threshold", "threshold"), &TerrainSlopeLayer::set_slope_threshold);
	ClassDB::bind_method(D_METHOD("get_slope_threshold"), &TerrainSlopeLayer::get_slope_threshold);

	ClassDB::bind_method(D_METHOD("set_slope_blend_range", "range"), &TerrainSlopeLayer::set_slope_blend_range);
	ClassDB::bind_method(D_METHOD("get_slope_blend_range"), &TerrainSlopeLayer::get_slope_blend_range);

	ADD_GROUP("Textures", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "albedo_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_albedo_texture", "get_albedo_texture");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "normal_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_normal_texture", "get_normal_texture");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "roughness_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_roughness_texture", "get_roughness_texture");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "height_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_height_texture", "get_height_texture");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "ao_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_ao_texture", "get_ao_texture");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "uv_scale", PROPERTY_HINT_RANGE, "0.001,2.0,0.001,or_greater"), "set_uv_scale", "get_uv_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pom_depth", PROPERTY_HINT_RANGE, "0.0,0.25,0.001,or_greater"), "set_pom_depth", "get_pom_depth");

	ADD_GROUP("Slope", "slope_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "slope_threshold", PROPERTY_HINT_RANGE, "0.0,1.0"), "set_slope_threshold", "get_slope_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "slope_blend_range", PROPERTY_HINT_RANGE, "0.01,1.0"), "set_slope_blend_range", "get_slope_blend_range");
}

TerrainSlopeLayer::TerrainSlopeLayer() :
		_uv_scale(0.1f), _pom_depth(0.0f), _slope_threshold(0.6f), _slope_blend_range(0.15f) {
}

TerrainSlopeLayer::~TerrainSlopeLayer() = default;

// ============== Getters and setters ==============

Ref<Texture2D> TerrainSlopeLayer::get_albedo_texture() const {
	return _albedo_texture;
}

void TerrainSlopeLayer::set_albedo_texture(const Ref<Texture2D> &p_texture) {
	set_texture_slot(this, _albedo_texture, p_texture, "albedo_texture", "TerrainSlopeLayer");
}

Ref<Texture2D> TerrainSlopeLayer::get_normal_texture() const {
	return _normal_texture;
}

void TerrainSlopeLayer::set_normal_texture(const Ref<Texture2D> &p_texture) {
	set_texture_slot(this, _normal_texture, p_texture, "normal_texture", "TerrainSlopeLayer");
}

Ref<Texture2D> TerrainSlopeLayer::get_roughness_texture() const {
	return _roughness_texture;
}

void TerrainSlopeLayer::set_roughness_texture(const Ref<Texture2D> &p_texture) {
	set_texture_slot(this, _roughness_texture, p_texture, "roughness_texture", "TerrainSlopeLayer");
}

Ref<Texture2D> TerrainSlopeLayer::get_height_texture() const {
	return _height_texture;
}

void TerrainSlopeLayer::set_height_texture(const Ref<Texture2D> &p_texture) {
	set_texture_slot(this, _height_texture, p_texture, "height_texture", "TerrainSlopeLayer");
}

Ref<Texture2D> TerrainSlopeLayer::get_ao_texture() const {
	return _ao_texture;
}

void TerrainSlopeLayer::set_ao_texture(const Ref<Texture2D> &p_texture) {
	set_texture_slot(this, _ao_texture, p_texture, "ao_texture", "TerrainSlopeLayer");
}

float TerrainSlopeLayer::get_uv_scale() const {
	return _uv_scale;
}

void TerrainSlopeLayer::set_uv_scale(const float p_scale) {
	if (_uv_scale != p_scale) {
		_uv_scale = p_scale;
		emit_changed();
	}
}

float TerrainSlopeLayer::get_pom_depth() const {
	return _pom_depth;
}

void TerrainSlopeLayer::set_pom_depth(const float p_depth) {
	if (_pom_depth != p_depth) {
		_pom_depth = p_depth;
		emit_changed();
	}
}

float TerrainSlopeLayer::get_slope_threshold() const {
	return _slope_threshold;
}

void TerrainSlopeLayer::set_slope_threshold(const float p_threshold) {
	if (_slope_threshold != p_threshold) {
		_slope_threshold = p_threshold;
		emit_changed();
	}
}

float TerrainSlopeLayer::get_slope_blend_range() const {
	return _slope_blend_range;
}

void TerrainSlopeLayer::set_slope_blend_range(const float p_range) {
	if (_slope_blend_range != p_range) {
		_slope_blend_range = p_range;
		emit_changed();
	}
}

} //namespace ts
