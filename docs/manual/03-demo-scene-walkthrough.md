# Demo scene walkthrough

[Quick start](02-quick-start.md) gets a terrain on screen with default values. This page walks
through the same node, tuned instead like `demo/terrain_server.tscn`, the packaged demo scene:
overlapping climate bands, a rock layer, extra lighting and fog, and a small physics test. Open
that scene alongside this page if you want to follow along with the real files. For every
property's exact meaning, range and default, press **F1** in the editor and look it up in the
class reference; this page is about *why* these particular numbers were picked.

[[TODO: Review this and actually build the scene in the demo project, then add screenshots.]]

## 1. Scene setup

The scene root is a plain `Node3D` holding four children: a `WorldEnvironment`, a
`DirectionalLight3D`, a `Camera3D`, and the `Terrain3D` node itself.

The `WorldEnvironment` uses a `ProceduralSkyMaterial` sky and turns on ambient occlusion, glow,
and height fog (`fog_height_density = 0.4077`, `fog_sky_affect = 0.0` so the fog thickens near
the ground without dimming the sky itself). None of this is required to see the terrain; it's
what makes the demo scene look intentional rather than a bare gray mesh.

The `DirectionalLight3D` has `shadow_enabled = true` with a modest `shadow_normal_bias` and
`shadow_blur`, so the clipmap's height variation actually reads as terrain instead of a flat
lit plane.

The `Camera3D` sits well above and back from the origin, angled down at the terrain. It's the
node `Terrain3D.focus_path` points at (see step 2), so where you place it is also where the
finest clipmap detail and the collision grid center themselves.

## 2. Add the `Terrain3D` node

Add the node, assign `demo_terrain_configuration.tres` (the resource this walkthrough tunes) to
`configuration`, and set `focus_path` to the `Camera3D` sibling. The demo also turns on
`editor_preview_physics`, so the collision gizmo and the terrain dock's live probe have real
data to show while editing, at the cost of the collision grid regridding continuously as you fly
the editor camera around. That's a reasonable trade for a demo scene meant to be looked at in
the editor; turn it off again in a project where editor performance matters more.

## 3. Shape the terrain

Starting from a fresh `TerrainConfiguration`, only a few **Terrain** and **Noise** properties
were moved off their defaults:

- `height_scale = 100.0`, up from the default `10.0`, for dramatic peaks and valleys instead of
  gentle rolling ground.
- `noise_base_frequency = 0.0071`, `noise_lacunarity = 1.744`, `noise_gain = 0.705`, picked by
  eye against the `noise_preview` swatch until the landforms looked rolling-but-craggy rather
  than smooth or noisy. There's no formula here; drag each slider and watch the preview update
  live, then check the result against the actual clipmap.
- `physics_range = 256.0`, up from the default `64.0`, because the camera in this scene ranges
  further from the origin than the default collision area would cover.

## 4. Climate: five biome layers stacking by altitude

`temperature_altitude_reference = 60.0` (with `temperature_frequency = 0.002` and
`moisture_frequency = 0.01`) sets how the noise-shaped terrain reads as climate. Five
`TerrainBiomeLayer`s are assigned to `biome_layers`, each claiming a temperature window:

| Layer | `min_temperature` | `max_temperature` | `max_moisture` | Notes |
|---|---|---|---|---|
| `snow` | `-10` (default) | `0.0` | | Coldest, so the highest ground. |
| `mountain` | `0.0` | `5.0` | | Next band down. |
| `grass` | `5.0` | `20.0` | `1.0` (default) | Mid band, any moisture. |
| `dirt` | `5.0` | `20.0` | `0.5` | Same band as grass, but only its drier half. |
| `river` | `20.0` | `30.0` (default) | | Warmest, so the lowest ground. |

Two things this table demonstrates concretely:

- The windows tile the full `-10` to `30` range with no gap between them, so every point on the
  terrain lands in at least one layer's window.
- `grass` and `dirt` share the same temperature band and are separated purely by moisture:
  `dirt` caps out at `max_moisture = 0.5`, so `grass` (uncapped) wins the wetter half of that
  band and `dirt` wins the drier half. This is the moisture-varies-within-a-band behavior in
  practice, not just in the abstract.

`pom_depth` also varies per layer: `0.1` for `grass`/`dirt`, `0.25` for `snow` (its height
texture reads best with a stronger effect), and left at the `0.0` default for `mountain` and
`river` (no height texture assigned to spend the cost on). The shared cost knobs
`pom_max_steps = 16` are set once on the configuration under **Parallax**, not per layer.

## 5. The rock layer

`rock_layer` is a `TerrainSlopeLayer` painted over all of the above wherever the ground is steep
enough, with `slope_threshold = 0.552` and `slope_blend_range = 0.195`. With `height_scale`
raised to `100.0` in step 3, the terrain's steepest slopes clear that threshold comfortably, so
the rock texture shows up on ridgelines and cliff faces across every climate band, exactly the
"exposed bedrock regardless of biome" effect it's meant for. This is also why the earlier
[Quick start](02-quick-start.md) defaults won't show a rock layer at all: at the default
`height_scale = 10.0`, the terrain never gets steep enough to earn it.

## 6. Prove physics tracks the render

A small rig sits above the terrain: a `Node3D` positioned a few units up, holding a
`RigidBody3D` with a `SphereShape3D` collision shape and a matching `SphereMesh` for visibility.
Press Play and the sphere falls, then settles on the generated collision heightmap exactly where
the rendered surface is. It's the cheapest possible proof that `TerrainPhysics` produces a
collision body matching what `TerrainRenderer` draws, short of building a full playable
character (see `TODO.md` task D5 for that).

## Going further

- The exact default, range and description of every property used above lives in the in-editor
  class reference: press **F1** and search `TerrainConfiguration`, `TerrainBiomeLayer`, or
  `TerrainSlopeLayer`.
- [Runtime API](04-runtime-api.md) covers querying these same values back from code.
- [Editor tools](05-editor-tools.md) covers the dock and gizmos used to watch this scene update
  live while tuning it.
