#pragma once

#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/variant/string.hpp>

namespace ts {

class TerrainServerEditorPlugin : public godot::EditorPlugin {
	GDCLASS(TerrainServerEditorPlugin, godot::EditorPlugin);

protected:
	static void _bind_methods();

public:
	godot::String _get_plugin_name() const override;

	void _enter_tree() override;
};

} //namespace ts
