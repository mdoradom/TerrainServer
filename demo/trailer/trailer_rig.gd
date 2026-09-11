extends Node3D

# Shared scaffold for the showreel trailer clips (see TODO_TRAILER.md T2). Handles cue loading
# and frame-indexed shot timing; `_position_camera()` dispatches to each shot's own choreography
# by name, falling back to a straight-line placeholder for shots T4-T6 haven't implemented yet.

const CONFIG_PATH := "res://addons/terrain_server/assets/terrains/demo_terrain_configuration.tres"
const CUES_PATH := "res://trailer/audio_cues.json"
const FPS := 60.0

# Provisional split of the track into per-shot windows, snapped to "strong" cue timestamps from
# audio_cues.json (regenerate via demo/trailer/tools/extract_cues.py if the track changes): the
# pre-bridge span (beats 1-4) divided roughly into thirds by strong-cue count. Each shot's own
# task (T3-T6) may narrow this window once it implements real per-shot content; the cinematic
# end is a placeholder (full track) until T6 picks the real trimmed endpoint.
const SHOT_RANGES := {
	"genesis": [0.0, 20.619],
	"erosion": [20.619, 42.388],
	"debug": [42.388, 63.936],
	"cinematic": [63.936, 145.276],
}

# Indexed by frame number, never by delta, matching demo/benchmark/benchmark.gd's convention so
# --write-movie renders the identical path regardless of machine speed.
const CAMERA_STEP := 2.0
const CAMERA_ALTITUDE := 20.0
const CAMERA_PITCH_DEGREES := -15.0
const CAMERA_YAW_DEGREES := -90.0

# Genesis shot (T3, beats 1-2): the terrain is an infinite clipmap that recenters on the camera
# (focus_path below), so there is no "far away, outside the terrain" vantage to move the camera
# to, and a ground-hugging grazing angle doesn't collapse to a flat line either - this config's
# mountains are tall/frequent enough that some peak always intrudes nearby. Instead the reveal
# looks from very high up (a multiple of height_scale) with a dead-level pitch and a needle-narrow
# vertical FOV framed exactly on the horizon: any terrain below the frustum is invisible, and the
# terrain that *is* in view is so far off (near-field is out of frame; only the horizon itself is
# in it) that its height variation is angularly negligible - it reads as a flat line regardless of
# local roughness. Widening the FOV and dropping altitude/pitch over the reveal brings the near
# field into view and grows that line into the full deformed wireframe plane. A hard cut on a cue
# timestamp (GENESIS_CUT_SEC, the "strong" cue nearest the reveal's natural midpoint) then jumps
# to a higher, wider mountain flyover.
const GENESIS_CUT_SEC := 9.451
const GENESIS_REVEAL_START_ALTITUDE_FACTOR := 3.0
const GENESIS_REVEAL_START_FOV_DEGREES := 6.0
const GENESIS_TRAVEL_ALTITUDE := 10.0
const GENESIS_TRAVEL_STEP := 5.0
const GENESIS_TRAVEL_PITCH_DEGREES := -12.0
const GENESIS_TRAVEL_FOV_DEGREES := 70.0
const GENESIS_FLYOVER_ALTITUDE := 350.0
const GENESIS_FLYOVER_STEP := 9.0
const GENESIS_FLYOVER_PITCH_DEGREES := -25.0
const GENESIS_FLYOVER_FOV_DEGREES := 75.0

var _shot := "genesis"
var _cues: Array = []
var _frame := 0
var _shot_start_frame := 0
var _shot_end_frame := 0
var _shot_duration_frames := 0
var _finished := false
var _genesis_reveal_start_altitude := 0.0
var _genesis_reveal_start_pitch_degrees := 0.0

@onready var _terrain: Terrain3D = $Terrain3D
@onready var _camera: Camera3D = $Camera3D
@onready var _world_environment: WorldEnvironment = $WorldEnvironment


func _ready() -> void:
	_parse_args()

	if not SHOT_RANGES.has(_shot):
		_abort("unknown shot '%s'; known shots are %s" % [_shot, ", ".join(SHOT_RANGES.keys())])
		return

	var config: TerrainConfiguration = load(CONFIG_PATH)
	if config == null:
		_abort("could not load %s" % CONFIG_PATH)
		return
	_terrain.configuration = config
	_genesis_reveal_start_altitude = config.height_scale * GENESIS_REVEAL_START_ALTITUDE_FACTOR
	# The outermost clipmap block is a unit square (see TerrainGenerator::create_block_mesh)
	# scaled by this level's "scale", so it only reaches half that far from the focus point.
	var visible_radius := _terrain.get_clipmap_level_extent(_terrain.get_clipmap_level_count() - 1) * 0.5
	_genesis_reveal_start_pitch_degrees = -rad_to_deg(atan(_genesis_reveal_start_altitude / (visible_radius * 0.9)))

	_cues = _load_cues()

	var window: Array = SHOT_RANGES[_shot]
	_shot_start_frame = _frame_for_time(window[0])
	_shot_end_frame = _frame_for_time(window[1])
	_shot_duration_frames = _shot_end_frame - _shot_start_frame

	_setup_shot()
	_camera.rotation = Vector3(deg_to_rad(CAMERA_PITCH_DEGREES), deg_to_rad(CAMERA_YAW_DEGREES), 0.0)
	_position_camera(0)

	print("[trailer] shot=%s window=%.3fs-%.3fs frames=%d-%d (%d frames) cues_in_range=%d" % [
			_shot, window[0], window[1], _shot_start_frame, _shot_end_frame,
			_shot_duration_frames, _cues_in_range(window[0], window[1]).size()])


func _process(_delta: float) -> void:
	if _finished:
		return

	_frame += 1
	_position_camera(_frame)

	if _frame >= _shot_duration_frames:
		_finished = true
		print("[trailer] shot=%s done at frame %d" % [_shot, _frame])
		get_tree().quit()


# One-time per-shot look: wireframe rendering and a solid black background for genesis's
# diagram-like reveal. Mutates the scene's own (process-local) Environment resource, never saved.
func _setup_shot() -> void:
	if _shot != "genesis":
		return

	get_viewport().debug_draw = Viewport.DEBUG_DRAW_WIREFRAME
	var environment := _world_environment.environment
	environment.background_mode = Environment.BG_COLOR
	environment.background_color = Color.BLACK
	environment.fog_enabled = false

	# The reveal's high-altitude horizon shot looks well past Camera3D's 4000-unit default far
	# plane, so extend it to comfortably cover the outermost clipmap ring.
	_camera.far = _terrain.get_clipmap_level_extent(_terrain.get_clipmap_level_count() - 1) * 2.0


func _position_camera(local_frame: int) -> void:
	if _shot == "genesis":
		_position_camera_genesis(local_frame)
	else:
		_position_camera_placeholder(local_frame)


# Placeholder path until T4-T6 give their shots real camera choreography: a straight,
# ground-hugging travelling shot over the terrain, reusing benchmark.gd's frame-step pattern.
func _position_camera_placeholder(local_frame: int) -> void:
	var x := float(local_frame) * CAMERA_STEP
	var z := 0.0
	_camera.global_position = Vector3(x, _terrain.get_height_at(Vector2(x, z)) + CAMERA_ALTITUDE, z)


func _position_camera_genesis(local_frame: int) -> void:
	var cut_frame := _frame_for_time(GENESIS_CUT_SEC) - _shot_start_frame
	if local_frame < cut_frame:
		_position_camera_genesis_reveal(local_frame, cut_frame)
	else:
		_position_camera_genesis_flyover(local_frame - cut_frame, cut_frame)


# Beat 1: keeps translating forward throughout, but swoops altitude/pitch/FOV from the grazing
# reveal pose out to the normal ground-level-travelling pose, so the terrain grows from a flat
# line at frame 0 into the full deformed wireframe plane by the hard cut.
func _position_camera_genesis_reveal(local_frame: int, cut_frame: int) -> void:
	var t := smoothstep(0.0, 1.0, float(local_frame) / float(cut_frame))
	var x := float(local_frame) * GENESIS_TRAVEL_STEP
	var z := 0.0
	var altitude := lerpf(_genesis_reveal_start_altitude, GENESIS_TRAVEL_ALTITUDE, t)
	var pitch := lerpf(_genesis_reveal_start_pitch_degrees, GENESIS_TRAVEL_PITCH_DEGREES, t)
	_camera.fov = lerpf(GENESIS_REVEAL_START_FOV_DEGREES, GENESIS_TRAVEL_FOV_DEGREES, t)
	_camera.global_position = Vector3(x, _terrain.get_height_at(Vector2(x, z)) + altitude, z)
	_camera.rotation = Vector3(deg_to_rad(pitch), deg_to_rad(CAMERA_YAW_DEGREES), 0.0)


# Beat 2: a hard cut (landing on GENESIS_CUT_SEC) into a higher, wider sweep over the wireframe
# mountains for the rest of the shot. Position keeps progressing from where the reveal left off;
# only the framing (altitude/pitch/FOV) jumps discontinuously, which reads as the cut.
func _position_camera_genesis_flyover(frame_since_cut: int, cut_frame: int) -> void:
	var x := float(cut_frame) * GENESIS_TRAVEL_STEP + float(frame_since_cut) * GENESIS_FLYOVER_STEP
	var z := 0.0
	_camera.fov = GENESIS_FLYOVER_FOV_DEGREES
	_camera.global_position = Vector3(x, _terrain.get_height_at(Vector2(x, z)) + GENESIS_FLYOVER_ALTITUDE, z)
	_camera.rotation = Vector3(deg_to_rad(GENESIS_FLYOVER_PITCH_DEGREES), deg_to_rad(CAMERA_YAW_DEGREES), 0.0)


func _parse_args() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--shot="):
			_shot = arg.substr("--shot=".length())


func _load_cues() -> Array:
	var file := FileAccess.open(CUES_PATH, FileAccess.READ)
	if file == null:
		printerr("[trailer] could not open %s" % CUES_PATH)
		return []

	var parsed: Variant = JSON.parse_string(file.get_as_text())
	if typeof(parsed) != TYPE_DICTIONARY or not parsed.has("cues"):
		printerr("[trailer] could not parse %s" % CUES_PATH)
		return []

	return parsed["cues"]


func _cues_in_range(start_sec: float, end_sec: float) -> Array:
	return _cues.filter(func(cue: Dictionary) -> bool:
		return cue["time_sec"] >= start_sec and cue["time_sec"] < end_sec)


func _frame_for_time(sec: float) -> int:
	return int(round(sec * FPS))


func _abort(message: String) -> void:
	_finished = true
	printerr("[trailer] %s" % message)
	get_tree().quit(1)
