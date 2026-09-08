# Quick start

This walks through getting a terrain on screen from an empty scene. If you'd rather look at a
finished example first, open `demo/terrain_server.tscn` in the demo project, it's the working
reference this guide describes.

## 1. Add a `Terrain3D` node

In the Scene dock, add a `Terrain3D` node anywhere in your scene tree, it doesn't need a
parent of any particular type. At this point it renders nothing: `Terrain3D` holds no terrain
logic of its own until it's given a configuration.

## 2. Create and assign a `TerrainConfiguration`

Select the node, and in the Inspector click into the `configuration` property and choose
**New TerrainConfiguration**. Save it as a `.tres` file so it can be reused across scenes.

As soon as it's assigned, terrain appears. The defaults alone (256×256 world units,
6 clipmap levels, a default noise fractal) are enough to see something. Every property is
documented in the in-editor class reference (press **F1** and search `TerrainConfiguration`);
edit any of them, or any `TerrainBiomeLayer`/`TerrainSlopeLayer` inside the configuration, and
the terrain rebuilds immediately, in the editor and at runtime, since editing a `Resource` emits
its `changed` signal.

## 3. Point `focus_path` at a camera or player

The clipmap and the collision heightmap both re-center themselves each frame around a single
*focus point*. With `focus_path` left empty, `Terrain3D` falls back to the active viewport's
`Camera3D`, which is enough to see the terrain react as you fly around the editor. For a real
scene, add a `Camera3D` (or your player's root node) and set `focus_path` to point at it. This
is what keeps the finest geometry and the collision grid under the player rather than under
whatever the current camera happens to be, and it's the only source that works with no camera
at all (useful on a dedicated server). See
[Runtime API](04-runtime-api.md) and the class reference for the full three-source resolution
order.

## 4. Run the scene

Press Play. The terrain should render exactly as it did in the editor preview, now with a
physics collision body underneath it that regrids as the focus point moves.

## Going further

- [Demo scene walkthrough](03-demo-scene-walkthrough.md): a worked example built from the
  actual packaged demo scene, tuned climate and slope layers, extra lighting and fog, and a
  physics test.
- [Runtime API](04-runtime-api.md): reading height, biome and climate back from code.
- [Editor tools](05-editor-tools.md): the bottom-panel dock, debug gizmos and Tools-menu
  actions that make tuning the above practical.
