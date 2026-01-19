#include "terrain_server.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace ts {

void TerrainServer::_bind_methods() {
	// TODO Binding methods can be done here
	ClassDB::bind_method(D_METHOD("get_configuration"), &TerrainServer::get_configuration);
	ClassDB::bind_method(D_METHOD("set_configuration", "config"), &TerrainServer::set_configuration);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "configuration", PROPERTY_HINT_RESOURCE_TYPE, "TerrainConfiguration"), "set_configuration", "get_configuration");
}

TerrainServer::TerrainServer() {
	UtilityFunctions::print("TerrainServer: initialized");
}

TerrainServer::~TerrainServer() {
	UtilityFunctions::print("TerrainServer: destroyed");
}

void TerrainServer::_process(double delta) {
	// TODO Implement per-frame logic here
}

Ref<TerrainConfiguration> TerrainServer::get_configuration() const {
	return _confi;
}
void TerrainServer::set_configuration(const Ref<TerrainConfiguration> &p_config) {
	_confi = p_config;
}

} //namespace ts