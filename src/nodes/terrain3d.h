#pragma once

#include "core/terrain_biome_layer.h"
#include "core/terrain_configuration.h"
#include "core/terrain_generator.h"
#include "core/terrain_physics.h"
#include "core/terrain_renderer.h"

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/node_path.hpp>

#include <vector>

namespace ts {

class Terrain3D : public godot::Node3D {
	GDCLASS(Terrain3D, godot::Node3D);

private:
	godot::Ref<TerrainConfiguration> _config;
	godot::Ref<TerrainRenderer> _renderer;
	godot::Ref<TerrainGenerator> _generator;
	godot::Ref<TerrainPhysics> _physics;
	godot::NodePath _focus_path;

	godot::Vector3 _editor_focus_override;
	bool _has_editor_focus_override = false;
	bool _editor_preview = true;
	bool _editor_preview_physics = false;

	static std::vector<Terrain3D *> _editor_instances;

protected:
	static void _bind_methods();
	void _on_config_changed();

public:
	Terrain3D();
	~Terrain3D();

	void _ready() override;
	void _process(double delta) override;
	void _notification(int p_what);

	void set_configuration(const godot::Ref<TerrainConfiguration> &p_config);
	godot::Ref<TerrainConfiguration> get_configuration() const;
	float get_height_at(godot::Vector2 p_world_xz) const;
	godot::Ref<TerrainBiomeLayer> get_biome_at(godot::Vector2 p_world_xz) const;
	float get_temperature_at(godot::Vector2 p_world_xz) const;
	float get_moisture_at(godot::Vector2 p_world_xz) const;

	int get_clipmap_level_count() const;
	float get_clipmap_level_extent(int p_level) const;
	godot::Vector2 get_clipmap_center() const;

	float get_collision_range() const;
	int get_collision_resolution() const;
	godot::Vector2 get_collision_center() const;
	float get_collision_built_range() const;
	int get_collision_built_resolution() const;
	godot::Vector2 get_collision_height_range() const;
	bool is_collision_built() const;
	bool is_collision_rebuild_pending() const;

	void set_focus_path(const godot::NodePath &p_path);
	godot::NodePath get_focus_path() const;

	void set_editor_focus_override(godot::Vector3 p_world_position);
	void clear_editor_focus_override();

	void set_editor_preview(bool p_enabled);
	bool get_editor_preview() const;
	void set_editor_preview_physics(bool p_enabled);
	bool get_editor_preview_physics() const;

	static const std::vector<Terrain3D *> &get_editor_instances();
};

} //namespace ts