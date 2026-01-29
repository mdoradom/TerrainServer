#include "terrain_configuration.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace ts {

void TerrainConfiguration::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_height_scale", "height_scale"), &TerrainConfiguration::set_height_scale);
	ClassDB::bind_method(D_METHOD("get_height_scale"), &TerrainConfiguration::get_height_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height_scale", PROPERTY_HINT_RANGE, "0.1,1000.0"), "set_height_scale", "get_height_scale");

	ClassDB::bind_method(D_METHOD("set_mesh_resolution", "resolution"), &TerrainConfiguration::set_mesh_resolution);
	ClassDB::bind_method(D_METHOD("get_mesh_resolution"), &TerrainConfiguration::get_mesh_resolution);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_resolution", PROPERTY_HINT_RANGE, "1,512"), "set_mesh_resolution", "get_mesh_resolution");

	ClassDB::bind_method(D_METHOD("set_material_override", "material"), &TerrainConfiguration::set_material_override);
	ClassDB::bind_method(D_METHOD("get_material_override"), &TerrainConfiguration::get_material_override);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material_override", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_material_override", "get_material_override");

	ClassDB::bind_method(D_METHOD("set_noise", "noise"), &TerrainConfiguration::set_noise);
	ClassDB::bind_method(D_METHOD("get_noise"), &TerrainConfiguration::get_noise);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise", PROPERTY_HINT_RESOURCE_TYPE, "FastNoiseLite"), "set_noise", "get_noise");
}

TerrainConfiguration::TerrainConfiguration() : _height_scale(100.0), _mesh_resolution(32) {
	_noise.instantiate();
	_noise->set_noise_type(FastNoiseLite::TYPE_PERLIN);
	_noise->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
	_noise->set_seed(0);
	_noise->set_frequency(0.01);

	// Connect to noise changed signal to propagate changes
	_noise->connect("changed", Callable(this, "emit_changed"));
}

TerrainConfiguration::~TerrainConfiguration() {
	if (_noise.is_valid()) {
		_noise->disconnect("changed", Callable(this, "emit_changed"));
	}
}

double TerrainConfiguration::get_height_scale() const {
	return _height_scale;
}

void TerrainConfiguration::set_height_scale(double p_scale) {
	_height_scale = p_scale;
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

Ref<FastNoiseLite> TerrainConfiguration::get_noise() const {
	return _noise;
}

void TerrainConfiguration::set_noise(const Ref<FastNoiseLite> &p_noise) {
	if (_noise.is_valid()) {
		_noise->disconnect("changed", Callable(this, "emit_changed"));
	}

	_noise = p_noise;

	if (_noise.is_valid()) {
		_noise->connect("changed", Callable(this, "emit_changed"));
	}

	emit_changed();
}

} //namespace ts