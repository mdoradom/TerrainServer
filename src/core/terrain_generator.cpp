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
	if (!p_config.is_valid()) return;
	_noise = p_config->get_noise();
	_height_scale = p_config->get_height_scale();
}

float TerrainGenerator::get_height(float x, float y) const {
	if (!_noise.is_valid()) return 0.0f;
	return _noise->get_noise_2d(x, y) * _height_scale;
}

Ref<ArrayMesh> TerrainGenerator::generate_mesh(int resolution, float size) const {
	return create_mesh_data(resolution, size / static_cast<float>(resolution));
}

Ref<ArrayMesh> TerrainGenerator::create_mesh_data(int resolution, float vertex_spacing, bool has_hole) const {
	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES);

	int vertex_count = resolution + 1;
	float offset = (resolution * vertex_spacing) * 0.5f;

	int hole_start = (resolution / 4) + 2;
	int hole_end = (3 * resolution / 4) - 2;

	for (int z = 0; z < vertex_count; z++) {
		for (int x = 0; x < vertex_count; x++) {
			float x_pos = (x * vertex_spacing) - offset;
			float z_pos = (z * vertex_spacing) - offset;

			float drop = 0.0f;

			if (x == 0 || x == resolution || z == 0 || z == resolution) {
				drop = -1.0f;
			}

			if (has_hole) {
				bool is_inner_x = (x == hole_start || x == hole_end) && (z >= hole_start && z <= hole_end);
				bool is_inner_z = (z == hole_start || z == hole_end) && (x >= hole_start && x <= hole_end);
				if (is_inner_x || is_inner_z) {
					drop = -1.0f;
				}
			}

			st->set_uv(Vector2((float)x / resolution, (float)z / resolution));
			st->set_normal(Vector3(0, 1, 0));
			st->add_vertex(Vector3(x_pos, drop, z_pos));
		}
	}

	for (int z = 0; z < resolution; z++) {
		for (int x = 0; x < resolution; x++) {
			if (has_hole && x >= hole_start && x < hole_end && z >= hole_start && z < hole_end) {
				continue;
			}
			int tl = z * vertex_count + x;
			int tr = tl + 1;
			int bl = (z + 1) * vertex_count + x;
			int br = bl + 1;

			st->add_index(tl); st->add_index(bl); st->add_index(tr);
			st->add_index(tr); st->add_index(bl); st->add_index(br);
		}
	}

	st->generate_tangents();
	return st->commit();
}

} //namespace ts