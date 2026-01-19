#include "terrain_configuration.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace ts {

void TerrainConfiguration::_bind_methods() {
	// Register the properties in order to be able to edit them in the inspector

	ClassDB::bind_method(D_METHOD("set_height_scale", "height_scale"), &TerrainConfiguration::set_height_scale);
	ClassDB::bind_method(D_METHOD("get_height_scale"), &TerrainConfiguration::get_height_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height_scale", PROPERTY_HINT_RANGE, "0.1, 1000.0"), "set_height_scale", "get_height_scale");

	ClassDB::bind_method(D_METHOD("set_seed", "seed"), &TerrainConfiguration::set_seed);
	ClassDB::bind_method(D_METHOD("get_seed"), &TerrainConfiguration::get_seed);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");

	ClassDB::bind_method(D_METHOD("set_noise_scale", "noise_scale"), &TerrainConfiguration::set_noise_scale);
	ClassDB::bind_method(D_METHOD("get_noise_scale"), &TerrainConfiguration::get_noise_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_scale", PROPERTY_HINT_RANGE, "0.1, 1000.0"), "set_noise_scale", "get_noise_scale");
}

TerrainConfiguration::TerrainConfiguration() : _height_scale(100.0), _seed(0), _noise_scale(0.01) {}

TerrainConfiguration::~TerrainConfiguration() {}

// Getters and Setters
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
void TerrainConfiguration::set_noise_scale(double p_noise_scale) {
	_noise_scale = p_noise_scale;
	emit_changed();
}
double TerrainConfiguration::get_noise_scale() const {
	return _noise_scale;
}

} //namespace ts