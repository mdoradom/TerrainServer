# TerrainServer

![Godot Engine](https://img.shields.io/badge/Godot-4.5+-478cbf?logo=godot-engine&logoColor=white)
![C++](https://img.shields.io/badge/Language-C++17-00599C?logo=c%2B%2B&logoColor=white)
![License](https://img.shields.io/badge/License-GNU%20GPLv3-blue.svg)
[![Build](https://github.com/mdoradom/TerrainServer/actions/workflows/build.yml/badge.svg)](https://github.com/mdoradom/TerrainServer/actions/workflows/build.yml)

A GDExtension plugin for Godot 4.5+ that renders an endless procedural terrain as a geometry
clipmap and keeps a matching heightmap collision body underneath it. Everything is derived from
noise at runtime; no heightmap asset is stored or streamed. It ships as the `Terrain3D` node
plus a set of `Resource` classes for shape, biome and slope tuning.

<!-- TODO: replace with a real capture of the demo scene (docs/manual/05-editor-tools.md and
     the demo scenes are good sources). Tracked as TODO.md task C9. -->
![Terrain Server rendering a clipmap terrain with biome and slope layers in the Godot editor](docs/images/hero.png)

## Features

- Real-time procedural generation driven by `TerrainConfiguration`
- Clipmap-based level-of-detail rendering
- Physics collision that tracks the rendered surface
- Biome (temperature/moisture) and slope-based layering
- Editor preview: live in-viewport terrain, debug gizmos, and a bottom-panel dock with stats, a
  Whittaker climate chart and a live probe

## Install from a release

1. Download the latest release archive (for example, `terrain_server-v1.2.3.zip`) from the
   [GitHub Releases page](https://github.com/mdoradom/TerrainServer/releases)
2. Extract it to your project root (creates `res://addons/terrain_server/`)
3. Restart Godot. The extension loads automatically, no Plugins-tab toggle needed

See [`docs/manual/01-installation.md`](docs/manual/01-installation.md) for details.

## Quick start

1. Add a `Terrain3D` node to your scene
2. Create a `TerrainConfiguration` resource and assign it to the node's `configuration` property
3. Point `focus_path` at your camera or player so the clipmap and collision follow it
4. Add `TerrainBiomeLayer` / `TerrainSlopeLayer` resources to shade by climate and slope

The defaults alone are enough to see terrain, see
[`docs/manual/02-quick-start.md`](docs/manual/02-quick-start.md) for a full walkthrough.

## Build from source

```sh
git clone --recurse-submodules https://github.com/mdoradom/TerrainServer.git
cd TerrainServer
scons platform=linux target=template_debug   # or template_release
```

`platform` is one of `linux`, `windows`, `macos`, `ios`. The resulting binary lands in
`demo/addons/terrain_server/bin/`, matched by
`demo/addons/terrain_server/terrain_server.gdextension`. If you already cloned without
submodules, run `git submodule update --init --recursive` first. See
[`docs/manual/01-installation.md`](docs/manual/01-installation.md) for the full build-from-source
guide, including a build flag to avoid that silently produces a binary Godot won't load.

## Documentation

The manual lives under [`docs/manual/`](docs/manual/):

1. [Installation](docs/manual/01-installation.md)
2. [Quick start](docs/manual/02-quick-start.md)
3. [Demo scene walkthrough](docs/manual/03-demo-scene-walkthrough.md)
4. [Runtime API](docs/manual/04-runtime-api.md)
5. [Editor tools](docs/manual/05-editor-tools.md)

The in-editor class reference (F1 in Godot) covers every bound class, property and method in
detail; the manual above is the narrative guide to using them together.

## Platform support

- ✅ Linux x86_64
- ✅ Windows x86_64
- ✅ macOS (Universal)

## Requirements

Godot 4.5+ (tested on 4.7.1).

## License

[GNU GPLv3](LICENSE).
