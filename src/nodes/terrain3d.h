#pragma once

#include "core/terrain_configuration.h"
#include "core/terrain_generator.h"
#include "godot_cpp/classes/mesh_instance3d.hpp"
#include "godot_cpp/classes/material.hpp"

#include <godot_cpp/classes/node3d.hpp>

using namespace godot;

namespace ts {

class Terrain3D : public godot::Node3D {
	GDCLASS(Terrain3D, godot::Node3D);

private:
	Ref<TerrainConfiguration> _config;
	Ref<TerrainGenerator> _generator;

	MeshInstance3D* _mesh_instance = nullptr;

	int _mesh_resolution = 64;
	Ref<Material> _material_override;

	void _generate_mesh();

protected:
	static void _bind_methods();
	void _update_generator();

public:
	Terrain3D();
	~Terrain3D();

	void _process(double delta) override;

	// Getter and Setter
	Ref<TerrainConfiguration> get_configuration() const;
	void set_configuration(const Ref<TerrainConfiguration> &p_config);

	int get_mesh_resolution() const;
	void set_mesh_resolution(int p_resolution);

	Ref<Material> get_material() const;
	void set_material(const Ref<Material> &p_material);
};

} //namespace ts
