#include "terrain_server.h"

#include "godot_cpp/classes/engine.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace ts {

void TerrainServer::_bind_methods() {
	// TODO Binding methods can be done here
	ClassDB::bind_method(D_METHOD("get_configuration"), &TerrainServer::get_configuration);
	ClassDB::bind_method(D_METHOD("set_configuration", "config"), &TerrainServer::set_configuration);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "configuration", PROPERTY_HINT_RESOURCE_TYPE, "TerrainConfiguration"), "set_configuration", "get_configuration");

	//
	ClassDB::bind_method(D_METHOD("_update_generator"), &TerrainServer::_update_generator);
}
void TerrainServer::_update_generator() {
	if (_config.is_valid() and _generator.is_valid()) {
		_generator->setup(_config);
		UtilityFunctions::print("TerrainServer:: Generator configuration updated.");
	}
}

TerrainServer::TerrainServer() {
	_generator.instantiate();
	UtilityFunctions::print("TerrainServer: initialized");
}

TerrainServer::~TerrainServer() {
	UtilityFunctions::print("TerrainServer: destroyed");
}

void TerrainServer::_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	UtilityFunctions::print("Height at (0,0): ", _generator->get_height(0.0f, 0.0f));

}

Ref<TerrainConfiguration> TerrainServer::get_configuration() const {
	return _config;
}
void TerrainServer::set_configuration(const Ref<TerrainConfiguration> &p_config) {
	// If the same config, do nothing
	if (_config == p_config) {
		return;
	}

	// Disconnect from previous config signals
	if (_config.is_valid()) {
		_config->disconnect("changed", Callable(this, "_update_generator"));
	}

	// Set new config
	_config = p_config;

	// Connect to new config signals
	if (_config.is_valid()) {
		_config->connect("changed", Callable(this, "_update_generator"));
	}

	_update_generator();
}

} //namespace ts