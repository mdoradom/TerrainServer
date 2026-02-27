#include "terrain_generator.h"
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace ts {

void TerrainGenerator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("setup", "config"), &TerrainGenerator::setup);
	ClassDB::bind_method(D_METHOD("create_block_mesh", "resolution"), &TerrainGenerator::create_block_mesh);
	ClassDB::bind_method(D_METHOD("create_ring_fixup_mesh", "resolution"), &TerrainGenerator::create_ring_fixup_mesh);
}

TerrainGenerator::TerrainGenerator() : _height_scale(1.0f) {}

TerrainGenerator::~TerrainGenerator() {}

void TerrainGenerator::setup(const Ref<TerrainConfiguration> &p_config) {
	if (!p_config.is_valid()) {
		return;
	}
	_noise = p_config->get_noise();
	_height_scale = p_config->get_height_scale();
}


Ref<ArrayMesh> TerrainGenerator::create_block_mesh(int resolution) const {
	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES);

	int vertex_count = resolution + 1;
	float offset = 0.5f;

	for (int z = 0; z < vertex_count; z++) {
		for (int x = 0; x < vertex_count; x++) {
			float x_pos = ((float)x / resolution) - offset;
			float z_pos = ((float)z / resolution) - offset;

			st->set_uv(Vector2((float)x / resolution, (float)z / resolution));
			st->set_normal(Vector3(0, 1, 0));
			st->add_vertex(Vector3(x_pos, 0.0f, z_pos));
		}
	}

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

Ref<ArrayMesh> TerrainGenerator::create_ring_fixup_mesh(int resolution) const {
	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES);

	int vertex_count = resolution + 1;
	float offset = 0.5f;

	int hole_start = resolution / 4;
	int hole_end = (3 * resolution) / 4;

	for (int z = 0; z < vertex_count; z++) {
		for (int x = 0; x < vertex_count; x++) {
			float x_pos = ((float)x / resolution) - offset;
			float z_pos = ((float)z / resolution) - offset;

			st->set_uv(Vector2((float)x / resolution, (float)z / resolution));
			st->set_normal(Vector3(0, 1, 0));
			st->add_vertex(Vector3(x_pos, 0.0f, z_pos));
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