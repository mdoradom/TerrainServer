#include "terrain_generator.h"

#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>

using namespace godot;

namespace ts {

void TerrainGenerator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("setup", "config"), &TerrainGenerator::setup);
	ClassDB::bind_static_method("TerrainGenerator", D_METHOD("create_block_mesh", "resolution"), &TerrainGenerator::create_block_mesh);
	ClassDB::bind_static_method("TerrainGenerator", D_METHOD("create_ring_fixup_mesh", "resolution"), &TerrainGenerator::create_ring_fixup_mesh);

}

TerrainGenerator::TerrainGenerator() : _height_scale(1.0f) {}

TerrainGenerator::~TerrainGenerator() {}

void TerrainGenerator::setup(const Ref<TerrainConfiguration> &p_config) {
	if (!p_config.is_valid()) {
		ERR_PRINT("Invalid TerrainConfiguration provided to TerrainGenerator::setup");
		return;
	}
	_noise = p_config->get_noise();
	_height_scale = p_config->get_height_scale();
}

Ref<ArrayMesh> TerrainGenerator::create_block_mesh(const int resolution) {
	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES);

	const int vertex_count = resolution + 1;

	for (int z = 0; z < vertex_count; z++) {
		for (int x = 0; x < vertex_count; x++) {
			const float offset = 0.5f;
			float x_pos = (static_cast<float>(x) / resolution) - offset;
			float z_pos = (static_cast<float>(z) / resolution) - offset;

			st->set_uv(Vector2(static_cast<float>(x) / resolution, static_cast<float>(z) / resolution));
			st->set_normal(Vector3(0, 1, 0));
			st->add_vertex(Vector3(x_pos, 0.0f, z_pos));
		}
	}

	// Draw two triangles per quad in the following order:
	// tl----------tr
	// |         / |
	// |  1    /   |
	// |     /     |
	// |   /    2  |
	// | /         |
	// bl----------br
	// Triangle 1: tl -> tr -> bl
	// Triangle 2: tr -> br -> bl
	for (int z = 0; z < resolution; z++) {
		for (int x = 0; x < resolution; x++) {
			int tl = z * vertex_count + x;
			int tr = tl + 1;
			int bl = (z + 1) * vertex_count + x;
			int br = bl + 1;
			st->add_index(tl);
			st->add_index(tr);
			st->add_index(bl);
			st->add_index(tr);
			st->add_index(br);
			st->add_index(bl);
		}
	}

	st->generate_tangents();
	return st->commit();
}

Ref<ArrayMesh> TerrainGenerator::create_ring_fixup_mesh(int resolution) {
	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES);

	const int vertex_count = resolution + 1;

	const int hole_start = resolution / 4;
	const int hole_end = (3 * resolution) / 4;

	for (int z = 0; z < vertex_count; z++) {
		for (int x = 0; x < vertex_count; x++) {
			const float offset = 0.5f;
			float x_pos = (static_cast<float>(x) / resolution) - offset;
			float z_pos = (static_cast<float>(z) / resolution) - offset;

			st->set_uv(Vector2(static_cast<float>(x) / resolution, static_cast<float>(z) / resolution));
			st->set_normal(Vector3(0, 1, 0));
			st->add_vertex(Vector3(x_pos, 0.0f, z_pos));
		}
	}

	// Draw two triangles per quad in the following order:
	// tl----------tr
	// |         / |
	// |  1    /   |
	// |     /     |
	// |   /    2  |
	// | /         |
	// bl----------br
	// Triangle 1: tl -> tr -> bl
	// Triangle 2: tr -> br -> bl
	for (int z = 0; z < resolution; z++) {
		for (int x = 0; x < resolution; x++) {
			if (x >= hole_start && x < hole_end && z >= hole_start && z < hole_end) {
				continue;
			}

			int tl = z * vertex_count + x;
			int tr = tl + 1;
			int bl = (z + 1) * vertex_count + x;
			int br = bl + 1;

			st->add_index(tl);
			st->add_index(tr);
			st->add_index(bl);
			st->add_index(tr);
			st->add_index(br);
			st->add_index(bl);
		}
	}

	st->generate_tangents();
	return st->commit();
}

} //namespace ts