# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

TerrainServer is a GDExtension (C++17) plugin for Godot 4.5+ that implements procedural clipmap-based terrain: generation, rendering, and physics, exposed to Godot as the `Terrain3D` node plus supporting `Resource`/`RefCounted` classes. Built against `godot-cpp` (git submodule pinned to branch `4.5`).

## Build commands

Build from the repo root with SCons (godot-cpp's SConstruct is included via `SConscript`):

```sh
scons platform=linux target=template_debug   # debug build (also used by CI)
scons platform=linux target=template_release # release build
```

`platform` is one of `linux`, `windows`, `macos`, `ios`; `arch` defaults per-platform (CI builds `x86_64`, macos `universal`). Output goes to `demo/addons/terrain_server/bin/` as `libterrain_server.<platform>.<target>[.<arch>].<ext>`, matched by `demo/addons/terrain_server/terrain_server.gdextension`.

**After adding a new C++ class to the plugin, regenerate `compile_commands.json`:**

```sh
scons compiledb=yes
```

There is no test suite in this repo. Validate changes by building successfully and, for rendering/behavior changes, opening `demo/project.godot` in the Godot editor and running the demo scene (`demo/terrain_server.tscn`).

Submodule setup (first clone / if `godot-cpp` is empty):

```sh
git submodule update --init --recursive
```

## Architecture

`Terrain3D` (`src/nodes/terrain3d.*`) is the Godot-facing `Node3D`. It owns one instance each of three collaborator objects and wires them together — it holds no terrain logic itself:

- **`TerrainConfiguration`** (`src/core/terrain_configuration.*`) — a `Resource` holding all tunable parameters (height scale, mesh resolution, terrain size, noise octaves/frequency/lacunarity/gain, clipmap level count, albedo texture). Assigned via the node's `configuration` property in the editor. Emits Godot's built-in `changed` signal on edit.
- **`TerrainGenerator`** (`src/core/terrain_generator.*`) — a `RefCounted` that builds the base clipmap block mesh and ring "fixup" mesh (`create_block_mesh`, `create_ring_fixup_mesh`) used at every clipmap level.
- **`TerrainRenderer`** (`src/core/terrain_renderer.*`) — a `RefCounted` that owns all `RenderingServer` RIDs directly (mesh, ring mesh, shader, and one instance+material per clipmap level) rather than going through Godot scene nodes. Builds the clipmap ring hierarchy in `rebuild_mesh()` and re-snaps/re-scales each level around the camera in `update_camera_position()`.
- **`TerrainPhysics`** (`src/core/terrain_physics.*`) — currently a stub (`initialize`/`cleanup`/`set_configuration`/`update_camera_position` with empty bodies), intended to mirror the renderer's per-level lifecycle for collision.

Data flow: `Terrain3D::set_configuration()` connects to the config's `changed` signal → `_on_config_changed()` calls `_generator->setup(config)`, `_physics->set_configuration(config)`, `_renderer->set_generator/set_configuration`, then `_renderer->rebuild_mesh()`. Every frame, `Terrain3D::_process()` reads the active viewport's `Camera3D` position and forwards it to `_renderer->update_camera_position()` (and should also drive physics once implemented) to keep clipmap rings centered on the camera.

Lifecycle: renderer/physics RIDs and state are set up in `_ready()` and `NOTIFICATION_ENTER_TREE`, torn down in the destructor and `NOTIFICATION_EXIT_TREE` — always via each collaborator's `cleanup()`, since they own raw `RenderingServer`/`PhysicsServer` RIDs that are not automatically freed.

Terrain shading lives in `demo/addons/terrain_server/shaders/terrain.gdshader`, loaded at runtime by `TerrainRenderer::rebuild_mesh()` via `FileAccess::get_file_as_string("res://addons/terrain_server/shaders/terrain.gdshader")` — it is not compiled into the extension, so shader edits take effect without a rebuild.

New classes registered in `src/register_types.cpp` must also be `GDREGISTER_CLASS`'d there and forward-declared/included, or Godot won't see them.

## Code style

Formatting is enforced by `.clang-format` (LLVM base, tabs, 4-space `AccessModifierOffset`) — run `clang-format` before committing C++ changes. All plugin code lives under the `ts` namespace; Godot types are referenced via `godot::` or `using namespace godot;` per existing file conventions.
