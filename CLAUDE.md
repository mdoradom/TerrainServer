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

**Never build with `dev_build=yes`.** It appends `.dev` to the filename — `libterrain_server.linux.template_debug.dev.x86_64.so` — and no `.gdextension` entry names that path, so Godot silently keeps loading whatever stale non-dev `.so` is already on disk. There is no error: the build succeeds and simply has no effect. Use `debug_symbols=yes optimize=none` for a debuggable build instead; those do not change the suffix. Note that CLion's "Build Godot debug" play button is configured to run `scons dev_build=yes -j8` and therefore never produces a binary Godot loads — build from the terminal instead. When a C++ or doc change appears to do nothing, check the mtime of the file the `.gdextension` actually names before suspecting the code.

**After adding a new C++ class to the plugin, regenerate `compile_commands.json`:**

```sh
scons compiledb=yes
```

**After adding or changing anything bound in a `_bind_methods`, refresh the class reference:**

```sh
godot --headless --path demo --doctool "$PWD" --gdextension-docs
```

This rewrites `doc_classes/*.xml` from `ClassDB`, so the extension must already be built.
It merges rather than overwrites: hand-written descriptions survive, new bindings appear as
empty `<description>` blocks to fill in, removed ones disappear, and escaping is normalised.
The XML is compiled into the binary, so a doc edit only takes effect after a `scons` rebuild
**and an editor restart** — extension doc data is read once at editor startup.

There is no test suite in this repo. Validate changes by building successfully and, for rendering/behavior changes, opening `demo/project.godot` in the Godot editor and running the demo scene (`demo/terrain_server.tscn`).

Submodule setup (first clone / if `godot-cpp` is empty):

```sh
git submodule update --init --recursive
```

## Architecture

`Terrain3D` (`src/nodes/terrain3d.*`) is the Godot-facing `Node3D`. It owns one instance each of three collaborator objects and wires them together — it holds no terrain logic itself:

- **`TerrainConfiguration`** (`src/core/terrain_configuration.*`) — a `Resource` holding all tunable parameters (height scale, mesh resolution, terrain size, noise octaves/frequency/lacunarity/gain, clipmap level count, albedo texture). Assigned via the node's `configuration` property in the editor. Emits Godot's built-in `changed` signal on edit.
- **`TerrainGenerator`** (`src/core/terrain_generator.*`) — a `RefCounted` that builds the base clipmap block mesh and ring "fixup" mesh (`create_block_mesh`, `create_ring_fixup_mesh`) used at every clipmap level.
- **`TerrainRenderer`** (`src/core/terrain_renderer.*`) — a `RefCounted` that owns all `RenderingServer` RIDs directly (mesh, ring mesh, shader, and one instance+material per clipmap level) rather than going through Godot scene nodes. Builds the clipmap ring hierarchy in `rebuild_mesh()` and re-snaps/re-scales each level around the current focus point in `update_focus_position()`.
- **`TerrainPhysics`** (`src/core/terrain_physics.*`) — a `RefCounted` that owns a static `PhysicsServer3D` heightmap collision body. `update_focus_position()` snaps to a grid cell and, once the focus point has moved past a rebuild margin, kicks off a regrid of the heightmap over a `physics_range`-sized square centered on it: `_compute_heightmap_task()` evaluates `TerrainNoise` (`src/core/terrain_noise.h`, a CPU port of the shader's noise/fBm functions shared with `TerrainRenderer` so collision matches the rendered surface) on a `WorkerThreadPool` task, and `_apply_heightmap()` pushes the result to `PhysicsServer3D` back on the main thread once that task completes — at most one regrid is in flight at a time, so the collision surface can lag the focus point by a frame or two while one is running. `get_height_at()` samples `TerrainNoise` directly (independent of the collision grid), backing `Terrain3D::get_height_at()`.

The editor surface lives in `src/editor/`, registered only at `MODULE_INITIALIZATION_LEVEL_EDITOR` in `src/register_types.cpp` (compiled into every build, including `template_release`, but only initialized when an actual editor loads the extension). `TerrainServerEditorPlugin` (`terrain_server_editor_plugin.*`) is installed via `EditorPlugins::add_by_type()`; its `_enter_tree()` installs three `EditorNode3DGizmoPlugin` subclasses from `terrain_gizmo_plugins.*` (clipmap level boundaries, the physics collision grid, and focus-point markers — one plugin per overlay so each gets its own View > Gizmos toggle) and the bottom panel `TerrainDock` (`terrain_dock.*`, backed by `WhittakerChart` in `whittaker_chart.*` for the temperature/moisture climate plot). The gizmo plugins, the dock, the chart and the editor plugin itself are all `GDREGISTER_INTERNAL_CLASS`'d so they never appear in the Create Node dialog. The plugin also drives editor-camera following (see Data flow below) and hosts the `Tools > Reload Terrain Shader` / `Rebuild Terrain` menu items.

Data flow: `Terrain3D::set_configuration()` connects to the config's `changed` signal → `_on_config_changed()` calls `_generator->setup(config)`, `_physics->set_configuration(config)`, `_renderer->set_configuration(config)`, then `_renderer->rebuild_mesh()`. Every frame, `Terrain3D::_process()` resolves a single focus position and forwards it to both `_renderer->update_focus_position()` (clipmap centering) and `_physics->update_focus_position()` (heightmap regrid), trying three sources in order: the `focus_path` property's `Node3D` global position if set; otherwise, in the editor, a focus override pushed each frame by `TerrainServerEditorPlugin` from the editor viewport's fly camera; otherwise the active viewport's `Camera3D` position. This keeps renderer and physics in sync with each other, lets `focus_path` win over editor-camera following when a specific node should be tracked instead, and — since `focus_path` doesn't require a camera — still works on a dedicated server or when the tracked player differs from the viewport camera.

Lifecycle: renderer/physics RIDs and state are set up in `_ready()` and `NOTIFICATION_ENTER_TREE`, torn down in the destructor and `NOTIFICATION_EXIT_TREE` — always via each collaborator's `cleanup()`, since they own raw `RenderingServer`/`PhysicsServer` RIDs that are not automatically freed.

Terrain shading lives in `demo/addons/terrain_server/shaders/terrain.gdshader`, loaded at runtime by `TerrainRenderer::rebuild_mesh()` via `FileAccess::get_file_as_string("res://addons/terrain_server/shaders/terrain.gdshader")` — it is not compiled into the extension, so shader edits take effect without a `scons` rebuild. The shader RID itself is only (re)loaded from disk on a full renderer teardown/setup (`cleanup()` + `rebuild_mesh()`, e.g. the node re-entering the tree or a scene reload) by default — `rebuild_mesh()` otherwise reuses the already-loaded shader RID across ordinary `TerrainConfiguration` edits. `TerrainRenderer::request_shader_reload()` (exposed as `Terrain3D::reload_shader()`, wired to the dock button and the Tools-menu item) forces a reload on demand instead: it sets a dirty flag that `rebuild_mesh()` consumes after freeing the old mesh instances and before reloading, so a live shader edit can be picked up without a full scene reload.

The in-editor class reference lives in `doc_classes/*.xml`. `SConstruct` runs godot-cpp's `GodotCPPDocData` builder over them into `gen/doc_data.gen.cpp` (gitignored; kept out of `src/` because `SConstruct` walks that tree for `.cpp` and would compile it twice), gated to `template_debug` — the target an editor build of the `.gdextension` resolves to. Only the four public classes carry full docs; `TerrainGenerator`, `TerrainRenderer` and `TerrainPhysics` are `GDREGISTER_CLASS`'d so doctool emits skeletons for them too, and they carry a class description only. Ranges and defaults in the XML come from `_bind_methods` — that is the source of truth, never invent one. Nothing but F1 in a real editor verifies the result: `--headless` does not load class docs, and `--doctool` ignores extension-registered doc data.

New classes registered in `src/register_types.cpp` must also be `GDREGISTER_CLASS`'d (or `GDREGISTER_INTERNAL_CLASS`'d for editor-only classes) there and forward-declared/included, or Godot won't see them.

## Code style

Formatting is enforced by `.clang-format` (LLVM base, tabs, 4-space `AccessModifierOffset`) — run `clang-format` before committing C++ changes. All plugin code lives under the `ts` namespace; Godot types are referenced via `godot::` or `using namespace godot;` per existing file conventions.
