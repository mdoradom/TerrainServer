extends Node3D

# Shared scaffold for the showreel trailer clips (see TODO_TRAILER.md).
#
# The "build" shot is a static-camera diagram sequence: a wireframe diorama of the real terrain
# floating in black, assembling itself to the track's beats -- grid, then noise, then displacement,
# then each noise parameter, then normals, climate and biomes -- and the "cinematic" shot is the
# photorealistic payoff on the other side of the bridge cue.
#
# Everything on screen is a pure function of the shot time: no state accumulates between frames, so
# any moment can be previewed on its own with --start=<sec> --frames=<n> without rendering what
# comes before it, and --write-movie renders the identical result on any machine.
#
# This script holds no authored numbers. Every time, camera, colour and effect value lives in
# trailer_timeline.json, which `--controls` lets you edit live and save back. What stays here is
# the choreography itself: which parameter a chapter ramps, and in what order.

const CONFIG_PATH := "res://addons/terrain_server/assets/terrains/demo_terrain_configuration.tres"
const CUES_PATH := "res://trailer/audio_cues.json"
const PEAKS_PATH := "res://trailer/audio_peaks.json"
const TIMELINE_PATH := "res://trailer/trailer_timeline.json"
const FPS := 60.0

# Cue kinds as the editor's ruler wants them, flattened out of audio_cues.json's string field.
const KIND_ONSET := 0
const KIND_STRONG := 1
const KIND_BRIDGE := 2

# Visual layers: the diorama draws on layer 2, the real Terrain3D on layer 1, and the camera's cull
# mask picks one. Terrain3D owns its RenderingServer instances directly and has no `visible`, so a
# cull mask is the only way to cut between the two without tearing the terrain down and back up.
const LAYER_TERRAIN := 1
const LAYER_DIORAMA := 2

var timeline := {}

var _shot := "build"
var _preview_start := 0.0
var _preview_frames := 0
var _controls_requested := false
var _controls_active := false

var _config: TerrainConfiguration
var _cue_times := PackedFloat32Array()
var _cue_amplitudes := PackedFloat32Array()
var _cue_kinds := PackedByteArray()
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

var _full_noise := {}

@onready var _terrain: Terrain3D = $Terrain3D
@onready var _camera: Camera3D = $Camera3D
@onready var _world_environment: WorldEnvironment = $WorldEnvironment
@onready var _diorama: Node3D = $Diorama
@onready var _whittaker: Control = $Overlay/Whittaker
@onready var _controls: CanvasLayer = $Controls


func _ready() -> void:
	_parse_args()

	if not _load_timeline():
		return

	var shots: Dictionary = timeline["shots"]
	if not shots.has(_shot):
		_abort("unknown shot '%s'; known shots are %s" % [_shot, ", ".join(shots.keys())])
		return

	_config = load(CONFIG_PATH)
	if _config == null:
		_abort("could not load %s" % CONFIG_PATH)
		return

	# The config's own values are the targets every parameter ramp animates towards.
	_full_noise = {
		"octaves": _config.noise_octaves,
		"ridge_amount": _config.noise_ridge_amount,
		"warp_amount": _config.noise_warp_amount,
		"continent_influence": _config.noise_continent_influence,
		"continent_elevation": _config.noise_continent_elevation,
		"relief_floor": _config.noise_relief_floor,
		"redistribution": _config.noise_redistribution,
	}

	_terrain.configuration = _config
	_load_cues()
	_load_peaks()

	var window: Array = shots[_shot]
	_shot_start = window[0]
	_shot_end = window[1]
	_shot_duration_frames = int(round((_shot_end - _shot_start) * FPS))
	if _preview_frames > 0:
		_shot_duration_frames = _preview_frames

	var diorama_cfg: Dictionary = timeline["diorama"]
	var center := _to_vector3(diorama_cfg["center"])
	_diorama.global_position = center
	if not _diorama.setup(_config, float(diorama_cfg["size"]), int(diorama_cfg["resolution"]),
			int(diorama_cfg["ring_levels"])):
		_abort("diorama setup failed")
		return

	_whittaker.setup(_config, _terrain, _diorama.biome_colors(), _diorama.biome_names(),
			Vector2(center.x, center.z), float(diorama_cfg["size"]) * 0.5)
	_setup_shot()
	_setup_controls()

	_time = _shot_start + _preview_start
	_apply_time(_time)

	print("[trailer] shot=%s window=%.3fs-%.3fs start=%.3fs frames=%d viewport=%s controls=%s" % [
			_shot, _shot_start, _shot_end, _time, _shot_duration_frames,
			str(get_viewport().size), str(_controls_active)])


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


# Throws away every unsaved edit by re-reading the file the panel writes. The diorama's own
# uniforms are all re-pushed from the timeline every frame, so re-applying the current moment is
# all it takes for the revert to show.
func reload_timeline() -> bool:
	if not _load_timeline():
		return false
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

# One-time per-shot look. The scene's Environment is a local sub-resource, so mutating it here only
# affects the running process, never the saved scene.
func _setup_shot() -> void:
	if _shot == "build":
		_camera.cull_mask = LAYER_DIORAMA
		_whittaker.visible = true
	else:
		_camera.cull_mask = LAYER_TERRAIN
		_diorama.visible = false
		_whittaker.visible = false
	_setup_environment()


func _setup_environment() -> void:
	var environment := _world_environment.environment
	if _shot != "build":
		environment.sdfgi_enabled = true
		return

	var look: Dictionary = timeline["look"]
	environment.background_mode = Environment.BG_COLOR
	environment.background_color = Color.BLACK
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.ambient_light_color = Color.BLACK
	environment.fog_enabled = false
	environment.ssao_enabled = false
	# Linear tonemapping keeps the diagram's flat colours exactly as authored, and lets the
	# wireframe's above-1.0 emission blow out to white-hot cores inside the glow.
	environment.tonemap_mode = Environment.TONE_MAPPER_LINEAR
	environment.glow_enabled = true
	environment.glow_intensity = float(look["glow_intensity"])
	environment.glow_strength = float(look["glow_strength"])
	environment.glow_bloom = float(look["glow_bloom"])
	environment.glow_hdr_threshold = float(look["glow_hdr_threshold"])
	environment.set("glow_levels/4", 1.0)
	environment.set("glow_levels/5", 0.5)


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
	_controls.setup(self)


func _apply_time(t: float) -> void:
	if _shot == "build":
		_apply_build(t)
	else:
		_apply_cinematic(t)


# --- Build shot -----------------------------------------------------------------------------

func _apply_build(t: float) -> void:
	var look: Dictionary = timeline["look"]
	var effects: Dictionary = timeline["effects"]
	var chapter: Dictionary = timeline["chapters"][get_chapter_index(t)]
	var size := float(timeline["diorama"]["size"])
	var shade_amount := float(look["shade_amount"])

	var pulse := _pulse_at(t)
	_apply_camera(chapter, t, pulse)

	_diorama.set_param("d_pulse", pulse)
	var ripple := _ripple_at(t)
	_diorama.set_param("d_ripple_radius", ripple.x)
	_diorama.set_param("d_ripple_strength", ripple.y)

	# Defaults every chapter starts from, then overrides below. Keeping them here (rather than
	# letting values persist) is what makes any single frame renderable on its own.
	var reveal_radius := size * 0.5
	var rings_visible := false
	var octaves: int = _full_noise["octaves"]
	var height_mix := 1.0
	var skirt_mix := 1.0
	var view_a := int(chapter.get("view_a", 6))
	var view_b := int(chapter.get("view_b", 6))
	var sweep := 0.0
	var sweep_dir := _to_vector2(chapter.get("sweep_dir", [1.0, 0.0]))
	var fill_intensity := float(chapter.get("fill_intensity", 1.0))
	var shade := float(chapter.get("shade", shade_amount))
	var wire_intensity := float(chapter.get("wire_intensity", look["wire_intensity"]))
	var noise_mix := 1.0
	var biome_reveals := PackedFloat32Array()
	var slope_reveal := 0.0
	var whittaker_fade := 0.0

	match chapter["name"]:
		"grid":
			# The grid draws outward from a single point at the first hit, then each clipmap ring
			# snaps in around it -- the clipmap's own structure, before there is any terrain.
			reveal_radius = _ease_out(_ramp(t, float(chapter["first_hit"]),
					float(chapter["level0_duration"]))) * size * 0.5
			var ring_times: Array = chapter["ring_times"]
			for i in ring_times.size():
				var ring_radius := size * 0.5 * pow(2.0, float(i + 1))
				reveal_radius = maxf(reveal_radius, _ease_out(_ramp(t, float(ring_times[i]),
						float(chapter["ring_duration"]))) * ring_radius)
			rings_visible = true
			height_mix = 0.0
			skirt_mix = 0.0
			octaves = int(chapter["octaves"])

		"noise":
			# Flat still: this beat is about the noise field itself, not the relief, so the fill
			# leads and the grid steps back.
			height_mix = 0.0
			skirt_mix = 0.0
			octaves = _step_count(t, chapter["octave_times"])
			sweep = _ease_in_out(_ramp(t, float(chapter["start"]), float(chapter["sweep_duration"])))
			noise_mix = 0.0

		"relief":
			# The texture stands up into terrain, then the shaping parameters arrive one per hit.
			height_mix = _back_out(_ramp(t, float(chapter["start"]), float(chapter["lift_duration"])))
			skirt_mix = _ease_out(_ramp(t, float(chapter["skirt_start"]), float(chapter["skirt_duration"])))
			shade = shade_amount * _ramp(t, float(chapter["start"]) + float(chapter["shade_delay"]),
					float(chapter["shade_duration"]))

		"normals":
			sweep = _ease_in_out(_ramp(t, float(chapter["start"]), float(chapter["sweep_duration"])))
			if t >= float(chapter["lit_time"]):
				view_a = int(chapter["lit_view_a"])
				view_b = int(chapter["lit_view_b"])
				sweep = _ease_in_out(_ramp(t, float(chapter["lit_time"]), float(chapter["lit_duration"])))
				sweep_dir = _to_vector2(chapter["lit_sweep_dir"])
				shade = shade_amount * float(chapter["lit_shade_scale"])

		"climate":
			sweep = _ease_in_out(_ramp(t, float(chapter["start"]), float(chapter["sweep_duration"])))
			if t >= float(chapter["moisture_time"]):
				view_a = int(chapter["moisture_view_a"])
				view_b = int(chapter["moisture_view_b"])
				sweep = _ease_in_out(_ramp(t, float(chapter["moisture_time"]),
						float(chapter["sweep_duration"])))
				sweep_dir = _to_vector2(chapter["moisture_sweep_dir"])
			# Into the track's silence the whole diagram dims down to almost nothing, so the biome
			# hit lands on a near-black frame.
			var breath := _ease_in_out(_ramp(t, float(chapter["breath_start"]),
					float(chapter["breath_duration"])))
			fill_intensity = lerpf(fill_intensity, float(chapter["fill_intensity_breath"]), breath)
			wire_intensity = lerpf(wire_intensity, float(look["wire_intensity_dim"]), breath)
			shade = lerpf(shade, 0.0, breath)

		"biomes":
			# Snap back out of the breath, overshooting bright on the hit itself.
			fill_intensity += float(chapter["fill_snap"]) * exp(
					-maxf(t - float(chapter["start"]), 0.0) / float(chapter["fill_snap_decay"]))
			for i in _diorama.biome_colors().size():
				biome_reveals.append(_ease_out(_ramp(t, _biome_reveal_time(chapter, i),
						float(chapter["reveal_duration"]))))
			slope_reveal = _ease_out(_ramp(t, float(chapter["slope_time"]), float(chapter["slope_duration"])))
			whittaker_fade = _ease_out(_ramp(t, float(chapter["start"]),
					float(chapter["whittaker_fade_duration"])))
			# One last flash carries the cut into the bridge.
			var bridge := float(timeline["bridge"])
			var flash_duration := float(effects["bridge_flash_duration"])
			var flash := _ramp(t, bridge - flash_duration, flash_duration)
			wire_intensity += float(effects["bridge_flash_wire"]) * flash * flash
			fill_intensity += float(effects["bridge_flash_fill"]) * flash * flash

	if biome_reveals.is_empty():
		for i in _diorama.biome_colors().size():
			biome_reveals.append(0.0)

	var wire_color := _to_color(look["wire_color"])

	# The ripple lifts the grid on the beat while it is still flat; once there is real relief the
	# same displacement only reads as a stray ring rolling across the mountains.
	_diorama.set_param("d_ripple_lift", float(effects["ripple_lift"]) * (1.0 - clampf(height_mix, 0.0, 1.0)))
	_diorama.set_param("d_ripple_width", float(effects["ripple_width"]))
	_diorama.set_param("d_pulse_gain", float(effects["pulse_gain"]))
	_diorama.set_param("d_reveal_radius", reveal_radius)
	_diorama.set_param("d_reveal_edge", float(look["reveal_edge"]))
	_diorama.set_param("d_height_mix", height_mix)
	_diorama.set_param("d_skirt_mix", skirt_mix)
	_diorama.set_param("d_skirt_depth", float(look["skirt_depth"]))
	_diorama.set_param("d_view_a", view_a)
	_diorama.set_param("d_view_b", view_b)
	_diorama.set_param("d_sweep", sweep)
	_diorama.set_param("d_sweep_dir", sweep_dir)
	_diorama.set_param("d_fill_intensity", fill_intensity * float(look["fill_gain"]))
	_diorama.set_param("d_shade_amount", shade)
	_diorama.set_param("d_wire_intensity", wire_intensity)
	_diorama.set_param("d_wire_width", float(look["wire_width"]))
	_diorama.set_param("d_wire_opacity", float(look["wire_opacity"]))
	_diorama.set_param("d_diagonal_intensity", float(look["diagonal_intensity"]))
	_diorama.set_param("d_wire_color", Vector3(wire_color.r, wire_color.g, wire_color.b))
	_diorama.set_param("d_biome_reveal", biome_reveals)
	_diorama.set_param("d_rock_reveal", slope_reveal)
	_diorama.set_param("d_light_dir", _light_direction(t))
	_diorama.set_param("octaves", octaves)

	_apply_noise_parameters(t, noise_mix)

	_diorama.set_ring_levels_visible(rings_visible)
	var ring_intensity: Array = look["ring_intensity"]
	for i in _diorama.level_count():
		_diorama.set_level_intensity(i, float(ring_intensity[i]) if i < ring_intensity.size() else 0.3)

	_whittaker.set_state(whittaker_fade, biome_reveals, slope_reveal)


# The shaping parameters ramp in one per hit during the relief chapter, and stay at the config's own
# values afterwards. p_mix scales the whole set to zero for chapters that predate the relief.
func _apply_noise_parameters(t: float, p_mix: float) -> void:
	var relief := _chapter_named("relief")
	if relief.is_empty():
		return

	var ridge: float = _full_noise["ridge_amount"] * _ease_out(_ramp(t,
			float(relief["ridge_time"]), float(relief["ridge_duration"])))
	var warp: float = _full_noise["warp_amount"] * _ease_out(_ramp(t,
			float(relief["warp_time"]), float(relief["warp_duration"])))
	var continent := _ease_out(_ramp(t, float(relief["continent_time"]), float(relief["continent_duration"])))
	var redistribution := lerpf(1.0, _full_noise["redistribution"], _ease_out(_ramp(t,
			float(relief["redistribution_time"]), float(relief["redistribution_duration"]))))

	_diorama.set_param("ridge_amount", ridge * p_mix)
	_diorama.set_param("warp_amount", warp * p_mix)
	_diorama.set_param("continent_influence", _full_noise["continent_influence"] * continent * p_mix)
	_diorama.set_param("continent_elevation", _full_noise["continent_elevation"] * continent * p_mix)
	_diorama.set_param("relief_floor", lerpf(1.0, _full_noise["relief_floor"], continent * p_mix))
	_diorama.set_param("redistribution", lerpf(1.0, redistribution, p_mix))


func _apply_camera(p_chapter: Dictionary, t: float, p_pulse: float) -> void:
	var effects: Dictionary = timeline["effects"]
	var camera: Dictionary = p_chapter["camera"]

	var settle := _ease_out(_ramp(t, float(p_chapter["start"]), float(effects["cut_settle_duration"])))
	var distance := float(camera["distance"]) * (1.0 - float(effects["cut_settle"]) * settle)
	var yaw := deg_to_rad(float(camera["yaw"]))
	var pitch := deg_to_rad(float(camera["pitch"]))

	var target := _to_vector3(timeline["diorama"]["center"]) + Vector3(0.0, float(camera["height"]), 0.0)
	var offset := Vector3(sin(yaw) * cos(pitch), -sin(pitch), cos(yaw) * cos(pitch)) * distance

	_camera.global_position = target + offset
	_camera.look_at(target, Vector3.UP)
	_camera.fov = float(camera["fov"]) * (1.0 - float(effects["fov_punch"]) * p_pulse)
	_camera.far = maxf(_camera.far, distance * 3.0)


# --- Cinematic shot -------------------------------------------------------------------------

# Placeholder until T6: holds the biome chapter's framing on the real, fully shaded terrain, so the
# bridge cut reads as the same world switching from diagram to render, then eases in.
func _apply_cinematic(t: float) -> void:
	var chapters: Array = timeline["chapters"]
	var chapter: Dictionary = (chapters[chapters.size() - 1] as Dictionary).duplicate(true)
	var cinematic: Dictionary = timeline["cinematic"]
	var bridge := float(timeline["bridge"])

	chapter["start"] = bridge
	var camera: Dictionary = chapter["camera"]
	camera["distance"] = lerpf(float(camera["distance"]),
			float(camera["distance"]) * float(cinematic["distance_factor"]),
			_ease_in_out(_ramp(t, bridge, float(cinematic["ease_duration"]))))
	_apply_camera(chapter, t, 0.0)


# --- Cue envelopes --------------------------------------------------------------------------

# A short exponential decay off the most recent cue, driving wire brightness and the FOV punch:
# the diagram visibly reacts on every hit instead of only on the ones that change a parameter.
func _pulse_at(t: float) -> float:
	var decay := float(timeline["effects"]["pulse_decay"])
	var pulse := 0.0
	for i in _cue_times.size():
		var age := t - _cue_times[i]
		if age < 0.0:
			break
		pulse = maxf(pulse, _cue_amplitudes[i] * exp(-age / decay))
	return clampf(pulse, 0.0, 1.0)


# A ring expanding out of the centre on each "strong" cue, lifting and lighting the wireframe as it
# passes. Returns (radius, strength).
func _ripple_at(t: float) -> Vector2:
	var effects: Dictionary = timeline["effects"]
	for i in range(_strong_times.size() - 1, -1, -1):
		var age := t - _strong_times[i]
		if age < 0.0:
			continue
		return Vector2(age * float(effects["ripple_speed"]), exp(-age / float(effects["ripple_decay"])))
	return Vector2(-1.0, 0.0)


# The key light swings slowly through the lit/climate/biome chapters, so relief keeps reading on an
# otherwise motionless frame.
func _light_direction(t: float) -> Vector3:
	var effects: Dictionary = timeline["effects"]
	var angle := deg_to_rad(float(effects["light_base_angle"])) + \
			maxf(t - float(effects["light_start"]), 0.0) * float(effects["light_rate"])
	return Vector3(sin(angle) * 0.75, 0.62, cos(angle) * 0.75)


# --- Helpers --------------------------------------------------------------------------------

func _chapter_named(p_name: String) -> Dictionary:
	for chapter in timeline["chapters"]:
		if chapter["name"] == p_name:
			return chapter
	return {}


func _biome_reveal_time(p_chapter: Dictionary, p_index: int) -> float:
	var times: Array = p_chapter["biome_reveal_times"]
	if p_index < times.size():
		return float(times[p_index])
	# An edited config with more biomes than the track has marked hits for still reveals them all.
	return float(times[times.size() - 1]) + float(p_index - times.size() + 1)


func _step_count(t: float, p_times: Array) -> int:
	var count := 0
	for time in p_times:
		if t >= float(time):
			count += 1
	return maxi(count, 1)


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


func _to_color(p_value: Variant) -> Color:
	var a: Array = p_value
	return Color(float(a[0]), float(a[1]), float(a[2]))


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
	for key in ["shots", "diorama", "look", "effects", "chapters", "bridge"]:
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
	for cue in parsed["cues"]:
		var time := float(cue["time_sec"])
		var kind := str(cue["kind"])
		_cue_times.append(time)
		_cue_amplitudes.append(onset_amplitude if kind == "onset" else 1.0)
		_cue_kinds.append(KIND_ONSET if kind == "onset" else (KIND_BRIDGE if kind == "bridge" else KIND_STRONG))
		if kind != "onset":
			_strong_times.append(time)


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
