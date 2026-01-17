#pragma once

#include <godot_cpp/classes/node3d.hpp>

namespace ts {

class TerrainServer : public godot::Node3D {
	GDCLASS(TerrainServer, godot::Node3D);

private:
protected:
	static void _bind_methods();

public:
	TerrainServer();
	~TerrainServer();

	void _process(double delta) override;
};

} //namespace ts
