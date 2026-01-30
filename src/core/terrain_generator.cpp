#include "terrain_generator.h"
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace ts {

void TerrainGenerator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("setup", "config"), &TerrainGenerator::setup);
	ClassDB::bind_method(D_METHOD("get_height", "x", "y"), &TerrainGenerator::get_height);
	ClassDB::bind_method(D_METHOD("generate_mesh", "resolution", "size"), &TerrainGenerator::generate_mesh);
}

TerrainGenerator::TerrainGenerator() : _height_scale(1.0f) {}

TerrainGenerator::~TerrainGenerator() {}

void TerrainGenerator::setup(const godot::Ref<TerrainConfiguration> &p_config) {
	if (!p_config.is_valid()) {
		return;
	}

	_noise = p_config->get_noise();
	_height_scale = p_config->get_height_scale();
}

float TerrainGenerator::get_height(float x, float y) const {
	if (!_noise.is_valid()) {
		return 0.0f;
	}
	float noise_value = _noise->get_noise_2d(x, y);
	return noise_value * _height_scale;
}

Ref<ArrayMesh> TerrainGenerator::create_mesh_data(int resolution, float vertex_spacing) const {
	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES);

	for (int z = 0; z < resolution; z++) {
		for (int x = 0; x < resolution; x++) {
			// Get the four corners of the quad
			float x0 = x * vertex_spacing;
			float z0 = z * vertex_spacing;
			float x1 = (x + 1) * vertex_spacing;
			float z1 = (z + 1) * vertex_spacing;

			float y00 = get_height(x0, z0);
			float y10 = get_height(x1, z0);
			float y01 = get_height(x0, z1);
			float y11 = get_height(x1, z1);

			// Triangle 1 (0,0 -> 1,0 -> 0,1)
			st->set_normal(Vector3(0, 1, 0));
			st->set_uv(Vector2(0, 0));
			st->add_vertex(Vector3(x0, y00, z0));

			st->set_normal(Vector3(0, 1, 0));
			st->set_uv(Vector2(1, 0));
			st->add_vertex(Vector3(x1, y10, z0));

			st->set_normal(Vector3(0, 1, 0));
			st->set_uv(Vector2(0, 1));
			st->add_vertex(Vector3(x0, y01, z1));

			// Triangle 2 (1,0 -> 1,1 -> 0,1)
			st->set_normal(Vector3(0, 1, 0));
			st->set_uv(Vector2(1, 0));
			st->add_vertex(Vector3(x1, y10, z0));

			st->set_normal(Vector3(0, 1, 0));
			st->set_uv(Vector2(1, 1));
			st->add_vertex(Vector3(x1, y11, z1));

			st->set_normal(Vector3(0, 1, 0));
			st->set_uv(Vector2(0, 1));
			st->add_vertex(Vector3(x0, y01, z1));
		}
	}
	st->generate_normals();

	return st->commit();
}

Ref<ArrayMesh> TerrainGenerator::generate_mesh(int resolution, float size) const {
	float vertex_spacing = size / resolution;
	return create_mesh_data(resolution, vertex_spacing);
}

} //namespace ts