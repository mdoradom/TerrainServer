#include "terrain_gizmo_plugins.h"

#include "nodes/terrain3d.h"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include <algorithm>

using namespace godot;

namespace ts {

namespace {

constexpr float REBUILD_MARGIN_FRACTION = 0.25f;
constexpr float FOCUS_MARKER_ARM = 1.0f;

Terrain3D *terrain_from_gizmo(const Ref<EditorNode3DGizmo> &p_gizmo, Transform3D &r_to_local) {
	if (p_gizmo.is_null()) {
		return nullptr;
	}

	Terrain3D *terrain = Object::cast_to<Terrain3D>(p_gizmo->get_node_3d());
	if (terrain == nullptr) {
		return nullptr;
	}

	const Transform3D global = terrain->get_global_transform();
	const Vector3 scale = global.basis.get_scale();
	if (Math::is_zero_approx(scale.x) || Math::is_zero_approx(scale.y) || Math::is_zero_approx(scale.z)) {
		return nullptr;
	}

	r_to_local = global.affine_inverse();
	return terrain;
}

void append_segment(PackedVector3Array &r_lines, const Transform3D &p_to_local, const Vector3 &p_from, const Vector3 &p_to) {
	r_lines.push_back(p_to_local.xform(p_from));
	r_lines.push_back(p_to_local.xform(p_to));
}

void append_square_xz(PackedVector3Array &r_lines, const Transform3D &p_to_local, const Vector2 &p_min_xz, const float p_side, const float p_y) {
	const float x0 = p_min_xz.x;
	const float z0 = p_min_xz.y;
	const float x1 = x0 + p_side;
	const float z1 = z0 + p_side;

	append_segment(r_lines, p_to_local, Vector3(x0, p_y, z0), Vector3(x1, p_y, z0));
	append_segment(r_lines, p_to_local, Vector3(x1, p_y, z0), Vector3(x1, p_y, z1));
	append_segment(r_lines, p_to_local, Vector3(x1, p_y, z1), Vector3(x0, p_y, z1));
	append_segment(r_lines, p_to_local, Vector3(x0, p_y, z1), Vector3(x0, p_y, z0));
}

void append_centered_square_xz(PackedVector3Array &r_lines, const Transform3D &p_to_local, const Vector2 &p_center, const float p_side, const float p_y) {
	append_square_xz(r_lines, p_to_local, p_center - Vector2(p_side, p_side) * 0.5f, p_side, p_y);
}

void append_box(PackedVector3Array &r_lines, const Transform3D &p_to_local, const Vector2 &p_center, const float p_side, const float p_min_y, const float p_max_y) {
	append_centered_square_xz(r_lines, p_to_local, p_center, p_side, p_min_y);
	append_centered_square_xz(r_lines, p_to_local, p_center, p_side, p_max_y);

	const float half = p_side * 0.5f;
	for (int corner = 0; corner < 4; corner++) {
		const float x = p_center.x + ((corner & 1) != 0 ? half : -half);
		const float z = p_center.y + ((corner & 2) != 0 ? half : -half);
		append_segment(r_lines, p_to_local, Vector3(x, p_min_y, z), Vector3(x, p_max_y, z));
	}
}

void append_cross_xz(PackedVector3Array &r_lines, const Transform3D &p_to_local, const Vector2 &p_center, const float p_arm, const float p_y) {
	append_segment(r_lines, p_to_local, Vector3(p_center.x - p_arm, p_y, p_center.y), Vector3(p_center.x + p_arm, p_y, p_center.y));
	append_segment(r_lines, p_to_local, Vector3(p_center.x, p_y, p_center.y - p_arm), Vector3(p_center.x, p_y, p_center.y + p_arm));
}

constexpr int LEVEL_MATERIAL_POOL = 10;

Ref<StandardMaterial3D> make_line_material(const Color &p_color) {
	Ref<StandardMaterial3D> material;
	material.instantiate();

	material->set_albedo(p_color);
	material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	material->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	material->set_render_priority(Material::RENDER_PRIORITY_MAX);
	material->set_flag(BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
	material->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);

	return material;
}

std::vector<Ref<StandardMaterial3D>> make_level_materials(const Color &p_near, const Color &p_far) {
	std::vector<Ref<StandardMaterial3D>> materials;
	materials.reserve(LEVEL_MATERIAL_POOL);

	for (int i = 0; i < LEVEL_MATERIAL_POOL; i++) {
		const float t = static_cast<float>(i) / static_cast<float>(LEVEL_MATERIAL_POOL - 1);
		materials.push_back(make_line_material(p_near.lerp(p_far, t)));
	}

	return materials;
}

} //namespace

// ============== Clipmap boundaries ==============

void TerrainClipmapGizmoPlugin::_bind_methods() {}

TerrainClipmapGizmoPlugin::TerrainClipmapGizmoPlugin() {
	_level_materials = make_level_materials(Color(0.35f, 0.85f, 1.0f, 0.9f), Color(0.55f, 0.3f, 0.95f, 0.9f));
}

bool TerrainClipmapGizmoPlugin::_has_gizmo(Node3D *p_for_node_3d) const {
	return cast_to<Terrain3D>(p_for_node_3d) != nullptr;
}

String TerrainClipmapGizmoPlugin::_get_gizmo_name() const {
	return "Terrain Clipmap";
}

void TerrainClipmapGizmoPlugin::_redraw(const Ref<EditorNode3DGizmo> &p_gizmo) {
	p_gizmo->clear();

	Transform3D to_local;
	const Terrain3D *terrain = terrain_from_gizmo(p_gizmo, to_local);
	if (terrain == nullptr) {
		return;
	}

	const int level_count = terrain->get_clipmap_level_count();
	if (level_count <= 0) {
		return;
	}

	for (int level = 0; level < level_count; level++) {
		PackedVector3Array lines;
		const Vector2 center = terrain->get_clipmap_level_origin(level);
		append_centered_square_xz(lines, to_local, center, terrain->get_clipmap_level_extent(level), 0.0f);
		p_gizmo->add_lines(lines, _level_materials[std::min(level, LEVEL_MATERIAL_POOL - 1)]);
	}
}

// ============== Collision grid ==============

void TerrainCollisionGizmoPlugin::_bind_methods() {}

TerrainCollisionGizmoPlugin::TerrainCollisionGizmoPlugin() {
	_volume_material = make_line_material(Color(0.35f, 1.0f, 0.55f, 0.9f));
	_margin_material = make_line_material(Color(1.0f, 0.45f, 0.3f, 0.85f));
	_pending_material = make_line_material(Color(1.0f, 0.85f, 0.2f, 1.0f));
}

bool TerrainCollisionGizmoPlugin::_has_gizmo(Node3D *p_for_node_3d) const {
	return cast_to<Terrain3D>(p_for_node_3d) != nullptr;
}

String TerrainCollisionGizmoPlugin::_get_gizmo_name() const {
	return "Terrain Collision";
}

void TerrainCollisionGizmoPlugin::_redraw(const Ref<EditorNode3DGizmo> &p_gizmo) {
	p_gizmo->clear();

	Transform3D to_local;
	const Terrain3D *terrain = terrain_from_gizmo(p_gizmo, to_local);

	// Nothing has been pushed to PhysicsServer3D yet, so there is no square to draw anywhere.
	if (terrain == nullptr || !terrain->is_collision_built()) {
		return;
	}

	const Vector2 center = terrain->get_collision_center();

	const float built_range = terrain->get_collision_built_range();
	if (built_range > 0.0f) {
		const Vector2 height_range = terrain->get_collision_height_range();

		PackedVector3Array lines;
		append_box(lines, to_local, center, built_range, height_range.x, height_range.y);
		p_gizmo->add_lines(lines, _volume_material);
	}

	const float margin = terrain->get_collision_range() * REBUILD_MARGIN_FRACTION;
	if (margin > 0.0f) {
		PackedVector3Array lines;
		append_centered_square_xz(lines, to_local, center, margin * 2.0f, 0.0f);

		p_gizmo->add_lines(lines, terrain->is_collision_rebuild_pending() ? _pending_material : _margin_material);
	}
}

// ============== Focus markers ==============

void TerrainFocusGizmoPlugin::_bind_methods() {}

TerrainFocusGizmoPlugin::TerrainFocusGizmoPlugin() {
	_raw_material = make_line_material(Color(1.0f, 0.35f, 0.8f, 0.95f));
}

bool TerrainFocusGizmoPlugin::_has_gizmo(Node3D *p_for_node_3d) const {
	return cast_to<Terrain3D>(p_for_node_3d) != nullptr;
}

String TerrainFocusGizmoPlugin::_get_gizmo_name() const {
	return "Terrain Focus";
}

void TerrainFocusGizmoPlugin::_redraw(const Ref<EditorNode3DGizmo> &p_gizmo) {
	p_gizmo->clear();

	Transform3D to_local;
	const Terrain3D *terrain = terrain_from_gizmo(p_gizmo, to_local);
	if (terrain == nullptr) {
		return;
	}

	const Vector3 focus = terrain->get_focus_position();
	const Vector2 focus_xz(focus.x, focus.z);

	PackedVector3Array focus_lines;
	append_cross_xz(focus_lines, to_local, focus_xz, FOCUS_MARKER_ARM, 0.0f);
	p_gizmo->add_lines(focus_lines, _raw_material);
}

} //namespace ts
