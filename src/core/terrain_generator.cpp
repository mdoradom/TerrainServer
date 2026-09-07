#include "terrain_generator.h"

#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>

#include <vector>

using namespace godot;

namespace ts {

void TerrainGenerator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("setup", "config"), &TerrainGenerator::setup);
	ClassDB::bind_static_method("TerrainGenerator", D_METHOD("create_block_mesh", "resolution"), &TerrainGenerator::create_block_mesh);
	ClassDB::bind_static_method("TerrainGenerator", D_METHOD("create_ring_fixup_mesh", "resolution"), &TerrainGenerator::create_ring_fixup_mesh);
	ClassDB::bind_static_method("TerrainGenerator", D_METHOD("create_trim_mesh", "resolution", "dx", "dz"), &TerrainGenerator::create_trim_mesh);
}

TerrainGenerator::TerrainGenerator() = default;

TerrainGenerator::~TerrainGenerator() {}

void TerrainGenerator::setup(const Ref<TerrainConfiguration> &p_config) {
	if (!p_config.is_valid()) {
		ERR_PRINT("Invalid TerrainConfiguration provided to TerrainGenerator::setup");
		return;
	}
}

static void add_grid_vertex(SurfaceTool *st, int resolution, int x, int z) {
	constexpr float offset = 0.5f;
	const float u = static_cast<float>(x) / resolution;
	const float v = static_cast<float>(z) / resolution;

	st->set_uv(Vector2(u, v));
	st->set_normal(Vector3(0, 1, 0));
	st->add_vertex(Vector3(u - offset, 0.0f, v - offset));
}

// Create two triangles for a quad, alternating the diagonal direction
//
// tl--tr  tl--tr
// | \  |  |  / |
// |  \ |  | /  |
// bl--br  bl--br
static void add_quad(SurfaceTool *st, int tl, int tr, int bl, int br, int x, int z) {
	if ((x + z) % 2 == 0) {
		//   tl--tr
		//   |  / |
		//   | /  |
		//   bl--br
		st->add_index(tl);
		st->add_index(tr);
		st->add_index(br);
		st->add_index(tl);
		st->add_index(br);
		st->add_index(bl);
	} else {
		//   tl--tr
		//   | \  |
		//   |  \ |
		//   bl--br
		st->add_index(tl);
		st->add_index(tr);
		st->add_index(bl);
		st->add_index(tr);
		st->add_index(br);
		st->add_index(bl);
	}
}

Ref<ArrayMesh> TerrainGenerator::create_block_mesh(const int resolution) {
	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES);

	const int vertex_count = resolution + 1;

	for (int z = 0; z < vertex_count; z++) {
		for (int x = 0; x < vertex_count; x++) {
			add_grid_vertex(st.ptr(), resolution, x, z);
		}
	}

	for (int z = 0; z < resolution; z++) {
		for (int x = 0; x < resolution; x++) {
			int tl = z * vertex_count + x;
			int tr = tl + 1;
			int bl = (z + 1) * vertex_count + x;
			int br = bl + 1;
			add_quad(st.ptr(), tl, tr, bl, br, x, z);
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
	const int hole_end = (3 * resolution) / 4 + 1;

	for (int z = 0; z < vertex_count; z++) {
		for (int x = 0; x < vertex_count; x++) {
			add_grid_vertex(st.ptr(), resolution, x, z);
		}
	}

	for (int z = 0; z < resolution; z++) {
		for (int x = 0; x < resolution; x++) {
			if (x >= hole_start && x < hole_end && z >= hole_start && z < hole_end) {
				continue;
			}

			int tl = z * vertex_count + x;
			int tr = tl + 1;
			int bl = (z + 1) * vertex_count + x;
			int br = bl + 1;
			add_quad(st.ptr(), tl, tr, bl, br, x, z);
		}
	}

	st->generate_tangents();
	return st->commit();
}

Ref<ArrayMesh> TerrainGenerator::create_trim_mesh(const int resolution, const int p_dx, const int p_dz) {
	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES);

	const int vertex_count = resolution + 1;
	const int hole_start = resolution / 4;
	const int hole_end = (3 * resolution) / 4;

	const int gap_x = (p_dx == 0) ? hole_end : hole_start;
	const int gap_z = (p_dz == 0) ? hole_end : hole_start;

	std::vector<int> remap(vertex_count * vertex_count, -1);
	int next_index = 0;

	auto vertex_at = [&](const int x, const int z) {
		int &slot = remap[z * vertex_count + x];
		if (slot < 0) {
			add_grid_vertex(st.ptr(), resolution, x, z);
			slot = next_index++;
		}
		return slot;
	};

	for (int z = hole_start; z <= hole_end; z++) {
		for (int x = hole_start; x <= hole_end; x++) {
			if (x != gap_x && z != gap_z) {
				continue;
			}

			const int tl = vertex_at(x, z);
			const int tr = vertex_at(x + 1, z);
			const int bl = vertex_at(x, z + 1);
			const int br = vertex_at(x + 1, z + 1);

			add_quad(st.ptr(), tl, tr, bl, br, x, z);
		}
	}

	st->generate_tangents();
	return st->commit();
}

} //namespace ts