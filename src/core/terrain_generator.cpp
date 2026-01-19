#include "terrain_generator.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace ts {

void TerrainGenerator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("setup", "config"), &TerrainGenerator::setup);
	ClassDB::bind_method(D_METHOD("get_height", "x", "y"), &TerrainGenerator::get_height);
}

TerrainGenerator::TerrainGenerator() {
	_noise.instantiate();
	_noise->set_noise_type(FastNoiseLite::TYPE_PERLIN);
	_noise->set_fractal_type(FastNoiseLite::FRACTAL_FBM);

	_height_scale = 1.0f;
}

TerrainGenerator::~TerrainGenerator() {}

void TerrainGenerator::setup(const godot::Ref<TerrainConfiguration> &p_config) {
	if (!p_config.is_valid()) {
		return;
	}

	_noise->set_seed(p_config->get_seed());
	_noise->set_frequency(p_config->get_noise_scale());
	_height_scale = p_config->get_height_scale();
}

float TerrainGenerator::get_height(float x, float y) const {
	float noise_value = _noise->get_noise_2d(x, y);
	return noise_value * _height_scale;
}

} //namespace ts