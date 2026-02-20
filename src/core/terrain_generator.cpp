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

void TerrainGenerator::setup(const Ref<TerrainConfiguration> &p_config) {
	if (!p_config.is_valid()) {
		ERR_PRINT("TerrainGenerator: Invalid configuration provided");
		return;
	}

	_noise = p_config->get_noise();
	_height_scale = p_config->get_height_scale();

	if (!_noise.is_valid()) {
		WARN_PRINT("TerrainGenerator: No noise configured - terrain will be flat");
	}
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

	int vertex_count_per_row = resolution + 1;
	float offset = (resolution * vertex_spacing) * 0.5f;

	for (int z = 0; z < vertex_count_per_row; z++) {
		for (int x = 0; x < vertex_count_per_row; x++) {
			float x_pos = (x * vertex_spacing) - offset;
			float z_pos = (z * vertex_spacing) - offset;

			float u = static_cast<float>(x) / static_cast<float>(resolution);
			float v = static_cast<float>(z) / static_cast<float>(resolution);

			st->set_uv(Vector2(u, v));
			st->set_normal(Vector3(0, 1, 0));

			st->add_vertex(Vector3(x_pos, 0.0f, z_pos));
		}
	}

	for (int z = 0; z < resolution; z++) {
		for (int x = 0; x < resolution; x++) {
			int top_left = z * vertex_count_per_row + x;
			int top_right = top_left + 1;
			int bottom_left = (z + 1) * vertex_count_per_row + x;
			int bottom_right = bottom_left + 1;

			st->add_index(top_left);
			st->add_index(top_right);
			st->add_index(bottom_left);

			st->add_index(top_right);
			st->add_index(bottom_right);
			st->add_index(bottom_left);
		}
	}

	st->generate_tangents();
	return st->commit();
}

Ref<ArrayMesh> TerrainGenerator::generate_mesh(int resolution, float size) const {
	float vertex_spacing = size / resolution;
	return create_mesh_data(resolution, vertex_spacing);
}

} //namespace ts