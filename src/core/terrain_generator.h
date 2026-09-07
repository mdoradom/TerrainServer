#pragma once

#include "terrain_configuration.h"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/ref_counted.hpp>

namespace ts {

class TerrainGenerator : public godot::RefCounted {
	GDCLASS(TerrainGenerator, godot::RefCounted);

protected:
	static void _bind_methods();

public:
	TerrainGenerator();
	~TerrainGenerator();

	void setup(const godot::Ref<TerrainConfiguration> &p_config);

	static godot::Ref<godot::ArrayMesh> create_block_mesh(int resolution);
	static godot::Ref<godot::ArrayMesh> create_ring_fixup_mesh(int resolution);
	static godot::Ref<godot::ArrayMesh> create_trim_mesh(int resolution, int p_dx, int p_dz);
};

} //namespace ts