#pragma once

#include "terrain_configuration.h"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/ref_counted.hpp>

namespace ts {

class TerrainGenerator : public godot::RefCounted {
	GDCLASS(TerrainGenerator, godot::RefCounted);

private:
	godot::Ref<godot::FastNoiseLite> _noise;
	double _height_scale;

protected:
	static void _bind_methods();

public:
	TerrainGenerator();
	~TerrainGenerator();

	void setup(const godot::Ref<TerrainConfiguration> &p_config);

	float get_height(float x, float y) const;
	godot::Ref<godot::ArrayMesh> generate_mesh(int resolution, float size) const;
};

} //namespace ts