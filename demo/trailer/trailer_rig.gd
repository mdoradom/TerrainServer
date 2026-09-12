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

const CONFIG_PATH := "res://addons/terrain_server/assets/terrains/demo_terrain_configuration.tres"
const CUES_PATH := "res://trailer/audio_cues.json"
const FPS := 60.0

# Both windows come from real audio_cues.json timestamps: the build-up runs from the track's first
# hit to the bridge, the cinematic from the bridge to the end (T6 picks the real trimmed endpoint).
const SHOT_RANGES := {
	"build": [0.0, 63.936],
	"cinematic": [63.936, 145.276],
}

# Not the origin: temperature and moisture are noise fields whose wavelengths are about one diorama
# wide, so most of the world sits inside a single climate band and renders as one biome. This patch
# was picked by demo/trailer/tools/pick_diorama_center.gd as the best-scoring candidate in a
# 12km search -- all four of the demo config's biomes in balance (rock 52%, grass 29%, snow 16%,
# dirt 3%) with 350 units of relief across it.
const DIORAMA_CENTER := Vector3(5632.0, 0.0, -4608.0)
const DIORAMA_SIZE := 1536.0
# Coarser than the config's real mesh_resolution on purpose: past roughly this density the cells
# fall below a few pixels and the wireframe stops reading as a grid and starts reading as fabric.
const DIORAMA_RESOLUTION := 96
const DIORAMA_RING_LEVELS := 2

# Recording size is not settable from here: --write-movie fixes it before the scene loads, from
# display/window/size/viewport_* in demo/project.godot (1920x1080). Neither --resolution nor a
# runtime DisplayServer resize moves it -- both change the viewport while the movie keeps coming
# out at the project's size.

# Visual layers: the diorama draws on layer 2, the real Terrain3D on layer 1, and the camera's cull
# mask picks one. Terrain3D owns its RenderingServer instances directly and has no `visible`, so a
# cull mask is the only way to cut between the two without tearing the terrain down and back up.
const LAYER_TERRAIN := 1
const LAYER_DIORAMA := 2

# --- Chapters -------------------------------------------------------------------------------
# One static camera each, cut on a "strong" cue. Pitch is the ~45-degree aerial the shot is built
# around; yaw/distance change per chapter so each cut lands on a visibly new angle.
const CHAPTERS: Array[Dictionary] = [
	# Beat 1: the clipmap grid draws itself out of nothing, level by level.
	{"name": "grid", "start": 0.0, "yaw": 35.0, "pitch": -44.0, "distance": 3400.0, "fov": 42.0, "height": 0.0},
	# Beat 2: noise appears on the flat grid, gaining an octave per hit.
	{"name": "noise", "start": 9.451, "yaw": 35.0, "pitch": -46.0, "distance": 1780.0, "fov": 42.0, "height": 0.0},
	# Beat 3: the noise texture lifts into relief, then each shaping parameter comes in.
	{"name": "relief", "start": 20.619, "yaw": -28.0, "pitch": -40.0, "distance": 1980.0, "fov": 44.0, "height": 130.0},
	# Beat 4: what the shader derives from that surface -- normals, then lighting.
	{"name": "normals", "start": 30.906, "yaw": 108.0, "pitch": -36.0, "distance": 1820.0, "fov": 46.0, "height": 150.0},
	# Beat 5: the climate fields the biomes are chosen from.
	{"name": "climate", "start": 42.388, "yaw": 18.0, "pitch": -54.0, "distance": 2150.0, "fov": 42.0, "height": 120.0},
	# Beat 6: biomes light up one by one, into the bridge.
	{"name": "biomes", "start": 50.085, "yaw": -62.0, "pitch": -42.0, "distance": 1980.0, "fov": 44.0, "height": 150.0},
]

# --- Cue-locked key times -------------------------------------------------------------------
# Every timestamp below is a cue from audio_cues.json, so the choreography lands on the track.
const GRID_FIRST_HIT := 0.035
const GRID_LEVEL0_DURATION := 4.6
const GRID_RING_TIMES := [6.316, 8.173]
const GRID_RING_DURATION := 0.5

const NOISE_START := 9.451
const NOISE_SWEEP_DURATION := 1.4
# One octave per hit: the pop is the point, it reads as detail snapping in on the beat.
const NOISE_OCTAVE_TIMES := [9.451, 10.600, 11.656, 13.758, 15.023, 17.310]

const RELIEF_LIFT := 20.619
const RELIEF_LIFT_DURATION := 1.6
const RELIEF_SKIRT := 21.2
const RELIEF_RIDGE := 23.069
const RELIEF_WARP := 24.950
const RELIEF_CONTINENT := 26.900
const RELIEF_REDISTRIBUTION := 29.013

const NORMALS_START := 30.906
const NORMALS_SWEEP_DURATION := 1.2
const NORMALS_LIT := 36.943
const NORMALS_LIT_DURATION := 1.0

const CLIMATE_TEMPERATURE := 42.388
const CLIMATE_MOISTURE := 43.537
const CLIMATE_SWEEP_DURATION := 1.0
# 44.0s-50.085s is a silence in the track; the diagram dims out into it and snaps back on the hit.
const CLIMATE_BREATH_START := 44.6
const CLIMATE_BREATH_DURATION := 4.8

const BIOMES_START := 50.085
# In demo_terrain_configuration.tres's own layer order: snow, rock, grass, dirt.
const BIOME_REVEAL_TIMES := [50.085, 53.534, 54.637, 55.449]
const BIOME_REVEAL_DURATION := 0.5
const BIOME_SLOPE_REVEAL := 60.825
const BIOME_SLOPE_DURATION := 0.8
const BRIDGE := 63.936
const BRIDGE_FLASH_DURATION := 0.34

# --- Look -----------------------------------------------------------------------------------
const WIRE_COLOR := Color(0.30, 0.82, 1.0)
const WIRE_INTENSITY := 1.15
const WIRE_INTENSITY_DIM := 0.7
# The fill sits well below the wireframe: the lines carry the image, the fill only tints the
# surface between them. At 1.0 the two together blow the whole diorama out to white.
const FILL_GAIN := 0.45
const SHADE_AMOUNT := 0.5
const RING_INTENSITY := [1.0, 0.62, 0.4]
const PULSE_DECAY := 0.11
const PULSE_ONSET_AMPLITUDE := 0.5
const RIPPLE_SPEED := 1150.0
const RIPPLE_DECAY := 0.45
const RIPPLE_LIFT := 20.0
# Each cut settles with a nearly imperceptible push-in, so a static frame still breathes.
const CUT_SETTLE := 0.022
const CUT_SETTLE_DURATION := 2.5
const FOV_PUNCH := 0.02

var _shot := "build"
var _preview_start := 0.0
var _preview_frames := 0

var _config: TerrainConfiguration
var _cue_times := PackedFloat32Array()
var _cue_amplitudes := PackedFloat32Array()
var _strong_times := PackedFloat32Array()

var _frame := 0
var _shot_start := 0.0
var _shot_duration_frames := 0
var _finished := false

var _full_noise := {}

@onready var _terrain: Terrain3D = $Terrain3D
@onready var _camera: Camera3D = $Camera3D
@onready var _world_environment: WorldEnvironment = $WorldEnvironment
@onready var _diorama: Node3D = $Diorama
@onready var _whittaker: Control = $Overlay/Whittaker


func _ready() -> void:
	_parse_args()

	if not SHOT_RANGES.has(_shot):
		_abort("unknown shot '%s'; known shots are %s" % [_shot, ", ".join(SHOT_RANGES.keys())])
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

	var window: Array = SHOT_RANGES[_shot]
	_shot_start = window[0]
	_shot_duration_frames = int(round((window[1] - window[0]) * FPS))
	if _preview_frames > 0:
		_shot_duration_frames = _preview_frames

	_diorama.global_position = DIORAMA_CENTER
	if not _diorama.setup(_config, DIORAMA_SIZE, DIORAMA_RESOLUTION, DIORAMA_RING_LEVELS):
		_abort("diorama setup failed")
		return

	_whittaker.setup(_config, _terrain, _diorama.biome_colors(), _diorama.biome_names(),
			Vector2(DIORAMA_CENTER.x, DIORAMA_CENTER.z), DIORAMA_SIZE * 0.5)
	_setup_shot()
	_apply(0)

	print("[trailer] shot=%s window=%.3fs-%.3fs start=%.3fs frames=%d viewport=%s" % [
			_shot, window[0], window[1], _shot_start + _preview_start, _shot_duration_frames,
			str(get_viewport().size)])


func _process(_delta: float) -> void:
	if _finished:
		return

	_frame += 1
	_apply(_frame)

	if _frame >= _shot_duration_frames:
		_finished = true
		print("[trailer] shot=%s done at frame %d" % [_shot, _frame])
		get_tree().quit()


# One-time per-shot look. The scene's Environment is a local sub-resource, so mutating it here only
# affects the running process, never the saved scene.
func _setup_shot() -> void:
	var environment := _world_environment.environment

	if _shot == "build":
		_camera.cull_mask = LAYER_DIORAMA
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
		environment.glow_intensity = 0.45
		environment.glow_strength = 0.85
		environment.glow_bloom = 0.0
		environment.glow_hdr_threshold = 1.15
		environment.set("glow_levels/4", 1.0)
		environment.set("glow_levels/5", 0.5)
		_whittaker.visible = true
	else:
		_camera.cull_mask = LAYER_TERRAIN
		_diorama.visible = false
		_whittaker.visible = false
		environment.sdfgi_enabled = true


func _apply(local_frame: int) -> void:
	var t := _shot_start + _preview_start + float(local_frame) / FPS
	if _shot == "build":
		_apply_build(t)
	else:
		_apply_cinematic(t)


# --- Build shot -----------------------------------------------------------------------------

func _apply_build(t: float) -> void:
	var chapter := _chapter_at(t)
	var pulse := _pulse_at(t)

	_apply_camera(chapter, t, pulse)

	_diorama.set_param("d_pulse", pulse)
	var ripple := _ripple_at(t)
	_diorama.set_param("d_ripple_radius", ripple.x)
	_diorama.set_param("d_ripple_strength", ripple.y)

	# Defaults every chapter starts from, then overrides below. Keeping them here (rather than
	# letting values persist) is what makes any single frame renderable on its own.
	var reveal_radius := DIORAMA_SIZE * 0.5
	var rings_visible := false
	var octaves: int = _full_noise["octaves"]
	var height_mix := 1.0
	var skirt_mix := 1.0
	var view_a := 6
	var view_b := 6
	var sweep := 0.0
	var sweep_dir := Vector2(1.0, 0.0)
	var fill_intensity := 1.0
	var shade := SHADE_AMOUNT
	var wire_intensity := WIRE_INTENSITY
	var noise_mix := 1.0
	var biome_reveals := PackedFloat32Array()
	var slope_reveal := 0.0
	var whittaker_fade := 0.0

	match chapter["name"]:
		"grid":
			# The grid draws outward from a single point at the first hit, then each clipmap ring
			# snaps in around it -- the clipmap's own structure, before there is any terrain.
			reveal_radius = _ease_out(_ramp(t, GRID_FIRST_HIT, GRID_LEVEL0_DURATION)) * DIORAMA_SIZE * 0.5
			for i in GRID_RING_TIMES.size():
				var ring_radius := DIORAMA_SIZE * 0.5 * pow(2.0, float(i + 1))
				reveal_radius = maxf(reveal_radius, _ease_out(_ramp(t, GRID_RING_TIMES[i], GRID_RING_DURATION)) * ring_radius)
			rings_visible = true
			height_mix = 0.0
			skirt_mix = 0.0
			octaves = 1
			view_a = 0
			view_b = 0
			shade = 0.0

		"noise":
			# Flat still: this beat is about the noise field itself, not the relief.
			height_mix = 0.0
			skirt_mix = 0.0
			octaves = _step_count(t, NOISE_OCTAVE_TIMES)
			view_a = 0
			view_b = 1
			sweep = _ease_in_out(_ramp(t, NOISE_START, NOISE_SWEEP_DURATION))
			shade = 0.0
			noise_mix = 0.0
			# This beat is the noise field itself, so the fill leads and the grid steps back.
			fill_intensity = 1.8
			wire_intensity = 0.85

		"relief":
			# The texture stands up into terrain, then the shaping parameters arrive one per hit.
			height_mix = _back_out(_ramp(t, RELIEF_LIFT, RELIEF_LIFT_DURATION))
			skirt_mix = _ease_out(_ramp(t, RELIEF_SKIRT, 1.4))
			view_a = 1
			view_b = 1
			shade = SHADE_AMOUNT * _ramp(t, RELIEF_LIFT + 1.9, 2.0)
			fill_intensity = 1.6

		"normals":
			view_a = 1
			view_b = 2
			sweep = _ease_in_out(_ramp(t, NORMALS_START, NORMALS_SWEEP_DURATION))
			sweep_dir = Vector2(0.0, 1.0)
			shade = 0.0
			fill_intensity = 1.6
			wire_intensity = 0.85
			if t >= NORMALS_LIT:
				view_a = 2
				view_b = 3
				sweep = _ease_in_out(_ramp(t, NORMALS_LIT, NORMALS_LIT_DURATION))
				sweep_dir = Vector2(-1.0, 0.0)
				shade = SHADE_AMOUNT * 1.6
				fill_intensity = 1.6

		"climate":
			view_a = 3
			view_b = 4
			sweep = _ease_in_out(_ramp(t, CLIMATE_TEMPERATURE, CLIMATE_SWEEP_DURATION))
			shade = 0.25
			if t >= CLIMATE_MOISTURE:
				view_a = 4
				view_b = 5
				sweep = _ease_in_out(_ramp(t, CLIMATE_MOISTURE, CLIMATE_SWEEP_DURATION))
				sweep_dir = Vector2(0.0, -1.0)
			# Into the track's silence the whole diagram dims down to almost nothing, so the biome
			# hit at 50.085s lands on a near-black frame.
			var breath := _ease_in_out(_ramp(t, CLIMATE_BREATH_START, CLIMATE_BREATH_DURATION))
			# The climate and biome views are the only ones carrying real colour, so the fill leads
			# and the wireframe steps back rather than washing them out.
			fill_intensity = lerpf(1.7, 0.1, breath)
			wire_intensity = lerpf(0.8, WIRE_INTENSITY_DIM, breath)
			shade = lerpf(shade, 0.0, breath)

		"biomes":
			view_a = 6
			view_b = 6
			# Snap back out of the breath, overshooting bright on the hit itself.
			fill_intensity = 1.7 + 0.9 * exp(-maxf(t - BIOMES_START, 0.0) / 0.3)
			wire_intensity = 0.75
			for i in _diorama.biome_colors().size():
				biome_reveals.append(_ease_out(_ramp(t, _biome_reveal_time(i), BIOME_REVEAL_DURATION)))
			slope_reveal = _ease_out(_ramp(t, BIOME_SLOPE_REVEAL, BIOME_SLOPE_DURATION))
			whittaker_fade = _ease_out(_ramp(t, BIOMES_START, 0.6))
			# One last flash carries the cut into the bridge.
			var flash := _ramp(t, BRIDGE - BRIDGE_FLASH_DURATION, BRIDGE_FLASH_DURATION)
			wire_intensity += 4.0 * flash * flash
			fill_intensity += 1.2 * flash * flash

	if biome_reveals.is_empty():
		for i in _diorama.biome_colors().size():
			biome_reveals.append(0.0)

	# The ripple lifts the grid on the beat while it is still flat; once there is real relief the
	# same displacement only reads as a stray ring rolling across the mountains.
	_diorama.set_param("d_ripple_lift", RIPPLE_LIFT * (1.0 - clampf(height_mix, 0.0, 1.0)))
	_diorama.set_param("d_reveal_radius", reveal_radius)
	_diorama.set_param("d_height_mix", height_mix)
	_diorama.set_param("d_skirt_mix", skirt_mix)
	_diorama.set_param("d_view_a", view_a)
	_diorama.set_param("d_view_b", view_b)
	_diorama.set_param("d_sweep", sweep)
	_diorama.set_param("d_sweep_dir", sweep_dir)
	_diorama.set_param("d_fill_intensity", fill_intensity * FILL_GAIN)
	_diorama.set_param("d_shade_amount", shade)
	_diorama.set_param("d_wire_intensity", wire_intensity)
	_diorama.set_param("d_wire_color", Vector3(WIRE_COLOR.r, WIRE_COLOR.g, WIRE_COLOR.b))
	_diorama.set_param("d_biome_reveal", biome_reveals)
	_diorama.set_param("d_rock_reveal", slope_reveal)
	_diorama.set_param("d_light_dir", _light_direction(t))
	_diorama.set_param("octaves", octaves)

	_apply_noise_parameters(t, noise_mix)

	_diorama.set_ring_levels_visible(rings_visible)
	for i in _diorama.level_count():
		_diorama.set_level_intensity(i, RING_INTENSITY[i] if i < RING_INTENSITY.size() else 0.3)

	_whittaker.set_state(whittaker_fade, biome_reveals, slope_reveal)


# The shaping parameters ramp in one per hit during the relief chapter, and stay at the config's own
# values afterwards. p_mix scales the whole set to zero for chapters that predate the relief.
func _apply_noise_parameters(t: float, p_mix: float) -> void:
	var ridge: float = _full_noise["ridge_amount"] * _ease_out(_ramp(t, RELIEF_RIDGE, 0.7))
	var warp: float = _full_noise["warp_amount"] * _ease_out(_ramp(t, RELIEF_WARP, 0.9))
	var continent := _ease_out(_ramp(t, RELIEF_CONTINENT, 1.1))
	var redistribution := lerpf(1.0, _full_noise["redistribution"], _ease_out(_ramp(t, RELIEF_REDISTRIBUTION, 0.8)))

	_diorama.set_param("ridge_amount", ridge * p_mix)
	_diorama.set_param("warp_amount", warp * p_mix)
	_diorama.set_param("continent_influence", _full_noise["continent_influence"] * continent * p_mix)
	_diorama.set_param("continent_elevation", _full_noise["continent_elevation"] * continent * p_mix)
	_diorama.set_param("relief_floor", lerpf(1.0, _full_noise["relief_floor"], continent * p_mix))
	_diorama.set_param("redistribution", lerpf(1.0, redistribution, p_mix))


func _apply_camera(p_chapter: Dictionary, t: float, p_pulse: float) -> void:
	var settle := _ease_out(_ramp(t, p_chapter["start"], CUT_SETTLE_DURATION))
	var distance: float = p_chapter["distance"] * (1.0 - CUT_SETTLE * settle)
	var yaw := deg_to_rad(float(p_chapter["yaw"]))
	var pitch := deg_to_rad(float(p_chapter["pitch"]))

	var target := DIORAMA_CENTER + Vector3(0.0, float(p_chapter["height"]), 0.0)
	var offset := Vector3(sin(yaw) * cos(pitch), -sin(pitch), cos(yaw) * cos(pitch)) * distance

	_camera.global_position = target + offset
	_camera.look_at(target, Vector3.UP)
	_camera.fov = float(p_chapter["fov"]) * (1.0 - FOV_PUNCH * p_pulse)
	_camera.far = maxf(_camera.far, distance * 3.0)


# --- Cinematic shot -------------------------------------------------------------------------

# Placeholder until T6: holds the biome chapter's framing on the real, fully shaded terrain, so the
# bridge cut reads as the same world switching from diagram to render, then eases in.
func _apply_cinematic(t: float) -> void:
	var chapter: Dictionary = CHAPTERS[CHAPTERS.size() - 1].duplicate()
	chapter["start"] = BRIDGE
	chapter["distance"] = lerpf(float(chapter["distance"]), float(chapter["distance"]) * 0.72,
			_ease_in_out(_ramp(t, BRIDGE, 24.0)))
	_apply_camera(chapter, t, 0.0)


# --- Cue envelopes --------------------------------------------------------------------------

# A short exponential decay off the most recent cue, driving wire brightness and the FOV punch:
# the diagram visibly reacts on every hit instead of only on the ones that change a parameter.
func _pulse_at(t: float) -> float:
	var pulse := 0.0
	for i in _cue_times.size():
		var age := t - _cue_times[i]
		if age < 0.0:
			break
		pulse = maxf(pulse, _cue_amplitudes[i] * exp(-age / PULSE_DECAY))
	return clampf(pulse, 0.0, 1.0)


# A ring expanding out of the centre on each "strong" cue, lifting and lighting the wireframe as it
# passes. Returns (radius, strength).
func _ripple_at(t: float) -> Vector2:
	for i in range(_strong_times.size() - 1, -1, -1):
		var age := t - _strong_times[i]
		if age < 0.0:
			continue
		return Vector2(age * RIPPLE_SPEED, exp(-age / RIPPLE_DECAY))
	return Vector2(-1.0, 0.0)


# The key light swings slowly through the lit/climate/biome chapters, so relief keeps reading on an
# otherwise motionless frame.
func _light_direction(t: float) -> Vector3:
	var angle := deg_to_rad(40.0) + maxf(t - NORMALS_LIT, 0.0) * 0.18
	return Vector3(sin(angle) * 0.75, 0.62, cos(angle) * 0.75)


# --- Helpers --------------------------------------------------------------------------------

func _chapter_at(t: float) -> Dictionary:
	var current: Dictionary = CHAPTERS[0]
	for chapter in CHAPTERS:
		if t >= float(chapter["start"]):
			current = chapter
	return current


func _biome_reveal_time(p_index: int) -> float:
	if p_index < BIOME_REVEAL_TIMES.size():
		return BIOME_REVEAL_TIMES[p_index]
	# An edited config with more biomes than the track has marked hits for still reveals them all.
	return BIOME_REVEAL_TIMES[BIOME_REVEAL_TIMES.size() - 1] + float(p_index - BIOME_REVEAL_TIMES.size() + 1)


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


func _parse_args() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--shot="):
			_shot = arg.substr("--shot=".length())
		elif arg.begins_with("--start="):
			_preview_start = arg.substr("--start=".length()).to_float()
		elif arg.begins_with("--frames="):
			_preview_frames = arg.substr("--frames=".length()).to_int()


func _load_cues() -> void:
	var file := FileAccess.open(CUES_PATH, FileAccess.READ)
	if file == null:
		printerr("[trailer] could not open %s" % CUES_PATH)
		return

	var parsed: Variant = JSON.parse_string(file.get_as_text())
	if typeof(parsed) != TYPE_DICTIONARY or not parsed.has("cues"):
		printerr("[trailer] could not parse %s" % CUES_PATH)
		return

	for cue in parsed["cues"]:
		var time := float(cue["time_sec"])
		_cue_times.append(time)
		_cue_amplitudes.append(PULSE_ONSET_AMPLITUDE if cue["kind"] == "onset" else 1.0)
		if cue["kind"] != "onset":
			_strong_times.append(time)


func _abort(message: String) -> void:
	_finished = true
	printerr("[trailer] %s" % message)
	get_tree().quit(1)
