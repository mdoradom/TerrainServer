#pragma once

#include "core/terrain_configuration.h"
#include "core/terrain_generator.h"
#include "core/terrain_renderer.h"

#include <godot_cpp/classes/node3d.hpp>

using namespace godot;

namespace ts {

class Terrain3D : public godot::Node3D {
	GDCLASS(Terrain3D, godot::Node3D);

private:
	Ref<TerrainConfiguration> _config;
	Ref<TerrainGenerator> _generator;
	Ref<TerrainRenderer> _renderer;

protected:
	static void _bind_methods();
	void _update_generator();

public:
	Terrain3D();
	~Terrain3D();

	void _ready() override;
	void _process(double delta) override;

	Ref<TerrainConfiguration> get_configuration() const;
	void set_configuration(const Ref<TerrainConfiguration> &p_config);

	Ref<TerrainGenerator> get_generator() const;
	Ref<TerrainRenderer> get_renderer() const;
};

} //namespace ts