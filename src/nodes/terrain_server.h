#pragma once

#include "core/terrain_configuration.h"
#include <godot_cpp/classes/node3d.hpp>

using namespace godot;

namespace ts {

class TerrainServer : public godot::Node3D {
	GDCLASS(TerrainServer, godot::Node3D);

private:
	Ref<TerrainConfiguration> _confi;

protected:
	static void _bind_methods();

public:
	TerrainServer();
	~TerrainServer();

	void _process(double delta) override;

	Ref<TerrainConfiguration> get_configuration() const;
	void set_configuration(const Ref<TerrainConfiguration> &p_config);
};

} //namespace ts
