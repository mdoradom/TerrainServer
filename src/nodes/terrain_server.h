#pragma once

#include "core/terrain_configuration.h"
#include "core/terrain_generator.h"
#include "godot_cpp/classes/mesh_instance3d.hpp"

#include <godot_cpp/classes/node3d.hpp>

using namespace godot;

namespace ts {

class TerrainServer : public godot::Node3D {
	GDCLASS(TerrainServer, godot::Node3D);

private:
	Ref<TerrainConfiguration> _config;
	Ref<TerrainGenerator> _generator;

	MeshInstance3D* _debug_mesh_instance = nullptr;

	void _generate_debug_mesh();

protected:
	static void _bind_methods();
	void _update_generator();

public:
	TerrainServer();
	~TerrainServer();

	void _process(double delta) override;

	Ref<TerrainConfiguration> get_configuration() const;
	void set_configuration(const Ref<TerrainConfiguration> &p_config);
};

} //namespace ts
