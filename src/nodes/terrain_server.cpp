#include "terrain_server.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace ts {

void TerrainServer::_bind_methods() {
    // TODO Binding methods can be done here
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

}