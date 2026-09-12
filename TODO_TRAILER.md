# Showreel trailer footage

Task list for generating the raw clips behind the 1:00–1:30 showreel trailer, driven as much as
possible by the actual plugin running inside Godot rather than built in an external video/motion
tool. The user assembles the finished trailer themselves — cuts, music mix, titles, logo, grade
— so the scope here stops at **exporting a handful of on-brand, silent clips**, one per
storyboard beat, whose internal timing already lands on the chosen track's marked hits.

The trailer is in two halves, split by the track's bridge cue. Everything before it is one
continuous **build-up diagram**: a static ~45-degree aerial on a wireframe patch of terrain floating
in black, assembling itself to the beat — the clipmap grid draws itself, noise appears on it and
gains an octave per hit, the sheet stands up into relief, each shaping parameter arrives on its own
hit, and the shot then wipes through what the shader derives from that surface (normals, lighting,
temperature, moisture) before the biomes light up one by one over a Whittaker chart. Everything
after the bridge is the photorealistic, SDFGI-lit payoff on the real terrain.

Each beat is its own static camera, cut on a marked hit — no travelling, no flythrough until the
payoff. The build-up is deliberately *not* a faithful reproduction of the plugin's workflow; it is
the generation pipeline staged so it reads at a glance and lands on the music.

## Decisions already made

| Decision | Why |
| --- | --- |
| Godot renders clips only; the user assembles the trailer | No in-Godot title cards, audio mix, beat-sync tooling, or ffmpeg/Remotion assembly step belongs in this repo |
| Export via `godot --path demo res://trailer/trailer.tscn --write-movie <file> -- --shot=<name>`, 1920x1080 @ 60fps | Deterministic, frame-accurate, one process per clip |
| New work lives in `demo/trailer/`, mirroring `demo/benchmark/` | Dedicated scene + script; `demo/terrain_server.tscn` stays untouched |
| Camera moves on a fixed step indexed by frame number, never `delta` | Matches `demo/benchmark/benchmark.gd`'s existing convention; reproducible across machines |
| The build-up runs on a purpose-built "diorama", not on the `Terrain3D` node | `Terrain3D` rebuilds its whole clipmap mesh on any `TerrainConfiguration` edit (see `CLAUDE.md`), which rules out animating a noise parameter per frame, and it owns its `RenderingServer` material, which rules out adding diagram-only uniforms. The diorama is clipmap-shaped geometry (centre block + rings) built in GDScript with its own shader |
| The diorama's shader **splices** the production shader rather than copying or `#include`-ing it | `diorama.gd` concatenates everything `terrain.gdshader` declares before its `void vertex()` (uniforms, varyings, noise/climate functions) with `diorama_body.gdshaderinc`. So the heights on screen are the plugin's own heights and the animated parameters are the plugin's own parameters, with no copy that can drift and no refactor of the shipped, benchmarked shader. `#include` was not an option: `TerrainRenderer` feeds the shader to `RenderingServer::shader_set_code`, which does not run the preprocessor |
| Wireframe is drawn in-shader from barycentric coordinates, not `Viewport.debug_draw` | `DEBUG_DRAW_WIREFRAME` cannot be coloured, faded, dimmed per clipmap level, or pulsed on the beat — all of which the build-up depends on. Unindexed geometry carries barycentrics in `COLOR`, and the third axis is always the vertex opposite the quad diagonal, so the shared diagonal can be drawn fainter than the quad edges |
| Every on-screen value is a pure function of shot time; no state accumulates between frames | Any moment can be previewed alone with `--start=<sec> --frames=<n>` without rendering what precedes it, and `--write-movie` is reproducible across machines. This is what made tuning the look practical at all |
| The diorama sits at a hand-picked world position, not the origin | Temperature and moisture are noise fields with wavelengths about one diorama wide, so most of the world falls inside a single climate band and renders as one biome. `demo/trailer/tools/pick_diorama_center.gd` scores candidate patches on real samples for biome balance and relief |
| Every authored number lives in `trailer_timeline.json`, edited live by an in-scene panel | Tuning a trailer is a look-at-it-and-adjust loop, and editing constants in GDScript between renders is the slowest possible version of it. The rig keeps the choreography (which parameter a chapter ramps, in what order); the file keeps the values. Moving them out was verified to change nothing on screen |
| `demo/project.godot` sets a 1920x1080 viewport | `--write-movie` fixes its recording size before the scene loads, from `display/window/size/viewport_*` alone — neither `--resolution` nor a runtime resize moves it (both change the viewport while the movie keeps recording at the project size). `demo/benchmark` passes `--resolution 1920x1080` to every run it spawns, so its measurements already ran at this size and do not move |
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

```sh
# tune it by eye: scrub, jump cue to cue, drag sliders, Save writes trailer_timeline.json
godot --path demo res://trailer/trailer.tscn -- --shot=build --controls
# preview one beat without rendering what precedes it
godot --path demo res://trailer/trailer.tscn -- --shot=build --start=50 --frames=840
```

`--controls` is refused during a `--write-movie` recording, since the panel draws into the very
viewport being captured.

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

- [x] **T2 — `demo/trailer/` scene scaffold + shared camera rig.**
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
      > Result: `SHOT_RANGES` assigns each of `genesis`/`erosion`/`debug`/`cinematic` a
      > `[start_sec, end_sec]` window snapped to real `audio_cues.json` "strong" cue
      > timestamps — the pre-bridge span (beats 1-4) split roughly into thirds, cinematic
      > running bridge (63.936s) to the track's end as a placeholder until T6 picks its real
      > trimmed endpoint. `_position_camera()` is a placeholder ground-hugging travelling shot
      > for T3-T6 to replace. Verified headlessly (`xvfb-run godot --path demo
      > res://trailer/trailer.tscn -- --shot=<name>`) for all four shots plus an unknown-shot
      > case: each prints its window/frame-range/cue-count with no console errors, and a full
      > run (genesis, 1237 frames) completes and quits cleanly.
      > Amended by T3: those four windows are now two — `build` (0s–63.936s) and `cinematic`
      > (63.936s–end) — and `--start=<sec>`/`--frames=<n>` were added so a single beat can be
      > previewed or recorded without rendering everything before it.

- [x] **T3 — Shot: the build-up diagram (`--shot=build`, 0s–63.936s).**
      Supersedes the original "genesis + flyover" travelling shot, which was built and then
      replaced: the trailer's build-up is now a static ~45-degree aerial on a wireframe diorama in
      black, assembling itself to the beat. Six chapters, each its own static camera cut on a
      "strong" cue — `grid` (0.035s, the clipmap draws itself, ring by ring), `noise` (9.451s, the
      field appears flat on the grid, one octave per hit to 17.310s), `relief` (20.619s, the sheet
      springs up, then ridge/warp/continent/redistribution each arrive on their own hit), `normals`
      (30.906s, wipe to world normals, then to lit clay at 36.943s), `climate` (42.388s,
      temperature then moisture, dimming out into the track's 44–50s silence), `biomes` (50.085s,
      the four biomes light up one per hit over a Whittaker chart, slope/cliff overlay at 60.825s,
      flash into the bridge).
      Files: `demo/trailer/diorama.gd` + `diorama_body.gdshaderinc` (geometry, spliced shader,
      the diagram views), `trailer_rig.gd` (timeline, cue envelopes, cameras),
      `whittaker_overlay.gd` (the 2D chart), `tools/pick_diorama_center.gd` (patch selection).
      *Verify:* every chapter renders as intended, on its cue, with no shader or script errors.
      > Result: the wireframe is drawn in-shader from barycentrics rather than with
      > `debug_draw`, and all displacement comes from the production shader's own noise, spliced
      > in at load (see the decisions table). Two findings worth keeping: `--write-movie` records
      > at `demo/project.godot`'s viewport size and ignores `--resolution`, so the project now
      > declares 1920x1080; and the terrain's climate fields are narrow enough that the origin
      > renders as a single-biome patch, which is why the diorama sits at (5632, -4608), chosen by
      > the tool on biome balance and relief.
      > Verified by rendering stills at 1920x1080 across the whole window (16 beats, then 6 more
      > after tuning) and inspecting them: the grid draws out and the clipmap rings pop in, the
      > noise field is legible and gains detail per hit, the sheet springs into relief and each
      > parameter visibly reshapes it, the normal/lit/temperature/moisture wipes read distinctly,
      > and at 61.5s all four biomes appear on the terrain with a matching scatter in the chart.
      > No shader or script errors in any run. The look needed two tuning passes — the first was
      > blown out to white by glow plus an over-bright fill, and a 128-quad grid read as fabric
      > rather than wireframe.
      > End-to-end: `--shot=build` runs its full 3836 frames (63.936s at 60fps) and
      > `--shot=cinematic` its preview window, both quitting cleanly with exit code 0.

- [~] **T4 — Shot: erosion particles. Dropped from the storyboard.**
      The build-up has no travelling shot left to rain onto, and its "parameter arrives on a hit,
      surface visibly reshapes" beats already carry the before/after idea that the erosion shot
      existed to show — with the plugin's real parameters rather than a scripted config swap.
      Revive only if the cut wants a distinct particle beat; nothing in T3 depends on it.

- [x] **T5 — Shot: biome/normal debug-view cuts. Absorbed into T3.**
      These are now the `normals`, `climate` and `biomes` chapters of the build-up, wiped in on
      their cues instead of hard-cut, over the diorama rather than the `Terrain3D` node.
      > Note: that makes **T1's `debug_view` uniform unused by the trailer**. The diorama's shader
      > computes its own diagram views, because it needs values T1 does not expose (per-biome
      > weights for the one-at-a-time reveal, temperature and moisture as separate fields) and
      > needs them on geometry `debug_view` cannot reach. `debug_view` is still a working,
      > documented plugin feature and T6 may yet use it; it is simply no longer load-bearing here.

- [ ] **T6 — Shot: cinematic final.**
      Depends on T0 for its start time (the ~1:07 bridge cue). Enable SDFGI on the scene's
      `WorldEnvironment`; smooth camera move, `debug_view = 0`.
      Currently a placeholder: `_apply_cinematic()` holds the build-up's last framing — same
      centre, same ~45-degree angle — on the real `Terrain3D` with the sky environment and SDFGI
      on, easing in slowly, so the bridge reads as the same world switching from diagram to
      render. It still needs a real camera move, a chosen endpoint, and a look pass; it has only
      been checked for running cleanly, never for how it looks.
      *Verify:* recorded clip is fully shaded/textured with SDFGI visibly contributing (bounce
      light change when panning past geometry), starting exactly at the bridge cue.

- [x] **T8 — Live control layer (`trailer_timeline.json` + `--controls`).**
      `trailer_rig.gd` holds no authored numbers any more: every time, camera, colour and effect
      value lives in `demo/trailer/trailer_timeline.json`, and `demo/trailer/trailer_controls.gd`
      is an in-scene panel that scrubs the shot, steps frames, jumps cue to cue and chapter to
      chapter, edits those values on live sliders and writes the file back with Save — so what you
      dial in by eye is exactly what the next recording renders. Sliders are declared as paths into
      the timeline dictionary, and the per-chapter rows follow the playhead, showing only the keys
      the chapter on screen actually defines.
      *Verify:* externalising the values must not change the render, and the panel must never
      reach a recorded frame.
      > Result: pixel-identical — six beats (7.2s, 12.5s, 25.6s, 33.0s, 43.0s, 61.5s) rendered
      > before and after the refactor have matching MD5s, so moving every value into a data file
      > changed nothing on screen. The panel itself was smoke-tested by grabbing the X display of a
      > `--controls` run: transport, per-chapter camera/content rows and the global LOOK/EFFECTS
      > sections all draw, with no script errors.
      > Two bugs found on the way, both worth remembering: `get_window()` collided with
      > `Node.get_window()` and broke the whole script's parse; and the recording guard tested
      > `OS.get_cmdline_args()` for `--write-movie`, which can never match, because Godot strips
      > its own flags from that array (it reports only `["--script", ...]`) — so the first version
      > of the guard would have quietly let the panel into a recording. It now uses
      > `Engine.get_write_movie_path()`, verified: `--write-movie` together with `--controls`
      > prints "--controls ignored", records at 1920x1080 and exits 0.

- [x] **T9 — Editor: timeline ruler, transport and reversible editing.**
      T8 made every value editable; this makes them tunable. `demo/trailer/trailer_timeline_ruler.gd`
      is a full-width bottom strip drawing four lanes against the shot window — the chapter bands,
      the track's waveform, the detected cues (onset / strong / bridge, each distinct) and a second
      ruler — with a draggable playhead. Tuning this trailer means landing authored values on musical
      hits, and that is far easier when the hits are a visible shape rather than a list of
      timestamps. `demo/trailer/tools/extract_peaks.py` generates that waveform
      (`audio_peaks.json`, one peak + RMS bin per frame at 60fps) with ffmpeg + numpy, mirroring
      `extract_cues.py`'s fallback decoder; a missing file just leaves the lane empty.
      `trailer_controls.gd` gains: a spin box beside every slider (exact entry, not just a drag),
      per-row revert plus "set to playhead" / "snap to nearest cue" buttons on every row whose value
      is a moment, a dirty marker per row and an unsaved count, coalesced undo/redo, a parameter
      filter, collapsible sections, chapter/cue/frame stepping, playback speed, and shot / chapter /
      free-region looping (drag the ruler with the right button). `trailer_rig.gd` grows the clock
      side of that — speed, loop region, cue kinds, the waveform, and `reload_timeline()` for
      "revert all" — leaving `_apply_build()` and the frame-indexed recording path untouched.
      *Verify:* the panel must never reach a recorded frame, and must not change what one looks like.
      > Result: the recording path is byte-identical — stills at 7.2s, 25.6s, 47.4s and 61.5s
      > rendered with `HEAD`'s `trailer_rig.gd` swapped back in have the same MD5s as the ones this
      > branch renders. The rig is the only changed file a recording can reach: `trailer_controls.gd`
      > is `queue_free()`d in `_setup_controls()` before it draws anything when `--controls` is
      > absent, and the ruler, the waveform and the tools are only ever referenced from it. That is
      > what the diff predicts — every change is either editor-only or additive on the `--controls`
      > branch of `_process()`.
      > Worth knowing before the next A/B: a fresh `git worktree` is the wrong harness for this.
      > `*.so` is gitignored, so the extension is absent, and a cold project imports before it can
      > register one — `Terrain3D` falls back to a placeholder node, the rig never runs, `--frames`
      > is therefore never honoured, and `--write-movie` records thousands of empty frames rather
      > than failing. Swapping the single file inside the already-imported project answers the same
      > question in seconds, and `--quit-after` bounds the damage when a scene does fail to load.
      > One bug worth remembering, caught only by reading a screenshot: the time rows were declared
      > with `step = 0.01`, but cue timestamps carry three decimals. Both `HSlider` and `SpinBox`
      > quantise to their step, so the panel displayed the moisture cut at 43.54 instead of 43.537
      > and would have written that rounded value back the moment the row was touched — silently
      > sliding an authored moment off the very beat it exists to land on. Every TIME row now steps
      > in milliseconds.

- [x] **T10 — Effects pass: camera drift, cut blending, strength-weighted pulse, vignette.**
      Four things the build-up had no way to express, all authored in `trailer_timeline.json` and all
      on live sliders. A per-chapter camera drift (`yaw_rate`, `pitch_rate`, `distance_rate`,
      `height_rate`, per second) so a locked-off beat breathes instead of reading as a freeze-frame.
      `blend_in`, which eases a chapter out of its predecessor's framing instead of cutting to its
      own — 0 everywhere, since the storyboard is built on hard cuts, but there for the beats where
      one turns out too abrupt with music under it. `pulse_strength_weight`, which scales each hit's
      reaction by the onset strength the detector actually measured rather than the flat
      strong/onset amplitude, so a run of quiet onsets breathes instead of strobing alike; the
      detector's scale bottoms out well above zero (this track runs ~2.4 to 5.3), so the strengths
      are stretched across the observed range before use. And `vignette`, a screen-space falloff in
      the diorama shader that stops the outer clipmap rings competing with level 0.
      *Verify:* every new key must be neutral by default and leave the render byte-identical, and
      only then be dialled in.
      > Result: done in those two steps. With every key at its neutral value the four stills came
      > back with MD5s identical to T9's, so the knobs alone changed nothing on screen; the values
      > were then authored (drift on all six chapters, `vignette` 0.32, `pulse_strength_weight` 0.5)
      > and checked by eye against the same frames.
      > `_apply_cinematic()` erases the drift keys from the chapter it clones. The build-up's rates
      > are sized for beats lasting seconds, and the cinematic reuses the biomes chapter for over a
      > minute — inherited, its -8/s dolly would have pulled the camera 650 units in by the end.

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
