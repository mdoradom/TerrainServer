# Editor tools

The plugin adds a bottom-panel dock, three independently-toggleable debug gizmos, and two
Tools-menu actions, all installed by `TerrainServerEditorPlugin` while an editor is open. None
of this exists in an exported game, it's editor-only tooling for tuning a terrain in place.

## The Terrain dock

<!-- TODO: replace with a real capture of the dock (stats row, Whittaker chart, live probe).
     Tracked as TODO.md task C9. -->
![The Terrain dock's stats row, Whittaker chart and live probe](../images/editor-dock.png)

Open the bottom panel's **Terrain** tab with a `Terrain3D` node selected (it falls back to the
first terrain in the scene if the current selection isn't terrain-related). It refreshes on
selection change and twice a second, showing:

- **Clipmap**: level count, the world-space extent of each level, the snapped clipmap center,
  and the morph band range.
- **Collision**: configured range/resolution, the collision center, and whether a regrid is
  currently pending.
- **Configuration**: `mesh_resolution`, `clipmap_levels`, and the number of biome layers
  assigned.
- **Local height sample**: a small grayscale preview sampled from `get_height_at` over a
  window centered on the focus point (a quarter of the finest clipmap level's extent), with a
  yellow pixel marking the focus point itself.
- **Live probe**: height, temperature, moisture and biome name at the current focus position,
  read straight from `get_height_at`/`get_temperature_at`/`get_moisture_at`/`get_biome_at`.

Below the stats is an embedded **Biome climate** chart (the Whittaker chart): a
temperature-by-moisture plot with one colored, labeled rectangle per `TerrainBiomeLayer` in the
assigned configuration, spanning that layer's `min/max_temperature` × `min/max_moisture`
window, plus a live dot marking the current probe position. This is the fastest way to spot a
coverage gap between layers (see the overlapping windows in
[Demo scene walkthrough](03-demo-scene-walkthrough.md)): a gap in the plotted rectangles is a
gap in biome coverage on the terrain itself.

Two buttons at the bottom act on the currently-tracked terrain: **Rebuild** (calls `rebuild()`)
and **Reload shader** (calls `reload_shader()`). See [Runtime API](04-runtime-api.md) for the
difference between the two.

## Debug gizmos (View > Gizmos)

Three `EditorNode3DGizmoPlugin`s are registered, each with its own independent toggle under the
3D viewport's **View > Gizmos** menu:

- **Terrain Clipmap**: draws one centered square outline per clipmap level, colored on a
  gradient from cyan (nearest/finest) to purple (farthest/coarsest).
- **Terrain Collision**: draws a wireframe box around the collision area actually built (green),
  plus a flat margin square around the collision center marking the rebuild-trigger distance.
  That margin square turns from orange to yellow while a regrid is in flight
  (`is_collision_rebuild_pending()`), so you can watch regrids happen as you move the focus
  point. Only drawn once a collision heightmap has been built at least once.
- **Terrain Focus**: a small magenta cross marking the currently-resolved focus position.

## Tools menu

Two items appear under **Tools** in the main editor menu (they act on every `Terrain3D` node in
the currently open scene):

- **Reload Terrain Shader**: re-reads `terrain.gdshader` from disk. Use this after editing the
  shader file; ordinary configuration edits don't need it, and it's more heavyweight than
  **Rebuild Terrain**.
- **Rebuild Terrain**: re-runs generation and rebuilds the clipmap mesh from the current
  configuration, without touching the loaded shader. Rarely needed by hand, since any
  configuration edit already triggers this automatically.

## `editor_preview` and `editor_preview_physics`

Two `Terrain3D` properties control what runs while editing, with no effect once the game is
actually running (both the renderer and physics always run at runtime):

- **`editor_preview`** (default `true`): whether the terrain renders in the editor viewport at
  all. Turning it off frees the renderer entirely, which is worth doing on a scene heavy enough
  to slow down editing.
- **`editor_preview_physics`** (default `false`): whether the collision body is also generated
  in-editor, which is what gives the Terrain Collision gizmo something to draw. It's off by
  default because enabling it makes the collision regrid run continuously as you fly the editor
  camera around, so only turn it on while actively debugging collision. It has no effect unless
  `editor_preview` is also on.

While editing, the plugin also drives the terrain's *focus point* itself: every frame, it pushes
the 3D viewport's fly-camera position as an override (see the focus resolution order in
[Quick start](02-quick-start.md)), so the clipmap, and the collision grid if
`editor_preview_physics` is on, follow you around the scene as you fly, without needing
`focus_path` set.
