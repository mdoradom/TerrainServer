# Installation

TerrainServer is a GDExtension plugin, there is nothing to compile or enable once it's in your
project, unless you're building from source.

## Requirements

Godot 4.5 or newer (tested on 4.7.1). A GDExtension bound against the 4.5 API cannot load in an
older editor.

## Install from a release

1. Download the latest release archive (for example, `terrain_server-v1.2.3.zip`) from the
   [GitHub Releases page](https://github.com/mdoradom/TerrainServer/releases).
2. Extract it into your project's root directory. This creates
   `res://addons/terrain_server/`, containing the prebuilt binaries for Linux, Windows and
   macOS plus the `.gdextension` file that tells Godot how to load them.
3. Restart Godot (or open the project for the first time). The extension loads automatically,
   there is no entry under **Project > Project Settings > Plugins** to toggle, since a
   GDExtension isn't a scripted editor plugin in that sense.

You should now be able to add a `Terrain3D` node from the Create Node dialog. Continue to
[Quick start](02-quick-start.md).

## Build from source

Building from source is only necessary if you want to modify the plugin itself, or need a
platform/architecture the release archive doesn't cover.

```sh
git clone --recurse-submodules https://github.com/mdoradom/TerrainServer.git
cd TerrainServer
scons platform=linux target=template_debug   # debug build
scons platform=linux target=template_release # release build
```

`platform` is one of `linux`, `windows`, `macos`, `ios`; `arch` defaults per-platform. If you
already have the repository cloned without submodules, fetch `godot-cpp` (pinned to its `4.5`
branch) with:

```sh
git submodule update --init --recursive
```

The build output lands in `demo/addons/terrain_server/bin/` as
`libterrain_server.<platform>.<target>[.<arch>].<ext>`, and
`demo/addons/terrain_server/terrain_server.gdextension` maps each platform/target/arch
combination to the matching file. To use a from-source build in your own project, copy the
whole `demo/addons/terrain_server/` directory (including the `.gdextension` file and your
rebuilt `bin/`) into your project's `res://addons/`.

**Never pass `dev_build=yes`.** It appends `.dev` to the output filename (for example,
`libterrain_server.linux.template_debug.dev.x86_64.so`), but no entry in
`terrain_server.gdextension` names that path, so Godot silently keeps loading whatever
non-`dev` binary is already on disk instead. There's no error: the build succeeds and simply has
no effect, which makes it easy to mistake a stale build for a fresh one. If you need a
debuggable build, use `scons platform=linux target=template_debug debug_symbols=yes
optimize=none` instead; that flag combination doesn't change the output filename.
