#pragma once

#include "core/terrain_configuration.h"
#include "core/terrain_renderer.h"
#include <godot_cpp/classes/node3d.hpp>

namespace ts {

class Terrain3D : public godot::Node3D {
	GDCLASS(Terrain3D, godot::Node3D);

private:
	godot::Ref<TerrainConfiguration> _config;
	godot::Ref<TerrainRenderer> _renderer;

protected:
	static void _bind_methods();
	void _on_config_changed();

public:
	Terrain3D();
	~Terrain3D();

	void _ready() override;
	void _notification(int p_what);

	void set_configuration(const godot::Ref<TerrainConfiguration> &p_config);
	godot::Ref<TerrainConfiguration> get_configuration() const;
};

} //namespace ts