extends Node3D

# Front end for the showreel trailer clips (see TODO_TRAILER.md).
#
# This script renders nothing itself. The picture is the plugin: `demo/terrain_server.tscn` is
# instanced whole as the scenario, so the terrain on screen is a real `Terrain3D` with the demo's
# own `TerrainConfiguration`, environment and light -- the mesh, the biome blend, the textures, the
# parallax and the shadows are all exactly what the plugin produces in that scene, with no
# trailer-side copy of any of it.
#
# What the trailer contributes is choreography, and it reaches the plugin through three channels
# and nothing else:
#
#   * `TerrainConfiguration` values, animated per frame and pushed with
#     `Terrain3D.refresh_parameters()` (no rebuild) -- height_scale, octaves, ridge, warp,
#     continent, redistribution. The build-up's "parameter arrives on a hit" beats are the real
#     parameters moving.
#   * `Terrain3D.rebuild()`, on beats only, for the values that change the mesh itself --
#     `clipmap_levels` and `mesh_resolution`. That is the expanding-clipmap beat: the real
#     parameter growing, not an animation of it.
#   * `Terrain3D.set_shader_parameter()`, for the diagram overlay uniforms `terrain.gdshader`
#     declares for tooling: the debug views, the wireframe, the reveal radius, and the wipe front
#     that carries both a change of view and a biome arriving.
#
# The two shots are split by the track's bridge cue: `build` is the diagram build-up in black,
# `cinematic` is the same terrain in the demo scene's own environment on the other side of the cut.
#
# Everything on screen is a pure function of the shot time: no state accumulates between frames
# (bar the geometry cache, which only ever reflects the current moment's values), so any moment can
# be previewed on its own with --start=<sec> --frames=<n>, and --write-movie renders the identical
# result on any machine.
#
# This script holds no authored numbers. Every time, camera, colour and effect value lives in
# trailer_timeline.json, which `--controls` lets you edit live and save back. What stays here is
# the choreography itself: which parameter a chapter moves, and in what order.

const CUES_PATH := "res://trailer/audio_cues.json"
const PEAKS_PATH := "res://trailer/audio_peaks.json"
const TIMELINE_PATH := "res://trailer/trailer_timeline.json"
const FPS := 60.0

# Cue kinds as the editor's ruler wants them, flattened out of audio_cues.json's string field.
const KIND_ONSET := 0
const KIND_STRONG := 1
const KIND_BRIDGE := 2

# terrain.gdshader's debug views. Only the shaded one is named here, because it is the only one
# this script picks itself -- every other view is authored per chapter in trailer_timeline.json,
# whose header lists them: 1 height, 2 biome id, 3 normals, 4 temperature, 5 moisture, 6 clay,
# 7 blank.
const VIEW_SHADED := 0

# Configuration properties the chapters ramp, and the scale each one starts from before its own
# beat has arrived. A value of 1.0 means "starts at the config's own value" (nothing to ramp);
# these are the ones that start somewhere else and arrive later.
const RAMPED_PROPERTIES := [
	"height_scale", "noise_octaves", "noise_ridge_amount", "noise_warp_amount",
	"noise_continent_influence", "noise_continent_elevation", "noise_relief_floor",
	"noise_redistribution", "pom_fade_start", "pom_fade_end",
]

# What the generator adds on top of its fBm, in the order the noise chapter's cycles add it: one
# term per beat once every octave is in, each ramping from neutral to the demo configuration's own
# value, so the step the cycle ends on leaves exactly the heightmap the plugin renders. The names
# are keys into the shaping dictionary `_noise_build_at()` returns and `_apply_build()` maps onto
# TerrainConfiguration properties -- `continent` carries the three that arrive together (influence,
# elevation and the relief floor they lift the land off).
const NOISE_SHAPING_STEPS := ["ridge", "warp", "continent", "redistribution"]

# 2*PI / phi^2, the angle a sunflower packs its seeds at. The noise chapter walks its patches
# around a spiral of these, which is what keeps consecutive fields far apart without any two of
# them ever landing on the same place. See _seed_offset().
const GOLDEN_ANGLE := 2.3999632297

# Preview-only, and built only on the --controls path, which already refuses to run during a
# recording -- so the music can never reach a rendered clip. See trailer_music.gd.
const MUSIC_SCRIPT := preload("res://trailer/trailer_music.gd")

var timeline := {}

var _shot := "build"
var _preview_start := 0.0
var _preview_frames := 0
var _controls_requested := false
var _controls_active := false
var _music_requested := true
var _music: AudioStreamPlayer

# A copy of the demo scene's configuration, so animating it per frame cannot touch the shared
# resource on disk. Shallow: the biome and slope layers are the demo's own, textures included.
var _config: TerrainConfiguration
# The demo scene's authored values -- the targets every ramp animates towards.
var _full := {}

var _cue_times := PackedFloat32Array()
var _cue_amplitudes := PackedFloat32Array()
var _cue_kinds := PackedByteArray()
# Detected onset strength per cue, normalised across the track to [0,1].
var _cue_strengths := PackedFloat32Array()
var _strong_times := PackedFloat32Array()

# Waveform envelope for the editor's ruler. Display-only: nothing in the choreography reads it.
var _peaks := PackedFloat32Array()
var _peaks_rms := PackedFloat32Array()
var _peaks_fps := FPS

var _frame := 0
var _time := 0.0
var _playing := true
var _speed := 1.0
# Vector2(-1, -1) means "no region": preview loops the whole shot window.
var _loop_region := Vector2(-1.0, -1.0)
var _shot_start := 0.0
var _shot_end := 0.0
var _shot_duration_frames := 0
var _finished := false

# The (levels, resolution) pair the clipmap was last rebuilt at, so a frame that asks for the pair
# it already has does not rebuild. Not accumulated state: it is always whatever the current
# moment's values are, so seeking anywhere reaches the same geometry.
var _built_levels := -1
var _built_resolution := -1

# The build-up's final ring count, computed once in `_setup_scenario()` from the "grid" chapter's
# `ring_times`. Every build chapter defaults its clipmap level count to this.
var _build_clipmap_levels := 1

# The diagram's patch, taken from the real clipmap rather than authored: the centre it is focused
# on and the half-width of its outermost level. `_center` is the patch the current frame is built
# on, which the noise chapter moves off `_patch_center` to re-seed its field -- see `_apply_seed()`.
var _patch_center := Vector2.ZERO
var _center := Vector2.ZERO
var _extent := 1.0

var _demo_environment: Environment
var _diagram_environment: Environment
var _light_basis := Basis()

@onready var _scenario: Node3D = $Scenario
@onready var _terrain: Terrain3D = $Scenario/Terrain3D
@onready var _world_environment: WorldEnvironment = $Scenario/WorldEnvironment
@onready var _light: DirectionalLight3D = $Scenario/DirectionalLight3D
@onready var _focus: Node3D = $Focus
@onready var _camera: Camera3D = $Camera3D
@onready var _controls: CanvasLayer = $Controls


func _ready() -> void:
	_parse_args()

	if not _load_timeline():
		return

	var shots: Dictionary = timeline["shots"]
	if not shots.has(_shot):
		_abort("unknown shot '%s'; known shots are %s" % [_shot, ", ".join(shots.keys())])
		return

	if not _setup_scenario():
		return

	_load_cues()
	_load_peaks()

	var window: Array = shots[_shot]
	_shot_start = window[0]
	_shot_end = window[1]
	_shot_duration_frames = int(round((_shot_end - _shot_start) * FPS))
	if _preview_frames > 0:
		_shot_duration_frames = _preview_frames

	_setup_shot()
	_setup_controls()

	_time = _shot_start + _preview_start
	_apply_time(_time)

	print("[trailer] shot=%s window=%.3fs-%.3fs start=%.3fs frames=%d centre=%s extent=%.0f viewport=%s controls=%s" % [
			_shot, _shot_start, _shot_end, _time, _shot_duration_frames, str(_patch_center), _extent,
			str(get_viewport().size), str(_controls_active)])

	# The noise chapter's schedule is derived from the cues rather than authored, so it is worth
	# saying out loud: how many beats it found, and how many times the field gets rebuilt on them.
	var noise := _chapter_named("noise")
	if _shot == "build" and not noise.is_empty():
		var beats := _beat_times(noise)
		var times := _step_times(noise)
		var per_cycle := maxi(int(_full["noise_octaves"]), 1) + NOISE_SHAPING_STEPS.size()
		var span := _chapter_end(noise) - float(noise["start"])
		print("[trailer] noise: %.3fs-%.3fs, %d beats at >=%.2fs x%d = %d steps (%.1f/s), %d per cycle (%d octaves + %s), %d cycles, %d steps holding" % [
				float(noise["start"]), _chapter_end(noise), beats.size(),
				float(noise.get("step_min_gap", 0.0)), maxi(int(noise.get("step_subdivisions", 1)), 1),
				times.size(), float(times.size()) / maxf(span, 0.001), per_cycle,
				int(_full["noise_octaves"]), ", ".join(NOISE_SHAPING_STEPS),
				maxi(times.size() / per_cycle, 1), times.size() % per_cycle])


# Takes over the instanced demo scene: its camera and its physics test rig have no place in a
# trailer frame, its Terrain3D is re-pointed at a fixed focus so the clipmap stays centred on the
# diagram's patch instead of following a camera, and its configuration is replaced with a copy this
# script may safely animate.
func _setup_scenario() -> bool:
	# These three are the trailer's whole dependency on the demo scene's shape. Named rather than
	# searched for by type, so renaming one there fails here with a sentence instead of a null
	# dereference twenty lines later.
	for named in [["Terrain3D", _terrain], ["WorldEnvironment", _world_environment],
			["DirectionalLight3D", _light]]:
		if named[1] == null:
			_abort("the instanced scenario has no %s; trailer.tscn expects demo/terrain_server.tscn" % named[0])
			return false

	# The demo scene is a working test scene: it carries its own camera and a physics test rig
	# (a sphere dropped on the terrain), neither of which belongs in a trailer frame. Everything
	# that is not the terrain, the light or the environment is taken out of the shot rather than
	# deleted, so the demo scene itself needs no trailer-shaped edits.
	for child in _scenario.get_children():
		if child == _terrain or child == _world_environment or child == _light:
			continue
		if child is Node3D:
			child.visible = false
		child.process_mode = Node.PROCESS_MODE_DISABLED

	# The scenario enters the tree first, so its own Camera3D is already the viewport's current one
	# and hiding it does not change that -- `visible` has no bearing on which camera renders. Taking
	# the current slot explicitly is the only thing that puts the trailer's camera on screen.
	_camera.make_current()

	var source: TerrainConfiguration = _terrain.configuration
	if source == null:
		_abort("the scenario's Terrain3D has no configuration")
		return false

	for property in RAMPED_PROPERTIES:
		_full[property] = source.get(property)
	_full["clipmap_levels"] = source.clipmap_levels
	_full["mesh_resolution"] = source.mesh_resolution

	# The build-up's own final ring count, from "grid" reaching its last `ring_times` hit -- not
	# the demo scene's `clipmap_levels`, which is a different (and generally lower) number. Every
	# build chapter after "grid" defaults to this so the clipmap never shrinks back down once
	# "grid" has grown it, which is what left a black gap past the mesh's edge in later chapters.
	_build_clipmap_levels = 1 + _chapter_named("grid")["ring_times"].size()

	_config = source.duplicate()
	_terrain.configuration = _config

	var patch: Dictionary = timeline["patch"]
	_focus.global_position = _to_vector3(patch["center"])
	_terrain.focus_path = _terrain.get_path_to(_focus)

	# The patch the diagram is measured against comes from the plugin's own numbers, not from
	# authored ones: the focus the clipmap snaps to, and the half-width of its outermost level.
	# Built at `_build_clipmap_levels` rather than the demo config's own (smaller) count, so
	# `_extent` matches the actual outermost ring the build-up chapters render.
	_config.clipmap_levels = _build_clipmap_levels
	_built_levels = _build_clipmap_levels
	_terrain.rebuild()
	_patch_center = Vector2(_focus.global_position.x, _focus.global_position.z)
	_center = _patch_center
	_extent = _level_extent(maxi(_terrain.get_clipmap_level_count() - 1, 0))

	_demo_environment = _world_environment.environment
	_diagram_environment = _build_diagram_environment(_demo_environment)
	_light_basis = _light.global_transform.basis

	return true


func _process(delta: float) -> void:
	if _finished:
		return

	# Interactive tuning runs on wall-clock time and loops the shot window (or the region the ruler
	# has set); recording stays indexed by frame number so --write-movie is reproducible regardless
	# of machine speed.
	if _controls_active:
		if _playing:
			_time += delta * _speed
			var from := get_loop_start()
			var to := get_loop_end()
			if _time >= to:
				_time = from
			elif _time < from:
				_time = from
		# While the track is rolling it, not the frame clock, says where we are -- otherwise the
		# picture drifts against the sound within seconds, which is the very error this is here to
		# reveal. sync() also re-seeks after a scrub or a loop wrap, and is a no-op when muted.
		if _music != null:
			_time = _music.sync(_time, _playing, _speed)
		_apply_time(_time)
		_controls.sync_time(_time)
		return

	_frame += 1
	_time = _shot_start + _preview_start + float(_frame) / FPS
	_apply_time(_time)

	if _frame >= _shot_duration_frames:
		_finished = true
		print("[trailer] shot=%s done at frame %d" % [_shot, _frame])
		get_tree().quit()


# --- Control panel interface ----------------------------------------------------------------

# Re-applies the current moment after the panel edits a value, so a slider shows its effect on the
# paused frame immediately.
func refresh() -> void:
	_setup_environment()
	_apply_time(_time)


func set_time(p_time: float) -> void:
	_time = clampf(p_time, _shot_start, _shot_end)
	_apply_time(_time)


func get_time() -> float:
	return _time


# Not get_window(): that name is taken by Node and returns a Window.
func get_shot_window() -> Vector2:
	return Vector2(_shot_start, _shot_end)


func set_playing(p_playing: bool) -> void:
	_playing = p_playing


func is_playing() -> bool:
	return _playing


func set_speed(p_speed: float) -> void:
	_speed = maxf(p_speed, 0.01)


func get_speed() -> float:
	return _speed


# An empty region (to <= from) restores looping over the whole shot window.
func set_loop_region(p_from: float, p_to: float) -> void:
	if p_to <= p_from:
		_loop_region = Vector2(-1.0, -1.0)
	else:
		_loop_region = Vector2(clampf(p_from, _shot_start, _shot_end), clampf(p_to, _shot_start, _shot_end))


func get_loop_region() -> Vector2:
	return _loop_region


func has_loop_region() -> bool:
	return _loop_region.y > _loop_region.x


func get_loop_start() -> float:
	return _loop_region.x if has_loop_region() else _shot_start


func get_loop_end() -> float:
	return _loop_region.y if has_loop_region() else _shot_end


func has_music() -> bool:
	return _music != null


func is_music_enabled() -> bool:
	return _music != null and _music.is_enabled()


func set_music_enabled(p_enabled: bool) -> void:
	if _music != null:
		_music.set_enabled(p_enabled)


func set_music_volume(p_db: float) -> void:
	if _music != null:
		_music.volume_db = p_db


func get_cue_times() -> PackedFloat32Array:
	return _cue_times


func get_cue_kinds() -> PackedByteArray:
	return _cue_kinds


func get_strong_cue_times() -> PackedFloat32Array:
	return _strong_times


func get_peaks() -> PackedFloat32Array:
	return _peaks


func get_peaks_rms() -> PackedFloat32Array:
	return _peaks_rms


func get_peaks_fps() -> float:
	return _peaks_fps


# Throws away every unsaved edit by re-reading the file the panel writes. Every value is re-pushed
# from the timeline each frame, so re-applying the current moment is all it takes for the revert to
# show -- except the geometry pair, whose cache is dropped so a reverted resolution rebuilds.
func reload_timeline() -> bool:
	if not _load_timeline():
		return false
	_built_levels = -1
	_built_resolution = -1
	refresh()
	return true


func get_chapter_index(p_time: float) -> int:
	var chapters: Array = timeline["chapters"]
	var index := 0
	for i in chapters.size():
		if p_time >= float(chapters[i]["start"]):
			index = i
	return index


func save_timeline() -> bool:
	var file := FileAccess.open(TIMELINE_PATH, FileAccess.WRITE)
	if file == null:
		printerr("[trailer] could not write %s" % TIMELINE_PATH)
		return false
	file.store_string(JSON.stringify(timeline, "\t") + "\n")
	file.close()
	print("[trailer] saved %s" % TIMELINE_PATH)
	return true


# --- Shot setup -----------------------------------------------------------------------------

func _setup_shot() -> void:
	_setup_environment()


# The diagram's environment, derived from the demo scene's rather than authored from scratch, so
# only the things the diagram actually needs to differ do: black instead of sky, and no fog or SSAO
# competing with the flat views. The tonemap in particular is deliberately left alone -- the
# build-up's last beats are the plugin's real textures under real light, and they have to grade the
# same way the demo scene does or the bridge cut lands on a different-looking image.
func _build_diagram_environment(p_source: Environment) -> Environment:
	var environment: Environment = p_source.duplicate()
	environment.background_mode = Environment.BG_COLOR
	environment.background_color = Color.BLACK
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.fog_enabled = false
	environment.ssao_enabled = false
	environment.glow_enabled = true
	return environment


func _setup_environment() -> void:
	if _shot != "build":
		_world_environment.environment = _demo_environment
		return

	var look: Dictionary = timeline["look"]
	# Swapping the demo's sky for black takes the sky's fill light with it, and the lit views
	# (clay, and the textured render at the end of the build-up) then drop to pure black wherever
	# the key light does not reach -- which reads as blotchy holes in the shadow rather than as
	# shade. A dim neutral ambient puts the fill back without putting a sky back, and stays neutral
	# so the textured beats are not tinted away from what the demo scene shows.
	_diagram_environment.ambient_light_color = _to_srgb_color(look["ambient_color"])
	_diagram_environment.ambient_light_energy = float(look["ambient_energy"])
	_diagram_environment.glow_intensity = float(look["glow_intensity"])
	_diagram_environment.glow_strength = float(look["glow_strength"])
	_diagram_environment.glow_bloom = float(look["glow_bloom"])
	_diagram_environment.glow_hdr_threshold = float(look["glow_hdr_threshold"])
	_diagram_environment.set("glow_levels/4", 1.0)
	_diagram_environment.set("glow_levels/5", 0.5)
	_world_environment.environment = _diagram_environment


func _setup_controls() -> void:
	if not _controls_requested:
		_controls.queue_free()
		return

	# The panel is drawn into the same viewport --write-movie captures, so it must never be up
	# during a recording. Engine.get_write_movie_path() is the only reliable signal here:
	# OS.get_cmdline_args() has the engine's own flags stripped out of it and never mentions
	# --write-movie, so testing that silently let the panel into a recording.
	if not Engine.get_write_movie_path().is_empty():
		printerr("[trailer] --controls ignored: the panel would be recorded into the movie")
		_controls.queue_free()
		return

	_controls_active = true
	_setup_music()
	_controls.setup(self)


# Built here rather than in the scene so it cannot exist at all outside a --controls session, and so
# a checkout with no preview audio simply tunes without sound instead of erroring.
func _setup_music() -> void:
	if not _music_requested:
		return

	var music: AudioStreamPlayer = MUSIC_SCRIPT.new()
	music.name = "Music"
	# In the tree before setup(): AudioStreamPlayer refuses to play from outside it.
	add_child(music)
	if music.setup():
		_music = music
	else:
		music.queue_free()


func _apply_time(t: float) -> void:
	if _shot == "build":
		_apply_build(t)
	else:
		_apply_cinematic(t)


# --- Plugin channels ------------------------------------------------------------------------

# Pushes animated TerrainConfiguration values without a rebuild. The configuration's `changed`
# signal is blocked around the writes: Terrain3D answers it with a full rebuild_mesh(), which is
# the right response to an editor edit and far too much for a value moving every frame.
func _apply_config(p_values: Dictionary) -> void:
	_config.set_block_signals(true)
	for property in p_values:
		_config.set(property, p_values[property])
	_config.set_block_signals(false)
	_terrain.refresh_parameters()


# The two configuration values that change the mesh rather than just its shape, applied by an
# actual rebuild -- and only when they move, which is on a beat, never per frame.
func _apply_geometry(p_levels: int, p_resolution: int) -> void:
	if p_levels == _built_levels and p_resolution == _built_resolution:
		return

	_built_levels = p_levels
	_built_resolution = p_resolution

	_config.set_block_signals(true)
	_config.clipmap_levels = p_levels
	_config.mesh_resolution = p_resolution
	_config.set_block_signals(false)
	_terrain.rebuild()


func _apply_overlay(p_values: Dictionary) -> void:
	for name in p_values:
		_terrain.set_shader_parameter(name, p_values[name])


# --- Build shot -----------------------------------------------------------------------------

func _apply_build(t: float) -> void:
	var look: Dictionary = timeline["look"]
	var effects: Dictionary = timeline["effects"]
	var chapter: Dictionary = timeline["chapters"][get_chapter_index(t)]

	# Defaults every chapter starts from, then overrides below. Keeping them here (rather than
	# letting values persist) is what makes any single frame renderable on its own.
	var levels := _build_clipmap_levels
	var resolution := int(chapter.get("mesh_resolution", _full["mesh_resolution"]))
	var reveal_radius := -1.0
	var ripple_radius := _ripple_off_array(-1.0)
	var ripple_strength := _ripple_off_array(0.0)
	var height_scale := float(_full["height_scale"])
	var octaves := int(_full["noise_octaves"])
	var parallax := 1.0
	# -1 on both sides leaves the shader's own biome_layer_count alone; see the biomes chapter.
	var biome_count_a := -1
	var biome_count_b := -1
	var view_a := int(chapter.get("view_a", VIEW_SHADED))
	var view_b := int(chapter.get("view_b", view_a))
	var wipe := 0.0
	var wipe_dir := _to_vector2(chapter.get("wipe_dir", [1.0, 0.0]))
	# Where the front starts and ends, as world-space distance along wipe_dir from _center, and how
	# soft its band is -- all authored per chapter rather than derived from the clipmap's own size,
	# so a chapter can land the front short of the mesh's true edge (e.g. out of camera view).
	var wipe_start := float(chapter.get("wipe_start", -_extent))
	var wipe_end := float(chapter.get("wipe_end", _extent))
	var wipe_width := float(chapter.get("wipe_width", look["wipe_glow"]))
	var fill := float(chapter.get("fill", 1.0))
	var wire_intensity := float(chapter.get("wire_intensity", look["wire_intensity"]))
	var wire_opacity := float(chapter.get("wire_opacity", look["wire_opacity"]))

	# The generator's own state at this moment, which is the noise chapter's business wherever we
	# are on the timeline: it builds the field beat by beat inside each of its cycles, and every
	# other chapter reads that same state clamped -- nothing shaped before the chapter, the demo
	# configuration's own heightmap after it. Resolved before the camera because re-seeding the
	# field relocates the patch, and the camera orbits the patch.
	var noise_build := _noise_build_at(_chapter_named("noise"), t)
	var shaping: Dictionary = noise_build["shaping"]
	var seed_offset: Vector2 = noise_build["seed"]
	_apply_seed(seed_offset, resolution)

	var pulse := _pulse_at(t)
	_apply_camera(chapter, t, pulse)
	_apply_light(t)

	match chapter["name"]:
		"grid":
			# The clipmap draws itself: level 0 outward from its centre at the first hit, then one
			# ring per hit, each one a real clipmap_levels step with the mesh rebuilt behind it.
			# There is no terrain yet -- height_scale is still zero, so this is the flat lattice.
			var rings := _step_count(t, chapter["ring_times"], 0)
			levels = 1 + rings
			reveal_radius = _ease_out(_ramp(t, float(chapter["first_hit"]),
					float(chapter["level0_duration"]))) * _level_extent(0)
			for i in rings:
				reveal_radius = maxf(reveal_radius, _ease_out(_ramp(t, float(chapter["ring_times"][i]),
						float(chapter["ring_duration"]))) * _level_extent(i + 1))
			# A ring expanding out of the centre on the same hit that grows the mesh -- a raindrop
			# landing on still water each time a level arrives, layering on top of any earlier ring
			# still fading rather than replacing it. Each ripple's own speed is its level's extent
			# over the same duration the lattice takes to reveal it, so the wave always lands on the
			# new edge exactly as that ring's growth animation stops, at any level: short first
			# hops read as a slow wave because they are a short distance over level0_duration, and
			# each doubling level travels twice as far in the same ring_duration rather than
			# lagging behind it.
			var ripple_hits: Array = [Vector3(float(chapter["first_hit"]), _level_extent(0),
					float(chapter["level0_duration"]))]
			for i in chapter["ring_times"].size():
				ripple_hits.append(Vector3(float(chapter["ring_times"][i]), _level_extent(i + 1),
						float(chapter["ring_duration"])))
			var ripples := _ripples_at(t, ripple_hits, float(effects["ripple_decay"]))
			for i in ripples.size():
				ripple_radius[i] = ripples[i].x
				ripple_strength[i] = ripples[i].y
			height_scale = 0.0
			octaves = int(chapter["octaves"])
			parallax = 0.0

		"noise":
			# Flat still: this beat is about the height field itself, not the relief, so the height
			# view wipes in over the lattice and the generator runs on it a step per beat -- an
			# octave of the plugin's own fBm at a time, then each term the configuration shapes it
			# with, ending on exactly the heightmap the demo scene renders.
			#
			# It runs more than once. When the last step lands, the next beat re-seeds the field and
			# the same build starts over on a different one. See _noise_build_at().
			height_scale = 0.0
			octaves = int(noise_build["octaves"])
			parallax = 0.0
			# Only the chapter's own entry wipes anything in: the field is global, so there is no
			# wiping an old one out against a new one, and a re-seed is the field changing outright
			# on the beat. (Sweeping each new field back in over the blank lattice was tried and
			# dropped: the front starts off-frame, so it costs a black frame or two every time.)
			wipe = _ease_in_out(_ramp(t, float(chapter["start"]), float(chapter["wipe_duration"])))

		"relief":
			# The finished field stands up into terrain, and gains the mesh resolution to carry the
			# detail it already has. The shaping parameters are not re-staged here: the noise
			# chapter's cycles have already arrived every one of them, and what lifts is the
			# heightmap they left.
			height_scale *= _back_out(_ramp(t, float(chapter["start"]), float(chapter["lift_duration"])))
			parallax = 0.0
			resolution = _step_resolution(chapter, t, resolution)

		"normals":
			# What the shader derives from that surface: the world normal, then the same normal
			# carrying real light instead of a colour ramp.
			wipe = _ease_in_out(_ramp(t, float(chapter["start"]), float(chapter["wipe_duration"])))
			parallax = 0.0
			if t >= float(chapter["lit_time"]):
				view_a = int(chapter["lit_view_a"])
				view_b = int(chapter["lit_view_b"])
				wipe = _ease_in_out(_ramp(t, float(chapter["lit_time"]), float(chapter["lit_duration"])))
				wipe_dir = _to_vector2(chapter["lit_wipe_dir"])

		"climate":
			wipe = _ease_in_out(_ramp(t, float(chapter["start"]), float(chapter["wipe_duration"])))
			parallax = 0.0
			if t >= float(chapter["moisture_time"]):
				view_a = int(chapter["moisture_view_a"])
				view_b = int(chapter["moisture_view_b"])
				wipe = _ease_in_out(_ramp(t, float(chapter["moisture_time"]),
						float(chapter["wipe_duration"])))
				wipe_dir = _to_vector2(chapter["moisture_wipe_dir"])
			# Into the track's silence the whole diagram dims down to almost nothing, so the biome
			# hit lands on a near-black frame.
			var breath := _ease_in_out(_ramp(t, float(chapter["breath_start"]),
					float(chapter["breath_duration"])))
			fill = lerpf(fill, float(chapter["fill_breath"]), breath)
			wire_intensity = lerpf(wire_intensity, float(look["wire_intensity_dim"]), breath)

		"biomes":
			# Snap back out of the breath, overshooting bright on the hit itself.
			fill += float(chapter["fill_snap"]) * exp(
					-maxf(t - float(chapter["start"]), 0.0) / float(chapter["fill_snap_decay"]))

			# The biomes arrive one per hit as real classification, not as a colour overlay: the
			# shader's blend is allowed one more layer to choose from per hit. And it arrives on the
			# same sweeping line the view wipes use -- the front separates the two counts, so the
			# biome sweeps in across the terrain instead of popping in everywhere at once.
			#
			# The first hit is the one that also carries the view from moisture to the flat biome
			# ids, so it moves the view rather than the count.
			var times: Array = chapter["biome_reveal_times"]
			var step := _step_count(t, times, 1)
			biome_count_a = step
			biome_count_b = step

			if step == 1:
				wipe = _ease_in_out(_ramp(t, float(chapter["start"]), float(chapter["wipe_duration"])))
			else:
				view_a = view_b
				var reveal := _ramp(t, float(times[step - 1]), float(chapter["reveal_duration"]))
				if reveal < 1.0:
					biome_count_a = step - 1
					wipe = _ease_in_out(reveal)
					# Alternating, so consecutive reveals do not all sweep the same way.
					wipe_dir = _to_vector2(chapter["reveal_wipe_dir"])
					if step % 2 == 0:
						wipe_dir = -wipe_dir

			# Then the flat biome ids wipe through to the textured render they stand for, and the
			# parallax those textures carry fades in behind it -- the last thing the diagram hands
			# over to the plugin's own output before the bridge cuts to it outright.
			if t >= float(chapter["texture_time"]):
				view_a = int(chapter["texture_view_a"])
				view_b = int(chapter["texture_view_b"])
				biome_count_a = times.size()
				biome_count_b = times.size()
				wipe = _ease_in_out(_ramp(t, float(chapter["texture_time"]),
						float(chapter["texture_duration"])))
				wipe_dir = _to_vector2(chapter["texture_wipe_dir"])
				# The wireframe goes with the diagram it belongs to: once the render it was
				# describing is on screen, the lines have nothing left to say.
				wire_opacity *= 1.0 - _ease_in_out(_ramp(t, float(chapter["texture_time"]),
						float(chapter["texture_duration"])))
			parallax = _ease_out(_ramp(t, float(chapter["parallax_time"]),
					float(chapter["parallax_duration"])))
			# One last flash carries the cut into the bridge.
			var bridge := float(timeline["bridge"])
			var flash_duration := float(effects["bridge_flash_duration"])
			var flash := _ramp(t, bridge - flash_duration, flash_duration)
			wire_intensity += float(effects["bridge_flash_wire"]) * flash * flash
			fill += float(effects["bridge_flash_fill"]) * flash * flash

	_apply_geometry(levels, resolution)

	var config_values := {
		"height_scale": height_scale,
		"noise_octaves": octaves,
		"noise_ridge_amount": float(_full["noise_ridge_amount"]) * float(shaping["ridge"]),
		"noise_warp_amount": float(_full["noise_warp_amount"]) * float(shaping["warp"]),
		"noise_continent_influence": float(_full["noise_continent_influence"]) * float(shaping["continent"]),
		"noise_continent_elevation": float(_full["noise_continent_elevation"]) * float(shaping["continent"]),
		# Both of these are 1.0 when absent rather than 0.0: 1.0 is the neutral value the shader
		# multiplies or exponentiates by, so the ramp runs from there to the config's own value.
		"noise_relief_floor": lerpf(1.0, float(_full["noise_relief_floor"]), float(shaping["continent"])),
		"noise_redistribution": lerpf(1.0, float(_full["noise_redistribution"]), float(shaping["redistribution"])),
		# Parallax is switched off by collapsing its fade window to nothing rather than by a
		# separate flag: pom_fade_end at zero puts every fragment past the fade, so the raymarch is
		# skipped outright, and the ramp opens the window back up to the config's own distances.
		"pom_fade_start": float(_full["pom_fade_start"]) * parallax,
		"pom_fade_end": float(_full["pom_fade_end"]) * parallax,
	}
	_apply_config(config_values)

	_apply_overlay({
		"debug_view": view_a,
		"debug_view_b": view_b,
		"debug_wipe": wipe,
		"debug_wipe_dir": wipe_dir,
		"debug_wipe_start": wipe_start,
		"debug_wipe_end": wipe_end,
		"debug_wipe_glow": wipe_width,
		"debug_fill": fill * float(look["fill_gain"]),
		"debug_center": _center,
		"debug_reveal_radius": reveal_radius,
		"debug_reveal_edge": float(look["reveal_edge"]),
		"debug_ripple_radius": ripple_radius,
		"debug_ripple_strength": ripple_strength,
		"debug_ripple_width": float(effects["ripple_width"]),
		"debug_ripple_lift": float(effects["ripple_lift"]),
		"debug_wire_opacity": wire_opacity,
		"debug_wire_color": _to_linear_vector3(look["wire_color"]),
		"debug_wire_intensity": wire_intensity * (1.0 + float(effects["pulse_gain"]) * pulse),
		"debug_wire_width": float(look["wire_width"]),
		"debug_wire_diagonal": float(look["wire_diagonal"]),
		"debug_wire_min_spacing": float(look["wire_min_spacing"]),
		"debug_wire_major": int(look["wire_major"]),
		"debug_vignette": float(look["vignette"]),
		"debug_biome_count": biome_count_a,
		"debug_biome_count_b": biome_count_b,
	})


# --- The noise chapter's build cycles -------------------------------------------------------

# How far the generator has got at time t: the octave count, how far each shaping term has arrived,
# and which patch the field is being built on. Every chapter reads this, not just the noise one --
# before the chapter's own cut nothing is shaped, after its last beat everything is, so a later
# chapter simply gets the demo configuration's own heightmap out of it.
#
# The noise chapter is every beat between its own cut and the next, and one cycle of the generator
# walks them a step at a time: an octave of fBm per beat until the sum is complete, then one
# NOISE_SHAPING_STEPS term per beat until the field is exactly what the plugin renders. The next
# beat re-seeds it and the same build starts over on a different field, so the chapter shows the
# generation process itself, several times over, rather than one pass of it.
#
# Only whole cycles run. Any beats left over at the end hold the finished field, which is the frame
# the relief chapter lifts -- and the last cycle is always the authored patch, so what stands up
# into relief is the field that was just finished rather than a stranger.
func _noise_build_at(p_chapter: Dictionary, t: float) -> Dictionary:
	var full_octaves := maxi(int(_full["noise_octaves"]), 1)
	var full_shaping := NOISE_SHAPING_STEPS.size()
	var times := _step_times(p_chapter) if not p_chapter.is_empty() else PackedFloat32Array()
	# No chapter, or no cue file to step against: the field is simply the finished one.
	if times.is_empty():
		return _noise_state(full_octaves, full_shaping, 1.0, Vector2.ZERO)
	# Before the chapter's own cut: bare lattice, nothing built, patch unmoved.
	if t < times[0]:
		return _noise_state(1, 0, 0.0, Vector2.ZERO)

	var per_cycle := full_octaves + full_shaping
	var cycles := maxi(times.size() / per_cycle, 1)
	var steps := mini(cycles * per_cycle, times.size())

	var reached := 0
	for i in steps:
		if t >= times[i]:
			reached = i + 1
	var index := clampi(reached - 1, 0, steps - 1)
	var step := index % per_cycle

	return _noise_state(mini(step + 1, full_octaves), maxi(step + 1 - full_octaves, 0),
			_ease_out(_ramp(t, times[index], float(p_chapter.get("shaping_duration", 0.0)))),
			_seed_offset(p_chapter, index / per_cycle, cycles))


# One moment of the build, as `_apply_build()` wants it. p_shaped is how many of
# NOISE_SHAPING_STEPS are in, the last of them at p_arriving: a term ramps up over its own beat
# rather than snapping on, the way the relief chapter's parameters used to arrive, while the octave
# count (an integer the shader loops on) can only step.
func _noise_state(p_octaves: int, p_shaped: int, p_arriving: float, p_seed: Vector2) -> Dictionary:
	var shaping := {}
	for i in NOISE_SHAPING_STEPS.size():
		var arrived := 0.0
		if i < p_shaped - 1:
			arrived = 1.0
		elif i == p_shaped - 1:
			arrived = p_arriving
		shaping[NOISE_SHAPING_STEPS[i]] = arrived
	return {"octaves": p_octaves, "shaping": shaping, "seed": p_seed}


# The moments a chapter's build actually steps on: its beats, each interval then split evenly into
# `step_subdivisions`. A detected onset is as fine as audio_cues.json goes (its own picker refuses
# to mark two hits closer than 0.25s), and the track's fastest rhythm runs several times denser than
# that, so stepping on the beats alone is far slower than the music. Subdividing keeps every step
# phase-locked to a real hit -- each beat is still a step, the extra ones sit evenly between them --
# and follows the music's own density, because a tighter run of beats subdivides tighter.
func _step_times(p_chapter: Dictionary) -> PackedFloat32Array:
	var beats := _beat_times(p_chapter)
	var splits := maxi(int(p_chapter.get("step_subdivisions", 1)), 1)
	if splits == 1 or beats.is_empty():
		return beats

	# The last beat has no successor to divide against, so it borrows the chapter's own end.
	var end := _chapter_end(p_chapter)
	var times := PackedFloat32Array()
	for i in beats.size():
		var from := beats[i]
		var to := beats[i + 1] if i + 1 < beats.size() else end
		for k in splits:
			times.append(from + (to - from) * float(k) / float(splits))
	return times


# The beats a chapter steps on: the detected cues inside its own window, thinned to a minimum
# spacing the way extract_cues.py picks its strong set out of the same list. Every step then lands
# on a real hit, and a dense run of onsets steps the field once rather than strobing it.
#
# Derived per frame rather than cached. It is a scan of a 178-entry array, and keeping it a pure
# function of the timeline is what lets --controls drag `step_min_gap` and see the new pacing on
# the paused frame immediately.
func _beat_times(p_chapter: Dictionary) -> PackedFloat32Array:
	var start := float(p_chapter["start"])
	var end := _chapter_end(p_chapter)
	var min_gap := float(p_chapter.get("step_min_gap", 0.0))
	var times := PackedFloat32Array()
	var last := -INF
	for time in _cue_times:
		if time < start - 0.001:
			continue
		if time >= end:
			break
		if time - last < min_gap:
			continue
		times.append(time)
		last = time
	return times


# The patch a cycle is built on, as an offset from the authored one. The last cycle is always the
# authored patch itself (offset zero): the relief chapter cuts straight out of it, and it is the
# patch every later chapter -- and the tool that picked it for biome balance -- is measured against.
#
# The earlier cycles walk a sunflower spiral out of it -- golden angle, radius growing as the square
# root of the cycle number, so neighbouring patches sit about `seed_distance` apart however many
# there turn out to be. Generated rather than authored as a list because how many cycles a chapter
# runs changes with every edit to the step rate, and a list would either run out or start repeating
# patches the moment it did.
func _seed_offset(p_chapter: Dictionary, p_cycle: int, p_cycles: int) -> Vector2:
	var distance := float(p_chapter.get("seed_distance", 0.0))
	if p_cycle >= p_cycles - 1 or distance <= 0.0:
		return Vector2.ZERO
	var n := float(p_cycle + 1)
	return Vector2(cos(GOLDEN_ANGLE * n), sin(GOLDEN_ANGLE * n)) * distance * sqrt(n)


# Re-seeds the field by relocating the patch. The noise has no seed -- it is one infinite
# world-space function, and another heightmap out of it is another place in it -- so a new field is
# the focus the clipmap centres on and the camera that orbits it moving together by the same
# offset, which leaves the framing and the geometry where they were and changes only what the
# height function returns underneath them.
#
# The offset is snapped to the coarsest clipmap level's own snapping step, which every finer
# level's divides: without that, the levels re-snap by up to a cell each and the mesh shifts under
# a camera that moved by the exact offset, which reads as a jolt rather than as a new field.
func _apply_seed(p_offset: Vector2, p_resolution: int) -> void:
	var step := 4.0 * _extent / float(maxi(p_resolution, 1))
	_center = _patch_center + Vector2(roundf(p_offset.x / step), roundf(p_offset.y / step)) * step
	_focus.global_position = Vector3(_center.x, _focus.global_position.y, _center.y)


# The half-width of clipmap level p_level, from the plugin's own numbers rather than authored.
func _level_extent(p_level: int) -> float:
	return _config.terrain_size * pow(2.0, float(p_level)) * 0.5


# Every ripple still bright enough to matter, most recent first, one per past time in p_times: age
# grows the radius, decay fades the strength. Each is independent -- a new ripple does not replace
# an older one still fading, it just occupies the next slot -- capped at DEBUG_RIPPLE_MAX (the
# shader's array size) and cut off once a hit is too faint to be worth a slot.
const DEBUG_RIPPLE_MAX := 4
const DEBUG_RIPPLE_CUTOFF := 0.01

func _ripple_off_array(p_value: float) -> PackedFloat32Array:
	var values := PackedFloat32Array()
	values.resize(DEBUG_RIPPLE_MAX)
	values.fill(p_value)
	return values


# p_hits are (time, distance, duration) triples: distance is what the ripple has to cover, duration
# is how long it has to cover it in -- normally the same duration the reveal front takes to grow to
# that distance, so the wave lands on the new edge exactly as that ring's own growth animation
# stops, at any level. A short first hop reads as a slow wave because it is a short distance over a
# long duration, and each doubling level travels twice as far in the same duration rather than
# lagging behind it. Age still drives strength's decay on its own clock, so a ring keeps glowing a
# little after it lands rather than vanishing the instant it arrives.
func _ripples_at(t: float, p_hits: Array, p_decay: float) -> Array:
	var ripples: Array = []
	for i in range(p_hits.size() - 1, -1, -1):
		var hit: Vector3 = p_hits[i]
		var age := t - hit.x
		if age < 0.0:
			continue
		var strength := exp(-age / p_decay)
		if strength < DEBUG_RIPPLE_CUTOFF:
			break
		var speed := hit.y / maxf(hit.z, 0.001)
		ripples.append(Vector2(age * speed, strength))
		if ripples.size() >= DEBUG_RIPPLE_MAX:
			break
	return ripples


# mesh_resolution steps up through the relief chapter, so the surface visibly gains the detail the
# noise already has -- another real plugin parameter rather than an effect standing in for one.
func _step_resolution(p_chapter: Dictionary, t: float, p_default: int) -> int:
	if not p_chapter.has("resolution_times"):
		return p_default
	var steps: Array = p_chapter["resolution_times"]
	var start := int(p_chapter["resolution_start"])
	var reached := _step_count(t, steps, 0)
	var resolution := start * int(pow(2.0, float(reached)))
	return mini(resolution, p_default)


# The framing a chapter asks for at time t, before any blend with its predecessor: the authored
# camera, the cut's settle-in, and the slow drift that keeps a locked-off shot from reading as a
# freeze. All four rates are per second and default to zero, so a chapter that sets none is exactly
# as static as it was.
func _camera_state(p_chapter: Dictionary, t: float) -> Dictionary:
	var effects: Dictionary = timeline["effects"]
	var camera: Dictionary = p_chapter["camera"]
	var elapsed := maxf(t - float(p_chapter["start"]), 0.0)

	var settle := _ease_out(_ramp(t, float(p_chapter["start"]), float(effects["cut_settle_duration"])))
	return {
		"yaw": float(camera["yaw"]) + float(camera.get("yaw_rate", 0.0)) * elapsed,
		"pitch": float(camera["pitch"]) + float(camera.get("pitch_rate", 0.0)) * elapsed,
		"distance": (float(camera["distance"]) + float(camera.get("distance_rate", 0.0)) * elapsed)
				* (1.0 - float(effects["cut_settle"]) * settle),
		"fov": float(camera["fov"]),
		"height": float(camera["height"]) + float(camera.get("height_rate", 0.0)) * elapsed,
	}


func _apply_camera(p_chapter: Dictionary, t: float, p_pulse: float) -> void:
	var state := _camera_state(p_chapter, t)

	# A chapter can ease out of the previous chapter's framing instead of cutting to its own.
	# blend_in is 0 everywhere by default -- the storyboard is built on hard cuts -- and exists for
	# the beats where a cut turns out to be too abrupt once there is music under it.
	var blend_in := float((p_chapter["camera"] as Dictionary).get("blend_in", 0.0))
	var index := get_chapter_index(t)
	if blend_in > 0.0 and index > 0:
		var blend := _ease_in_out(_ramp(t, float(p_chapter["start"]), blend_in))
		# The predecessor keeps drifting past its own cut, so the blend starts from where that shot
		# would have been now, not from where it was abandoned.
		var previous := _camera_state(timeline["chapters"][index - 1], t)
		for key in state:
			state[key] = lerpf(float(previous[key]), float(state[key]), blend)

	_set_camera_from_state(state, p_pulse)


# Turns a {yaw, pitch, distance, height, fov} state (in the same shape _camera_state returns) into
# the camera's actual transform, orbiting the patch centre. Shared by the build-up chapters and the
# cinematic reveal so both move the camera the same way.
func _set_camera_from_state(p_state: Dictionary, p_pulse: float) -> void:
	var effects: Dictionary = timeline["effects"]
	var yaw := deg_to_rad(float(p_state["yaw"]))
	var pitch := deg_to_rad(float(p_state["pitch"]))
	var distance := float(p_state["distance"])

	var target := Vector3(_center.x, float(p_state["height"]), _center.y)
	var offset := Vector3(sin(yaw) * cos(pitch), -sin(pitch), cos(yaw) * cos(pitch)) * distance

	_camera.global_position = target + offset
	_camera.look_at(target, Vector3.UP)
	_camera.fov = float(p_state["fov"]) * (1.0 - float(effects["fov_punch"]) * p_pulse)
	_camera.far = maxf(_extent * 6.0, distance * 3.0)


# The demo scene's own key light, swung slowly around its authored direction so relief keeps
# reading on an otherwise motionless frame. An offset on the scene's basis rather than a direction
# of our own: retuning the light in the demo scene retunes the trailer with it.
func _apply_light(t: float) -> void:
	var effects: Dictionary = timeline["effects"]
	var angle := deg_to_rad(float(effects["light_yaw_offset"]) +
			maxf(t - float(effects["light_start"]), 0.0) * float(effects["light_yaw_rate"]))
	_light.global_transform = Transform3D(Basis(Vector3.UP, angle) * _light_basis,
			_light.global_transform.origin)


# --- Cinematic shot -------------------------------------------------------------------------

# The bridge cut holds the build-up's last framing for continuity -- the terrain the diagram was
# describing is suddenly the same thing in the demo scene's own light and sky -- then eases into
# "reveal", a close, low orbit sized to sit inside the clipmap's innermost rings (the same LOD0/LOD1
# distance the demo scene's own camera sits at) rather than the diagram's aerial, which puts most of
# the frame several clipmap levels out. yaw keeps drifting afterwards so the shot is a slow reveal,
# not a freeze.
func _apply_cinematic(t: float) -> void:
	var cinematic: Dictionary = timeline["cinematic"]
	var bridge := float(timeline["bridge"])

	# Full plugin render: the demo scene's configuration, untouched, and no overlay at all, on the
	# authored patch -- the noise chapter's re-seeding is the build shot's own business.
	_apply_seed(Vector2.ZERO, int(_full["mesh_resolution"]))
	_apply_geometry(int(_full["clipmap_levels"]), int(_full["mesh_resolution"]))
	var config_values := {}
	for property in RAMPED_PROPERTIES:
		config_values[property] = _full[property]
	_apply_config(config_values)
	_apply_overlay({
		"debug_view": VIEW_SHADED,
		"debug_view_b": VIEW_SHADED,
		"debug_wipe": 0.0,
		"debug_fill": 1.0,
		"debug_reveal_radius": -1.0,
		"debug_ripple_radius": _ripple_off_array(-1.0),
		"debug_ripple_strength": _ripple_off_array(0.0),
		"debug_wire_opacity": 0.0,
		"debug_vignette": 0.0,
		"debug_biome_count": -1,
		"debug_biome_count_b": -1,
	})
	_apply_light(t)

	var start_state := _camera_state(_chapter_named("biomes"), bridge)

	var reveal: Dictionary = cinematic["reveal"]
	var elapsed := maxf(t - bridge, 0.0)
	var reveal_state := {
		"yaw": float(reveal["yaw"]) + float(reveal.get("yaw_rate", 0.0)) * elapsed,
		"pitch": float(reveal["pitch"]),
		"distance": float(reveal["distance"]),
		"height": float(reveal["height"]),
		"fov": float(reveal.get("fov", start_state["fov"])),
	}

	var blend := _ease_in_out(_ramp(t, bridge, float(cinematic["ease_duration"])))
	var state := {}
	for key in start_state:
		state[key] = lerpf(float(start_state[key]), float(reveal_state[key]), blend)

	_set_camera_from_state(state, 0.0)


# --- Cue envelopes --------------------------------------------------------------------------

# A short exponential decay off the most recent cue, driving wire brightness and the FOV punch:
# the diagram visibly reacts on every hit instead of only on the ones that change a parameter.
func _pulse_at(t: float) -> float:
	var effects: Dictionary = timeline["effects"]
	var decay := float(effects["pulse_decay"])
	# 0 keeps the original two-level response (every strong cue punches alike); 1 scales each hit by
	# how hard the detector actually heard it, so a run of onsets breathes instead of strobing flat.
	var weight := clampf(float(effects.get("pulse_strength_weight", 0.0)), 0.0, 1.0)
	var pulse := 0.0
	for i in _cue_times.size():
		var age := t - _cue_times[i]
		if age < 0.0:
			break
		var amplitude := _cue_amplitudes[i]
		if weight > 0.0 and i < _cue_strengths.size():
			amplitude = lerpf(amplitude, amplitude * _cue_strengths[i], weight)
		pulse = maxf(pulse, amplitude * exp(-age / decay))
	return clampf(pulse, 0.0, 1.0)


# --- Helpers --------------------------------------------------------------------------------

func _chapter_named(p_name: String) -> Dictionary:
	for chapter in timeline["chapters"]:
		if chapter["name"] == p_name:
			return chapter
	return {}


# Where a chapter's window closes: the next chapter's cut, or the end of the shot for the last one.
func _chapter_end(p_chapter: Dictionary) -> float:
	var chapters: Array = timeline["chapters"]
	for i in chapters.size():
		if chapters[i]["name"] != p_chapter["name"]:
			continue
		return float(chapters[i + 1]["start"]) if i + 1 < chapters.size() else _shot_end
	return _shot_end


# How many of p_times have passed at t, floored at p_minimum. Used wherever a beat list steps a
# discrete count -- octaves, clipmap levels, mesh resolution, biome layers.
func _step_count(t: float, p_times: Array, p_minimum: int) -> int:
	var count := 0
	for time in p_times:
		if t >= float(time):
			count += 1
	return maxi(count, p_minimum)


func _ramp(t: float, p_start: float, p_duration: float) -> float:
	return clampf((t - p_start) / maxf(p_duration, 0.0001), 0.0, 1.0)


func _ease_out(x: float) -> float:
	return 1.0 - pow(1.0 - x, 3.0)


func _ease_in_out(x: float) -> float:
	return x * x * (3.0 - 2.0 * x)


# Overshoots past 1 and settles back, so the terrain springs up rather than sliding up.
func _back_out(x: float) -> float:
	const C1 := 1.70158
	const C3 := C1 + 1.0
	return 1.0 + C3 * pow(x - 1.0, 3.0) + C1 * pow(x - 1.0, 2.0)


func _to_vector2(p_value: Variant) -> Vector2:
	var a: Array = p_value
	return Vector2(float(a[0]), float(a[1]))


func _to_vector3(p_value: Variant) -> Vector3:
	var a: Array = p_value
	return Vector3(float(a[0]), float(a[1]), float(a[2]))


func _to_srgb_color(p_value: Variant) -> Color:
	var a: Array = p_value
	return Color(float(a[0]), float(a[1]), float(a[2]))


# Colours are authored in sRGB, the way a colour picker shows them, but a shader uniform set
# through RenderingServer is taken as-is -- so the conversion has to happen here.
func _to_linear_vector3(p_value: Variant) -> Vector3:
	var color := _to_srgb_color(p_value).srgb_to_linear()
	return Vector3(color.r, color.g, color.b)


func _parse_args() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--shot="):
			_shot = arg.substr("--shot=".length())
		elif arg.begins_with("--start="):
			_preview_start = arg.substr("--start=".length()).to_float()
		elif arg.begins_with("--frames="):
			_preview_frames = arg.substr("--frames=".length()).to_int()
		elif arg == "--controls":
			_controls_requested = true
		elif arg == "--no-music":
			_music_requested = false


func _load_timeline() -> bool:
	var file := FileAccess.open(TIMELINE_PATH, FileAccess.READ)
	if file == null:
		_abort("could not open %s" % TIMELINE_PATH)
		return false

	var parsed: Variant = JSON.parse_string(file.get_as_text())
	if typeof(parsed) != TYPE_DICTIONARY:
		_abort("could not parse %s" % TIMELINE_PATH)
		return false

	timeline = parsed
	for key in ["shots", "patch", "look", "effects", "chapters", "cinematic", "bridge"]:
		if not timeline.has(key):
			_abort("%s is missing the '%s' section" % [TIMELINE_PATH, key])
			return false
	return true


func _load_cues() -> void:
	var file := FileAccess.open(CUES_PATH, FileAccess.READ)
	if file == null:
		printerr("[trailer] could not open %s" % CUES_PATH)
		return

	var parsed: Variant = JSON.parse_string(file.get_as_text())
	if typeof(parsed) != TYPE_DICTIONARY or not parsed.has("cues"):
		printerr("[trailer] could not parse %s" % CUES_PATH)
		return

	var onset_amplitude := float(timeline["effects"]["pulse_onset_amplitude"])
	var raw_strengths := PackedFloat32Array()
	for cue in parsed["cues"]:
		var time := float(cue["time_sec"])
		var kind := str(cue["kind"])
		_cue_times.append(time)
		_cue_amplitudes.append(onset_amplitude if kind == "onset" else 1.0)
		_cue_kinds.append(KIND_ONSET if kind == "onset" else (KIND_BRIDGE if kind == "bridge" else KIND_STRONG))
		raw_strengths.append(float(cue.get("strength", 1.0)))
		if kind != "onset":
			_strong_times.append(time)

	# The detector's strengths are an arbitrary scale whose floor sits well above zero (this track
	# runs about 2.4 to 5.3), so they are stretched across the observed range rather than used raw:
	# otherwise "weighting by strength" would barely separate the softest hit from the hardest.
	var lowest := INF
	var highest := -INF
	for strength in raw_strengths:
		lowest = minf(lowest, strength)
		highest = maxf(highest, strength)
	var span := maxf(highest - lowest, 0.0001)
	for strength in raw_strengths:
		_cue_strengths.append(clampf((strength - lowest) / span, 0.0, 1.0))


# The waveform the editor's ruler draws. Optional: a missing file just means the ruler shows cue
# ticks over an empty lane, so a checkout without it still tunes.
func _load_peaks() -> void:
	var file := FileAccess.open(PEAKS_PATH, FileAccess.READ)
	if file == null:
		return

	var parsed: Variant = JSON.parse_string(file.get_as_text())
	if typeof(parsed) != TYPE_DICTIONARY or not parsed.has("peak"):
		printerr("[trailer] could not parse %s" % PEAKS_PATH)
		return

	for value in parsed["peak"]:
		_peaks.append(float(value))
	for value in parsed.get("rms", []):
		_peaks_rms.append(float(value))
	_peaks_fps = float(parsed.get("fps", FPS))


func _abort(message: String) -> void:
	_finished = true
	printerr("[trailer] %s" % message)
	get_tree().quit(1)
