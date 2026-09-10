# Runtime API

`Terrain3D` exposes query methods for reading the terrain back from code: sampling height and
climate at a point, and introspecting the current clipmap and collision state. All of them are
`const` and safe to call every frame. For the exact signature, parameters and return value of
every method, press **F1** in the editor and search `Terrain3D`; this page covers the parts
worth knowing before you reach for them, not a restatement of the reference.

## Climate and height queries

`get_height_at`, `get_temperature_at`, `get_moisture_at` and `get_biome_at` all take a
world-space XZ position and return a default value (`0.0`, or `null` for `get_biome_at`) if no
`configuration` is assigned. Two things are easy to miss:

- `get_height_at` samples noise directly on the CPU, independent of the collision grid entirely,
  so it's exact and stays valid arbitrarily far outside `TerrainConfiguration.physics_range`.
- `get_biome_at` returns the outright winner of the climate weighting described in
  `TerrainBiomeLayer`'s class reference (F1), not the blend the shader draws. Near a boundary
  the neighbouring layer may be almost equally visible on screen even though only one layer
  comes back here. See [Demo scene walkthrough](03-demo-scene-walkthrough.md) for a concrete
  set of overlapping windows.

## Worked example: spawning objects on the surface

```gdscript
@export var terrain: Terrain3D
@export var object_scene: PackedScene
@export var count := 20
@export var spread := 100.0

func scatter() -> void:
	for i in count:
		var xz := Vector2(randf_range(-spread, spread), randf_range(-spread, spread))
		var height := terrain.get_height_at(xz)
		var instance := object_scene.instantiate()
		add_child(instance)
		instance.global_position = Vector3(xz.x, height, xz.y)
```

Because `get_height_at` doesn't depend on the collision grid, this works even for a `spread`
larger than `TerrainConfiguration.physics_range`, useful for scattering decoration far beyond
where the physics body currently reaches.

## Clipmap and collision introspection

`Terrain3D` also exposes getters for the current clipmap layout (level count, extents, origins,
morph band start) and collision state (range, resolution, center, built range, height range,
whether a heightmap has been built at all). These mostly back the debug gizmos and the terrain
dock (see [Editor tools](05-editor-tools.md)). The one worth knowing about in your own code is
`is_collision_rebuild_pending()`: at most one regrid runs at a time, so the collision surface can
trail the focus point by a frame or two while one is in flight. That's by design, not a bug.

## Forcing a rebuild

`rebuild()` re-runs generation from the current configuration; this already happens
automatically on every configuration change, so it's rarely needed by hand. `reload_shader()` is
the one that actually matters day to day: the loaded shader is reused across ordinary
configuration edits, so `rebuild()` alone will *not* pick up a `terrain.gdshader` edit, and
`reload_shader()` is what re-reads it from disk. Both are also available as menu items and dock
buttons, see [Editor tools](05-editor-tools.md).
