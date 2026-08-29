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

	ClassDB::bind_method(D_METHOD("set_slope_threshold", "threshold"), &TerrainSlopeLayer::set_slope_threshold);
	ClassDB::bind_method(D_METHOD("get_slope_threshold"), &TerrainSlopeLayer::get_slope_threshold);

	ClassDB::bind_method(D_METHOD("set_slope_blend_range", "range"), &TerrainSlopeLayer::set_slope_blend_range);
	ClassDB::bind_method(D_METHOD("get_slope_blend_range"), &TerrainSlopeLayer::get_slope_blend_range);

	ADD_GROUP("Textures", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "albedo_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_albedo_texture", "get_albedo_texture");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "normal_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_normal_texture", "get_normal_texture");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "roughness_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_roughness_texture", "get_roughness_texture");

	ADD_GROUP("Slope", "slope_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "slope_threshold", PROPERTY_HINT_RANGE, "0.0,1.0"), "set_slope_threshold", "get_slope_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "slope_blend_range", PROPERTY_HINT_RANGE, "0.01,1.0"), "set_slope_blend_range", "get_slope_blend_range");
}

TerrainSlopeLayer::TerrainSlopeLayer() :
		_slope_threshold(0.6f), _slope_blend_range(0.15f) {
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
