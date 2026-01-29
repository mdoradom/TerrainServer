#include "terrain_configuration.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace ts {

void TerrainConfiguration::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_height_scale", "height_scale"), &TerrainConfiguration::set_height_scale);
	ClassDB::bind_method(D_METHOD("get_height_scale"), &TerrainConfiguration::get_height_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height_scale", PROPERTY_HINT_RANGE, "0.1,1000.0"), "set_height_scale", "get_height_scale");

	ClassDB::bind_method(D_METHOD("set_seed", "seed"), &TerrainConfiguration::set_seed);
	ClassDB::bind_method(D_METHOD("get_seed"), &TerrainConfiguration::get_seed);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");

	ClassDB::bind_method(D_METHOD("set_noise_scale", "noise_scale"), &TerrainConfiguration::set_noise_scale);
	ClassDB::bind_method(D_METHOD("get_noise_scale"), &TerrainConfiguration::get_noise_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_scale", PROPERTY_HINT_RANGE, "0.001,1.0"), "set_noise_scale", "get_noise_scale");

	ClassDB::bind_method(D_METHOD("set_mesh_resolution", "resolution"), &TerrainConfiguration::set_mesh_resolution);
	ClassDB::bind_method(D_METHOD("get_mesh_resolution"), &TerrainConfiguration::get_mesh_resolution);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_resolution", PROPERTY_HINT_RANGE, "1,512"), "set_mesh_resolution", "get_mesh_resolution");

	ClassDB::bind_method(D_METHOD("set_material_override", "material"), &TerrainConfiguration::set_material_override);
	ClassDB::bind_method(D_METHOD("get_material_override"), &TerrainConfiguration::get_material_override);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material_override", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_material_override", "get_material_override");
}

TerrainConfiguration::TerrainConfiguration() :
	_height_scale(100.0),
	_seed(0),
	_noise_scale(0.01),
	_mesh_resolution(32) {}

TerrainConfiguration::~TerrainConfiguration() {}

double TerrainConfiguration::get_height_scale() const {
	return _height_scale;
}

void TerrainConfiguration::set_height_scale(double p_scale) {
	_height_scale = p_scale;
	emit_changed();
}

int TerrainConfiguration::get_seed() const {
	return _seed;
}

void TerrainConfiguration::set_seed(int p_seed) {
	_seed = p_seed;
	emit_changed();
}

double TerrainConfiguration::get_noise_scale() const {
	return _noise_scale;
}

void TerrainConfiguration::set_noise_scale(double p_noise_scale) {
	_noise_scale = p_noise_scale;
	emit_changed();
}

int TerrainConfiguration::get_mesh_resolution() const {
	return _mesh_resolution;
}

void TerrainConfiguration::set_mesh_resolution(int p_resolution) {
	_mesh_resolution = p_resolution;
	emit_changed();
}

Ref<Material> TerrainConfiguration::get_material_override() const {
	return _material_override;
}

void TerrainConfiguration::set_material_override(const Ref<Material> &p_material) {
	_material_override = p_material;
	emit_changed();
}

} //namespace ts