# Showreel trailer footage

Task list for generating the raw clips behind the 1:00–1:30 showreel trailer, driven as much as
possible by the actual plugin running inside Godot rather than built in an external video/motion
tool. The user assembles the finished trailer themselves — cuts, music mix, titles, logo, grade
— so the scope here stops at **exporting a handful of on-brand, silent clips**, one per
storyboard beat, whose internal timing already lands on the chosen track's marked hits.

Storyboard beats covered: (1) a wireframe "genesis" opening — a flat line revealing itself as a
deformed plane, fast ground-level travelling; (2) a wireframe mountain flyover; (3) an "erosion"
shot — particles raining onto the terrain with a visible before/after; (4) fast cuts through
splatmap/normal-map debug views of biomes; (5) a photorealistic, SDFGI-lit cinematic flythrough.

## Decisions already made

| Decision | Why |
| --- | --- |
| Godot renders clips only; the user assembles the trailer | No in-Godot title cards, audio mix, beat-sync tooling, or ffmpeg/Remotion assembly step belongs in this repo |
| Export via `godot --path demo res://trailer/trailer.tscn --write-movie <file> -- --shot=<name>`, 1920x1080 @ 60fps | Deterministic, frame-accurate, one process per clip |
| New work lives in `demo/trailer/`, mirroring `demo/benchmark/` | Dedicated scene + script; `demo/terrain_server.tscn` stays untouched |
| Camera moves on a fixed step indexed by frame number, never `delta` | Matches `demo/benchmark/benchmark.gd`'s existing convention; reproducible across machines |
| Wireframe look = `Viewport.debug_draw = DEBUG_DRAW_WIREFRAME` over the real `Terrain3D` mesh, not a custom wireframe shader | A custom shader would need the vertex displacement extracted into a shared `.gdshaderinc` — a real refactor of the shipped, benchmarked production shader for a one-off cosmetic need. `debug_draw` gets the same real noise-displaced geometry for free |
| "Line becomes a plane" opening is a camera-framing trick, not new geometry | Frame the real wireframe mesh near edge-on and far away at t=0, then swoop down/around to reveal the full plane. No synthetic mesh system |
| Erosion shot = `GPUParticles3D` + one real `TerrainConfiguration` swap, not per-frame mesh animation | The renderer isn't built for per-frame config edits — `changed` triggers a full `rebuild_mesh()` (see `CLAUDE.md`) |
| Clip timing is driven by real audio cues, not guessed timestamps | Track is "Bliss" by Klsr, trimmed to ~1:07 with a bridge at that point separating the track's two halves — lines up with the storyboard's own pivot into the cinematic reveal. Cuts landing on the track's marked hits means the eventual edit needs minimal manual nudging |
| The one real plugin change is a `debug_view` shader uniform + bound setter | Only the splatmap/normal-map "diagram" cuts need fragment-internal values (biome blend weights, `normal_ws`) that GDScript can't otherwise reach. Everything else needs no C++/shader work |

## How to pick up a task

- **One task = one commit.** Each item below is self-contained. Do not bundle several items
  into a single commit, and do not commit while still drafting.
- Do the task's **Verify** step before committing. If it cannot be verified, say so in the
  handback rather than marking the box.
- **T0 gates T3–T6's real timing** (they can be built against placeholder cue timestamps in the
  meantime, but must be re-verified once T0's real `audio_cues.json` lands). **T1 gates T5.**
  T2 depends on T0 for real data but can stub it. T7 comes last — it documents the recording
  commands for clips that already exist.

## Build and verify commands

```sh
scons platform=linux target=template_debug   # standard build (T1 only)
clang-format -i <files>                      # before committing C++ changes (T1 only)
```

Open `demo/project.godot` in Godot and run `demo/trailer/trailer.tscn` (once it exists) with
`--shot=<name>` to preview a shot before ever recording it.

> [!WARNING]
> **Stale binary trap.** `*.so` is gitignored, so `demo/addons/terrain_server/bin/` survives
> `git checkout` and is never what git says it is. Before any A/B comparison on T1, run
> `rm demo/addons/terrain_server/bin/*.so` first so a wrong path fails loudly instead of
> silently loading a stale build. `.gdshader` edits are unaffected — they load from `res://` at
> runtime, no rebuild needed to see a shader change.

---

## Tasks

- [x] **T0 — Audio cue extraction script.**
      `demo/trailer/tools/extract_cues.py`: tries `librosa` onset detection first, falls back to
      a self-written `ffmpeg` + `numpy` spectral-flux detector (decode to mono PCM, STFT via
      `numpy.fft`, half-wave-rectified frame-to-frame spectral difference) if `librosa` isn't
      importable. Both paths hand a continuous strength envelope to one shared peak-picker
      (`--min-gap`/`--threshold-k`), then a second greedy pass marks the loudest, well-spaced
      subset as `"strong"` (`--strong-min-gap`/`--strong-fraction`) — librosa's own
      `onset_detect` defaults were tried first and produced ~1000 cues on this track, far too
      dense to cut to; the shared picker tuned to `threshold_k=2.0, min_gap=0.25s` lands on a
      usable density instead. `--bridge-hint <sec>` snaps the bridge cue to the nearest detected
      peak to a human-picked approximate second, rather than trying to blindly auto-detect a
      song structure change — that auto-heuristic (quietest stretch + loudest peak after it)
      drifted between 85s/111s across tuning attempts and isn't trustworthy on its own.
      Writes `demo/trailer/audio_cues.json`: `{source, method, duration_sec, cues: [{time_sec,
      kind, strength}]}`, `kind` one of `onset`/`strong`/`bridge`.
      **Reproducing needs `librosa`**, not installable system-wide on this Arch box
      (externally-managed-environment) — use a venv: `python -m venv .venv && .venv/bin/pip
      install librosa matplotlib && .venv/bin/python demo/trailer/tools/extract_cues.py
      "demo/trailer/audio/klsr - Bliss.flac" --bridge-hint 67 --plot cues_plot.png`. The `.venv`
      itself isn't checked in.
      > Result: source `klsr - Bliss.flac` (145.28s). 178 cues, 43 `strong`. Bridge snapped to
      > **63.94s** from a `--bridge-hint 67` (user confirmed "aprox 1m 7s" by ear, to be
      > fine-tuned with a fade in the final edit — the automatic guess doesn't need to be exact).
      > `demo/trailer/audio_cues.json` is generated and committed.
      *Verify:* cue count and spacing look right against a manual listen/waveform check; the
      bridge cue lands within a fraction of a second of the audible transition. **Done above**,
      bridge precision explicitly deferred to the user's own edit.

- [x] **T1 — `debug_view` uniform + bound API.**
      `demo/addons/terrain_server/shaders/terrain.gdshader`: add `uniform int debug_view = 0;`
      near the existing uniform block, and additive branches at the very end of `fragment()`
      (~line 600–613, before the final `ALBEDO`/`NORMAL` writes) for: 1 = raw noise grayscale,
      2 = biome id as flat color (splatmap look), 3 = `normal_ws` visualized as RGB.
      `debug_view == 0` must reproduce the current output exactly — no regression to the
      benchmarked baseline in `docs/benchmarks.md`.
      `src/core/terrain_renderer.h/.cpp`: add `TerrainRenderer::set_debug_view(int)`, a
      lightweight loop over `_clipmap_levels` calling `RenderingServer::material_set_param(
      material_rid, "debug_view", value)` — same shape as the existing per-frame loop in
      `update_focus_position()` (~line 781–807). No rebuild.
      `src/nodes/terrain3d.h/.cpp`: bind `set_debug_view`/`get_debug_view`, mirroring how
      `reload_shader()` is bound and forwarded (~line 44, ~line 398).
      *Verify:* rebuild (`scons platform=linux target=template_debug`), open
      `demo/terrain_server.tscn`, confirm `debug_view == 0` looks identical to before the
      change, then exercise 1/2/3 from a throwaway script call.
      > Result: debug modes write to `EMISSION` (with `ALBEDO` zeroed) rather than `ALBEDO`
      > directly, so the debug color reads unshaded/unlit instead of being modulated by
      > scene lighting. Verified via a throwaway `SceneTree` script driving
      > `demo/terrain_server.tscn`, stepping `debug_view` 0→1→2→3 and capturing a viewport
      > screenshot at each: 0 is pixel-identical in kind to the pre-change shading (normal
      > lit terrain), 1 shows a height-based grayscale gradient, 2 shows flat per-biome
      > colors (clean splatmap look), 3 shows `normal_ws` as RGB. No shader compile errors
      > in any mode.

- [ ] **T2 — `demo/trailer/` scene scaffold + shared camera rig.**
      `demo/trailer/trailer.tscn` (a `Terrain3D`, `WorldEnvironment`, `DirectionalLight3D`,
      `Camera3D`) and `demo/trailer/trailer_rig.gd`: frame-indexed camera movement (reusing the
      `CAMERA_STEP`-per-frame pattern from `demo/benchmark/benchmark.gd`),
      `Terrain3D.get_height_at()` for ground-hugging shots, `audio_cues.json` loaded to size
      shot/cut durations in frames at 60fps, and a `--shot=` cmdline arg (via
      `OS.get_cmdline_user_args()`, matching `run_benchmarks.gd`) selecting which of T3–T6
      plays. Depends on T0 for real cue data, but can be stubbed with placeholder timestamps to
      unblock development in parallel.
      *Verify:* scene opens and runs in-editor for each `--shot=` value with no console errors,
      and shot durations printed at runtime match the expected cue gaps.

- [ ] **T3 — Shot: wireframe genesis + flyover.**
      Pure GDScript/scene work on top of T2, no engine changes: `Viewport.debug_draw =
      DEBUG_DRAW_WIREFRAME`, `Environment.background_mode` = solid black, camera starts near
      edge-on/far, swoops down to a fast ground-level travelling over the wireframe mountains.
      Hard cuts inside the shot (if any) land on T0's cue timestamps.
      *Verify:* recorded clip (see T7) opens a flat line at frame 0 and reads as a full deformed
      wireframe plane within the shot, with internal cuts landing on the marked hits.

- [ ] **T4 — Shot: erosion particles.**
      `GPUParticles3D` cyan rain grounded via `get_height_at()`, one scripted
      `TerrainConfiguration` swap mid-shot for a real before/after rebuild, timed to a cue from
      T0.
      *Verify:* recorded clip shows a visible surface change coincident with both the particle
      pass and the timed audio cue.

- [ ] **T5 — Shot: biome/normal debug-view cuts.**
      Depends on T0 and T1. Fast cuts toggling `Terrain3D.set_debug_view(1|2|3)` over one or two
      configs, one cut per cue in this shot's cue range rather than a fixed ~1s guess.
      *Verify:* each debug mode renders distinctly, without shader errors, and each cut lands on
      its cue frame in the recorded clip.

- [ ] **T6 — Shot: cinematic final.**
      Depends on T0 for its start time (the ~1:07 bridge cue). Reuses `demo/river_terrain.tres`
      as-is; enable SDFGI on the scene's `WorldEnvironment`; smooth camera move,
      `debug_view = 0`.
      *Verify:* recorded clip is fully shaded/textured with SDFGI visibly contributing (bounce
      light change when panning past geometry), starting exactly at the bridge cue.

- [ ] **T7 — Recording convention.**
      Document the exact `--write-movie` invocation per shot (1920x1080 @ 60fps) in a short
      section at the bottom of this file, once T3–T6 exist to be recorded.
      *Verify:* a reader can reproduce any clip from the command alone.

---

## Out of scope

Titles, logo card, music mixing, color grading and final cut assembly — the user does all of
this themselves outside the repo. No ffmpeg/Remotion assembly step, no `AudioStreamPlayer` or
audio mixing inside Godot, no ambition to make `--write-movie`'s output itself the finished
trailer.
