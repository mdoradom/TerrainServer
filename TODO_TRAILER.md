# Showreel trailer footage

Task list for generating the raw clips behind the 1:00–1:30 showreel trailer, driven as much as
possible by the actual plugin running inside Godot rather than built in an external video/motion
tool. The user assembles the finished trailer themselves — cuts, music mix, titles, logo, grade
— so the scope here stops at **exporting a handful of on-brand, silent clips**, one per
storyboard beat, whose internal timing already lands on the chosen track's marked hits.

The trailer is in two halves, split by the track's bridge cue. Everything before it is one
continuous **build-up diagram**: an aerial on the real clipmap terrain floating in black, assembling
itself to the beat — the clipmap draws itself outward and gains a ring per hit, the noise field
appears on it and gains an octave per hit, the surface stands up into relief as `height_scale` ramps
and each shaping parameter arrives on its own hit, and the shot then wipes through what the shader
derives from that surface (normals, lighting, temperature, moisture) before the biomes arrive one
per hit, each sweeping in on the same wipe front the views change on, and hand over to the plugin's
own textured render. Everything after the bridge is that same render in the demo scene's own sky and
light.

Each beat is its own camera, cut on a marked hit. The build-up is deliberately *not* a faithful
reproduction of the plugin's workflow; it is the generation pipeline staged so it reads at a glance
and lands on the music.

**The trailer is a front end over the plugin, not a parallel implementation of it.** It owns the
choreography — cameras, cue envelopes, the timeline file and its editor — and nothing else. The
picture is `demo/terrain_server.tscn`, instanced whole, so every mesh, biome blend, texture, shadow
and grade on screen is what the plugin produces in that scene. The trailer reaches it through
exactly three channels: animated `TerrainConfiguration` values pushed with
`Terrain3D.refresh_parameters()`, `Terrain3D.rebuild()` on beats for the two values that change the
mesh, and `Terrain3D.set_shader_parameter()` for the diagram overlay uniforms the shader declares
for tooling. See T12.

## Decisions already made

| Decision | Why |
| --- | --- |
| Godot renders clips only; the user assembles the trailer | No in-Godot title cards, audio mix, beat-sync tooling, or ffmpeg/Remotion assembly step belongs in this repo |
| Export via `godot --path demo res://trailer/trailer.tscn --write-movie <file> -- --shot=<name>`, 1920x1080 @ 60fps | Deterministic, frame-accurate, one process per clip |
| New work lives in `demo/trailer/`, mirroring `demo/benchmark/` | Dedicated scene + script. `demo/terrain_server.tscn` is never edited — `trailer.tscn` instances it as the scenario and hides the nodes that do not belong in a frame at runtime |
| Camera moves on a fixed step indexed by frame number, never `delta` | Matches `demo/benchmark/benchmark.gd`'s existing convention; reproducible across machines |
| The build-up runs on the real `Terrain3D` node (**T12**, supersedes the diorama) | The diorama it replaced was a parallel implementation — its own clipmap geometry in GDScript, its own `unshaded` shader spliced out of the production one, its own CPU-built biome texture arrays — and it looked like it: fake lambert instead of shadows, textures configured differently from the demo scene. The two reasons it existed are both gone: `Terrain3D.refresh_parameters()` pushes an animated parameter with no rebuild, and the diagram uniforms live in the production shader's own tooling block |
| Wireframe is drawn in-shader from the mesh's own UV grid, not `Viewport.debug_draw` and not barycentrics | `DEBUG_DRAW_WIREFRAME` cannot be coloured, faded or pulsed on the beat. Barycentrics needed unindexed geometry of the trailer's own; `TerrainGenerator` lays out every mesh (block, ring fixup, trim) so that `UV * resolution` is the cell coordinate, which puts the lines on the real mesh's real quad edges with no vertex attribute and no geometry change — and makes each clipmap level show its own cell size for free. Lines are measured in pixels and fade out as their spacing closes on `debug_wire_min_spacing`, without which a high `mesh_resolution` seen from far off fills in to a solid wash |
| Every on-screen value is a pure function of shot time; no state accumulates between frames | Any moment can be previewed alone with `--start=<sec> --frames=<n>` without rendering what precedes it, and `--write-movie` is reproducible across machines. This is what made tuning the look practical at all |
| The clipmap is focused on a hand-picked world position, not the origin | Temperature and moisture are noise fields with wavelengths about one patch wide, so most of the world falls inside a single climate band and renders as one biome. `demo/trailer/tools/pick_patch_center.gd` scores candidate patches on real samples for biome balance and relief |
| Every authored number lives in `trailer_timeline.json`, edited live by an in-scene panel | Tuning a trailer is a look-at-it-and-adjust loop, and editing constants in GDScript between renders is the slowest possible version of it. The rig keeps the choreography (which parameter a chapter ramps, in what order); the file keeps the values. Moving them out was verified to change nothing on screen |
| `demo/project.godot` sets a 1920x1080 viewport | `--write-movie` fixes its recording size before the scene loads, from `display/window/size/viewport_*` alone — neither `--resolution` nor a runtime resize moves it (both change the viewport while the movie keeps recording at the project size). `demo/benchmark` passes `--resolution 1920x1080` to every run it spawns, so its measurements already ran at this size and do not move |
| A configuration value is animated by writing it with `changed` blocked, then `refresh_parameters()` | The signal is the right response to an editor edit and far too much for a value moving every frame: `Terrain3D` answers it with a full `rebuild_mesh()`. `Object.set_block_signals()` around the writes plus a uniform re-push gets the value on screen without touching geometry. The two values that genuinely change the mesh — `mesh_resolution` and `clipmap_levels` — do get a real `rebuild()`, but only on a beat |
| Clip timing is driven by real audio cues, not guessed timestamps | Track is "Bliss" by Klsr, trimmed to ~1:07 with a bridge at that point separating the track's two halves — lines up with the storyboard's own pivot into the cinematic reveal. Cuts landing on the track's marked hits means the eventual edit needs minimal manual nudging |
| The plugin changes are a tooling block in `terrain.gdshader` and a live-refresh path in `TerrainRenderer` | The overlay has to attach to the real displaced surface, so it can only live in the shader that displaces it. Every uniform in that block is neutral at its default and the whole block sits behind one uniform-coherent branch, so a normal render is byte-identical to before it existed — verified. On the C++ side, `refresh_parameters()` re-pushes the configuration without rebuilding, and `set_shader_parameter()` gives tooling a channel to the uniforms that survives a rebuild, so neither needs a `TerrainConfiguration` field |

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
      **Superseded by T12**, which kept the six chapters and the cue timing and threw the diorama
      away: `diorama.gd` and `diorama_body.gdshaderinc` no longer exist, and the chapters now drive
      the real `Terrain3D`. The result notes below are the history of the diorama, kept for the
      traps in them; the current files are `trailer_rig.gd` (timeline, cue envelopes, cameras),
      `whittaker_overlay.gd` (the 2D chart) and `tools/pick_patch_center.gd` (patch selection).
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
      > Bug found later (during T6 tuning): the `normals`→`lit clay` wipe and every view after it
      > showed a fine dark speckle across shaded slopes, worst in shadow. Cause: `d_cell`, the
      > vertex-normal finite-difference step in `diorama_body.gdshaderinc`, was one fixed value
      > (`p_size / p_resolution * 0.35`, clamped to 12) shared by every ring, deliberately finer
      > than the mesh's own vertex spacing so "the shaded and normal views still show the detail
      > the noise actually has." That reasoning was backwards: sampling the normal at a higher
      > frequency than the mesh resolves makes adjacent vertices catch different, uncorrelated
      > phases of the noise, and Gouraud interpolation turns that into per-fragment speckle instead
      > of detail — the production shader avoids exactly this by setting its own differencing step
      > to `vertex_spacing = lod_scale / resolution` (`terrain.gdshader`'s `vertex()`). `d_cell` is
      > now an `instance uniform`, set per level in `diorama.gd`'s `_add_level()` to that level's
      > true `size / resolution`, the same fix. Verified by rendering the `normals` chapter's lit
      > moment (38.5s) and the `biomes` chapter (61.5s) with `wire_opacity` forced to 0 for a clean
      > look, before and after (`git stash` on just the two changed files): the speckle is gone from
      > the lit-clay lighting, the biome-colored view (only mildly shading-modulated to begin with)
      > is otherwise unaffected, and the full `--shot=build` run still completes with no errors.
      > Follow-up: the `d_cell` fix only quieted the lit-clay speckle, it did not remove it, and the
      > `biomes` view was still a flat per-biome colour rather than the real terrain textures. Both
      > are now fixed together. `diorama.gd`'s `_build_shader()` now splices two pieces of
      > `terrain.gdshader` instead of one: the noise/climate half before `void vertex()` (as
      > before) *and* the triplanar texturing helpers between the production `vertex()` and
      > `fragment()` (`axis_uv`, `triplanar_weights`, `triplanar_array`, ...), found by brace-
      > matching past the production `vertex()` body rather than a hardcoded offset. `diorama_body
      > .gdshaderinc`'s `diorama_biome_color()` (view 6) now calls `triplanar_array()` against real
      > `biome_albedo_textures`/`rock_albedo_textures`, built by a new `_build_texture_array()` in
      > `diorama.gd` from each `TerrainBiomeLayer`/`TerrainSlopeLayer`'s own `albedo_texture`
      > (falling back to a flat placeholder in that layer's diagram colour if a texture is missing
      > or its image can't be read back on the CPU). Turning textures on immediately reintroduced
      > the speckle, worse than before and no longer fixable by `d_cell`: the real cause was that
      > `_build_texture_array()` built its `Texture2DArray` straight from each image with no mip
      > chain, unlike `TerrainRenderer::_prepare_layer_image()` (`src/core/terrain_renderer.cpp`),
      > which calls `image->generate_mipmaps()` before handing images to
      > `texture_2d_layered_create()`. Without mips, `triplanar_array()`'s `textureGrad()` has only
      > the base level to sample regardless of how minified the surface is — which a fixed-resolution
      > mesh viewed at an angle does constantly — and that is what was actually aliasing into grain,
      > not the normal. `_build_texture_array()` now calls `img.generate_mipmaps()` on every image
      > (including the flat placeholders) before `create_from_images()`. Verified the same way as
      > above (`wire_opacity` forced to 0, `biomes` chapter, 61.5s): before the mipmap fix the
      > textured rock/snow slopes were a dense, sparkling grain (confirmed independent of `d_cell` —
      > widening it 4x changed nothing); after, they read as smooth, shaded rock and snow texture,
      > and the full `--shot=build` run still completes with no shader errors or warnings.

- [~] **T4 — Shot: erosion particles. Dropped from the storyboard.**
      The build-up has no travelling shot left to rain onto, and its "parameter arrives on a hit,
      surface visibly reshapes" beats already carry the before/after idea that the erosion shot
      existed to show — with the plugin's real parameters rather than a scripted config swap.
      Revive only if the cut wants a distinct particle beat; nothing in T3 depends on it.

- [x] **T5 — Shot: biome/normal debug-view cuts. Absorbed into T3.**
      These are now the `normals`, `climate` and `biomes` chapters of the build-up, wiped in on
      their cues instead of hard-cut, over the diorama rather than the `Terrain3D` node.
      > Note: for as long as the diorama existed this made **T1's `debug_view` uniform unused by
      > the trailer** — the diorama computed its own views, because it needed values T1 did not
      > expose and needed them on geometry `debug_view` could not reach. T12 inverted that: the
      > missing views (temperature, moisture, clay, blank) were added to `debug_view` itself
      > alongside the wireframe and the wipe, and the trailer now has no shader code of its own at
      > all. The one-at-a-time biome reveal turned out not to need per-biome weights either — the
      > shader's existing `biome_layer_count` does it, and does it as real classification.

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
      > Partial progress: the placeholder's ~45-degree, ~1900-unit-distance aerial held most of
      > the frame several clipmap levels out from the camera, which reads as flat and mottled next
      > to the demo scene's own close-up (that camera sits tens of units from the ground, deep
      > inside LOD0/LOD1). `_apply_cinematic()` now still opens on the diagram's exact last framing
      > for the cut, then eases (`cinematic.ease_duration`) into `cinematic.reveal` — a close, low
      > orbit (`distance` 480, `height` 300, `pitch` -20°) around the diorama centre, sized off a
      > real `get_height_at()` survey of the patch so the ring clears every sampled peak, then
      > drifts via `yaw_rate` so the shot moves instead of freezing for 80+ seconds. `_apply_camera`
      > and the transform math it ended in were split into a shared `_set_camera_from_state()` so
      > both the build-up and the reveal move the camera the same way — verified pixel-identical to
      > `HEAD` on a `build` still (`--shot=build --start=25.6 --frames=1`), and the full
      > `--shot=cinematic` run (4880 frames) completes headlessly with no errors and no
      > camera/terrain clipping across the eased-in orbit.
      > Still open: a chosen endpoint (the shot still runs to the track's raw end, 145.276s, per
      > T2's placeholder) and a full played-through look pass — this pass only fixed the
      > LOD/distance complaint, it did not re-pick the shot's cut point or dial `reveal` by eye
      > against the music the way `--controls` dials the build-up.

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

- [x] **T11 — The track under the preview (`--controls`).**
      `demo/trailer/trailer_music.gd` plays the music beneath the editor, and **while it plays it is
      the clock**: `trailer_rig.gd` hands it the moment it wants heard and takes back the stream's
      own position. Accumulating `delta` on the visual side instead would drift against the sound
      card within seconds, which is precisely the error the preview exists to expose. It re-seeks
      after a scrub, a cue jump or a loop wrap, follows the preview speed with `pitch_scale`, and
      toggles with M.
      Godot 4.7 has no FLAC loader at all — `ResourceLoader` reports no loader for the master, and
      `--import` produces no sidecar for it — so the editor plays an Ogg Vorbis transcode made by
      `tools/make_preview_audio.sh`. It is loaded with `AudioStreamOggVorbis.load_from_file()`, which
      reads the file directly and so needs no `.import` (gitignored anyway) and no editor pass. Cues
      and waveform are still extracted from the master, never from the transcode.
      *Verify:* the clock must actually follow the audio, and no sound may reach a recording.
      > Result: verified on both the Dummy and PulseAudio drivers — the stream clock advances
      > monotonically with real time; `sync()` given a matching time returns the audio's position
      > without re-seeking; given a 20-second jump it re-seeks for real (the clock lands at 52.015);
      > and given a paused preview it stops. The player is built only inside `_setup_controls()`,
      > which already refuses to run when `Engine.get_write_movie_path()` is set, so a recording
      > cannot reach it — recorded clips stay silent by design.
      > Two traps worth keeping: `AudioStreamPlayer.play()` fails ("Playback can only happen when a
      > node is inside the scene tree") if the node is added during a `SceneTree` script's
      > `_initialize()`, which made a first sync probe report success while measuring nothing; and
      > `get_playback_position()` only moves per mixed buffer, so the clock adds
      > `AudioServer.get_time_since_last_mix()` and subtracts the output latency, without which the
      > playhead visibly stair-steps against a smoothly moving image.

- [x] **T12 — Rebuild the trailer as a front end over the plugin.**
      Supersedes T3's diorama and T5's note about `debug_view` being unused. The build-up was a
      parallel implementation of the renderer — clipmap-shaped geometry built in GDScript, a
      spliced `unshaded` shader with its own fake lambert, biome textures rebuilt into its own
      CPU `Texture2DArray` — and it read as one: no real shadows, and textures configured
      differently from the demo scene. It is gone. `demo/trailer/diorama.gd` and
      `diorama_body.gdshaderinc` are deleted; `trailer.tscn` instances `demo/terrain_server.tscn`
      whole as the scenario, and the trailer drives the real `Terrain3D` through three channels
      and nothing else (see the intro). `tools/pick_diorama_center.gd` is renamed
      `pick_patch_center.gd`.
      Plugin side, two changes, both additive:
      `demo/addons/terrain_server/shaders/terrain.gdshader` grows T1's `debug_view` into a
      tooling block — views 4/5 (temperature, moisture), 6 (clay: flat albedo under real
      lighting), 7 (blank), a second view plus a world-space wipe between the two, a wireframe off
      the mesh's own UV cell grid with a pixel-spacing density fade and a major grid, a Chebyshev
      reveal radius that `discard`s before the parallax raymarch, an emission gain and a vignette.
      `src/core/terrain_renderer.*` and `src/nodes/terrain3d.*` gain `refresh_parameters()` (the
      `rebuild_mesh()` uniform block factored into a shared `_push_parameters()`, so the live path
      cannot drift from the rebuild path — and it re-sets the custom AABB, without which a live
      `height_scale` ramp grows the surface straight out of a stale one and the terrain culls) and
      `set_shader_parameter()`/`get_shader_parameter()`/`clear_shader_parameters()`, which
      remember what they push and re-apply it after a rebuild replaces every material.
      Chapter semantics are now plugin state throughout: `grid` steps `clipmap_levels` 1→6 on its
      ring cues with a real `rebuild()` behind each, `noise` steps `noise_octaves` at
      `height_scale` 0, `relief` ramps `height_scale` and steps `mesh_resolution` 64→256,
      `normals`/`climate` wipe between debug views, and `biomes` steps the shader's own
      biome count 1→4 so each hit widens the set of layers the Whittaker blend may choose from, then
      wipes the flat ids through to the textured render and opens the parallax fade window.
      *Verify:* the plugin's normal output must not move, and every chapter must render.
      > Result: **byte-identical.** A normal render (`debug_view` 0, defaults) of the terrain from
      > a fixed camera matches `HEAD` exactly, checked separately for each half of the change — the
      > new shader against `HEAD`'s with the same binary, then `HEAD`'s `src/` rebuilt against the
      > same shader. Worth keeping for the next A/B: `demo/terrain_server.tscn` is **not** a valid
      > oracle, because its falling `RigidBody3D` test sphere makes the frame differ run to run
      > even with nothing changed at all (confirmed: two runs of the same build, different MD5s).
      > A scene holding only the demo's environment, its light and a `Terrain3D` on a fixed focus
      > is deterministic, and that is what the comparison used.
      > Every chapter was then rendered and looked at (14 stills across the window). Four things
      > the port surfaced: the scenario's own `Camera3D` enters the tree first and stays the
      > viewport's current camera, and hiding it changes nothing — `make_current()` on the
      > trailer's camera is the only fix; a `Node.PROCESS_MODE_DISABLED` on the rig to freeze its
      > clock is inherited by `Terrain3D` and stops it ever placing its clipmap levels, so
      > `set_process(false)` is what a still-capture harness wants; the height view has to be
      > re-evaluated per fragment rather than interpolated from a varying, or it is only as sharp
      > as `mesh_resolution` and vanishes entirely at `height_scale` 0; and swapping the demo's sky
      > for black takes the sky's fill light with it, which turns every shadow into a blotchy black
      > hole — a dim neutral `ambient_light_color` puts the fill back, and the tonemap is
      > deliberately inherited rather than forced to linear so the textured beats grade the way the
      > demo scene does.
      > One latent bug fixed on the way: `pom_fade_start == pom_fade_end` (which is how the trailer
      > switches parallax off) hits `smoothstep` with `edge0 >= edge1`, undefined in GLSL. The
      > shader now clamps the span.
      > Dropped with the diorama, all of them diorama-only effects with no plugin state behind
      > them: the beat ripple (`ripple_*`) and the skirt walls (`skirt_*`) needed vertex
      > displacement uniforms of their own; `shade_amount` was the fake lambert's strength, and
      > real lighting replaces it; per-level `ring_intensity` went with the per-instance uniform;
      > and the one-biome-at-a-time *slope/cliff* reveal is gone, because the rock threshold in
      > effect is a per-layer array the renderer derives from the biome and slope layers, not a
      > scalar a chapter can ramp.
      > **The authored values are a starting point, not a tuned look.** The real clipmap spans
      > ±4096 against the diorama's ±3072 half-patch, and its level 0 is only 256 units against
      > the diorama's 1536, so every camera distance, dolly rate and fill had to be re-picked
      > rather than carried over. They were set by eye on stills; the window has not been played
      > through against the music. Dial it with `--controls`.

- [x] **T13 — Biomes chapter: drop the Whittaker overlay, sweep the biomes in.**
      Two changes to the `biomes` chapter, both asked for after seeing T12's stills.
      The 2D Whittaker chart is gone — `demo/trailer/whittaker_overlay.gd` is deleted and
      `trailer.tscn` no longer has an `Overlay` layer. (The plugin's own `WhittakerChart` in the
      editor dock is untouched; this was only the trailer's copy of the idea.)
      The reveal no longer pops. `terrain.gdshader` gains `debug_biome_count` /
      `debug_biome_count_b`, which put the *biome count* on the same wipe front the views already
      change on: behind the line the blend may choose from the first N layers, ahead of it from the
      first N−1, so a biome sweeps in across the terrain. It is one per-fragment integer selection
      in front of the existing weight loop — not a second blend — and it is off at its default
      (−1), so a normal render still uses `biome_layer_count` unchanged. The wipe front itself is
      factored into `debug_wipe_at()` and now runs whenever it is carrying *either* a view change
      or a biome, with the seam highlight riding it in both cases. The rig alternates
      `reveal_wipe_dir`'s sign per hit so consecutive reveals do not all sweep the same way.
      Particles ("fall from the sky") were the other option offered and were not taken: painting
      biomes from particle impacts needs a splat texture the shader has no input for, so the
      particles would have been decoration over the same pop.
      *Verify:* the plugin's normal output must still not move, and each hit must read as a sweep.
      > Result: the normal render is **still byte-identical to `HEAD`** on the same deterministic
      > oracle T12 used. Each of the four hits was rendered and looked at: the front crosses frame
      > with its seam lit, the new biome behind it, alternating direction.
      > One thing this surfaced, and it applies to every wipe in the trailer, not just these: the
      > front's travel range is not a look value, it is a correctness one. Shortening `debug_extent`
      > to make a sweep read faster on screen (tried, as `look.wipe_extent` = 1800 against a ±4096
      > patch) leaves everything past it unconverted, to snap the instant the wipe reaches 1.0 —
      > which is a worse artefact than the pacing it was meant to fix. `debug_extent` is the
      > clipmap's own outermost half-width, and `debug_wipe_at()` now derives the front's reach as
      > that square's true projection onto the wipe direction (so a diagonal covers the corners too)
      > plus the band's own width at each end (so the last fragment finishes converting rather than
      > stopping half-blended). Verified by sampling 1 ms either side of a reveal completing, where
      > camera drift is nil: 0.012% of pixels change, and those are the seam switching off at the
      > far edge. Pacing is the durations' job — they were lengthened to suit the longer travel.
      > `reveal_duration` is 0.78s because the last two biome cues are 1.10s and 0.81s apart; a
      > longer reveal is cut off by the next front.

- [ ] **T7 — Recording convention.**
      Document the exact `--write-movie` invocation per shot (1920x1080 @ 60fps) in a short
      section at the bottom of this file, once T3–T6 exist to be recorded.
      *Verify:* a reader can reproduce any clip from the command alone.

---

## Out of scope

Titles, logo card, music mixing, color grading and final cut assembly — the user does all of
this themselves outside the repo. No ffmpeg/Remotion assembly step, no audio mixing inside
Godot, no ambition to make `--write-movie`'s output itself the finished trailer: every recorded
clip is silent.

The one `AudioStreamPlayer` in the project (T11) is a tuning instrument, not part of the output.
It exists only on the `--controls` path — which refuses to run during a recording — so it cannot
reach a rendered frame. It plays the track under the editor so a cut can be judged by ear
instead of against a list of timestamps.
