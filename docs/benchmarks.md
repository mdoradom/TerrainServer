# Benchmarking TerrainServer

`demo/benchmark/` measures what a `Terrain3D` costs: frame time, collision rebuild latency, and
memory. It is pure Godot so it runs identically on Linux, macOS and Windows.
This page explains how to run it and how it works.

## Quick start

```sh
scons benchmark device=device-name
```

That builds the extension and then runs the suite, which is the safe order: the benchmark target
depends on the library, so a run cannot silently measure a stale binary.

To run it without building or from an IDE that drives Godot directly:

```sh
godot --headless --path demo res://benchmark/run_benchmarks.tscn -- --scenarios=@baseline
```

Each measurement point runs as its own Godot process and prints a line as it lands:

```
round 1   baseline                 gpu p10 2.556 ms | cpu p10 0.069 ms | 8 regrids
```

## Output

Everything goes to `.benchmarks/` (gitignored), or to `out=<dir>`:

| File                  | Contents                           |
|-----------------------|------------------------------------|
| `results.csv`         | one row per run                    |
| `results.jsonl`       | the same runs as full JSON records |
| `<scenario>.r<N>.log` | the complete Godot log of that run |

The CSV carries the identity of the machine (`device`, `os`, `cpu_name`, `gpu_name`, `godot`),
the full configuration that was measured (`mesh_resolution`, `clipmap_levels`, `noise_octaves`,
`physics_range`, the POM settings, …) and every metric, so a chart can be built from one file
with no cross-referencing:

| Column group      | Columns                                                                        |
|-------------------|--------------------------------------------------------------------------------|
| Frame time        | `gpu_p10`, `gpu_p50`, `gpu_mean`, `gpu_p90`, `gpu_min`, `gpu_max`, and `cpu_*` |
| Collision rebuild | `regrid_count`, `regrid_mean`, `regrid_p90`                                    |
| Scene load        | `draw_calls`, `primitives`                                                     |
| Memory            | `video_mem_mib`, `static_mem_mib`                                              |
| Run identity      | `device`, `round`, `scenario`, `frames`, `watchdog`                            |

`regrid_mean` and `regrid_p90` are left **blank**, not zero, when no rebuild completed inside the
sampling window, so a plot does not read "never observed" as "took no time".

To chart one axis, sweep it and plot `scenario` against `gpu_p10`; to compare machines, run the
same scenario everywhere and plot `device`. Since each row carries its own configuration, several
devices CSVs can simply be concatenated (drop the repeated header row).

## Scenarios

A scenario is a named override applied to a `duplicate()` of the shipped configuration, so a
sweep never dirties `demo_terrain_configuration.tres` on disk.

| Scenario             | Effect                                                                |
|----------------------|-----------------------------------------------------------------------|
| `baseline`           | the shipped configuration, unmodified                                 |
| `pom_off`            | collapses the POM distance fade so the shader skips parallax entirely |
| `stress`             | every sweepable axis at the top of its declared range at once         |
| `pom_<min>_<max>`    | e.g. `pom_32_64` — sets both POM step counts                          |
| `<property>_<value>` | e.g. `mesh_resolution_512`, `noise_octaves_10`                        |

`<property>` is one of `terrain_size`, `height_scale`, `mesh_resolution`, `clipmap_levels`,
`noise_octaves`, `physics_range`, `pom_min_steps`, `pom_max_steps`. Bounds come from the
`PROPERTY_HINT_RANGE` declarations in `src/core/terrain_configuration.cpp`, and a value outside the
declared range is **refused**, not silently clamped, and an unknown scenario name exits non-zero
rather than quietly measuring the baseline.

### Groups

A whole axis is one `@group`, so a sweep does not need a hand-written scenario list:

| Group       | Scenarios                               |
|-------------|-----------------------------------------|
| `@baseline` | `baseline`, `pom_off`                   |
| `@clipmap`  | `clipmap_levels` 1 → 10                 |
| `@mesh`     | `mesh_resolution` 32, 64, 128, 256, 512 |
| `@octaves`  | `noise_octaves` 1 → 10                  |
| `@physics`  | `physics_range` 8, 64, 128, 256, 512    |
| `@pom`      | POM off through 64/128                  |
| `@sweep`    | every group above                       |
| `@all`      | `baseline` + `@sweep` + `stress`        |

```sh
scons benchmark scenarios=@sweep rounds=2
scons benchmark scenarios=@mesh,@octaves
scons benchmark scenarios=baseline,mesh_resolution_512,stress
```

Groups and names can be mixed freely; duplicates are measured once.

## How it works

* **A dedicated scene.** `demo/benchmark/benchmark.tscn` holds a `Terrain3D`, a `WorldEnvironment`,
a `DirectionalLight3D` with shadows on, and a `Camera3D` wired as the terrain's
`focus_path`. It deliberately is *not* `demo/terrain_server.tscn`: if the benchmark depended on
the demo, later edits to the demo's props or camera would silently move every number.

* **A deterministic camera.** It advances a fixed 2.0 units per frame **indexed by frame number,
never by `delta`**. A slower machine therefore walks the identical path and triggers the identical
number of collision rebuilds, which is the only reason per-device rows are comparable. It flies
20m above the surface, low enough that the POM distance fade is actually engaged. Higher up,
parallax fades out and the POM scenarios measure nothing.

* **Sampling.** 90 warmup frames, which also absorb shader compilation, then 240 samples of
`RenderingServer.viewport_get_measured_render_time_gpu()` and `_cpu()`. A watchdog quits at a hard
cap of 600 frames whatever the state, so a run that stops progressing fails visibly instead of
hanging; the `watchdog` column flags any run that ended that way.

* **Collision rebuild latency.** Timed as `Terrain3D.is_collision_rebuild_pending()`, 
polled once per frame. Granularity is therefore one frame, and a rebuild that completes inside 
a single frame is not observed at all.

* **Resolution.** 1920x1080 with vsync off. The terrain shader is vertex-bound, so a higher
resolution measures the same thing more slowly.

## Options

`scons benchmark` accepts these as `name=value`; the Godot invocation accepts the same names as
`--name=value` after the `--`.

| Option      | Default                        | Meaning                                      |
|-------------|--------------------------------|----------------------------------------------|
| `scenarios` | `@baseline`                    | comma-separated scenario names and `@groups` |
| `rounds`    | `2`                            | interleaved rounds                           |
| `out`       | `.benchmarks`                  | output directory                             |
| `device`    | `$BENCH_DEVICE`, else hostname | label recorded in the results                |
| `godot`     | `godot`                        | Godot binary, `scons benchmark` only         |

## Running on another device

```sh
scons benchmark device=device-name scenarios=@baseline
```

`device` is only a label; the CPU, GPU, OS and Godot version are recorded automatically from the
running engine, so a device table can be derived from the CSV rather than maintained by hand.

The runner picks the rendering backend per platform (**Metal on macOS, Vulkan elsewhere**) and
on Linux without a display it wraps each run in `xvfb-run -a` (or always, with `BENCH_XVFB=1`).
Nothing needs adjusting by hand, and no shell is involved: the same command works in cmd.exe,
PowerShell and a Unix shell.

Use the same Godot version everywhere; the engine version is recorded per run, so a mismatch is
at least visible afterwards.