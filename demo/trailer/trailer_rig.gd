extends Node3D

# Shared scaffold for the showreel trailer clips (see TODO_TRAILER.md T2). Handles cue loading,
# frame-indexed shot timing and a placeholder camera path; T3-T6 replace `_position_camera()`
# with each shot's real choreography and only need `_local_frame`/`_terrain` to do it.

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

var _shot := "genesis"
var _cues: Array = []
var _frame := 0
var _shot_start_frame := 0
var _shot_end_frame := 0
var _finished := false

@onready var _terrain: Terrain3D = $Terrain3D
@onready var _camera: Camera3D = $Camera3D


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

	_cues = _load_cues()

	var window: Array = SHOT_RANGES[_shot]
	_shot_start_frame = _frame_for_time(window[0])
	_shot_end_frame = _frame_for_time(window[1])

	_camera.rotation = Vector3(deg_to_rad(CAMERA_PITCH_DEGREES), deg_to_rad(CAMERA_YAW_DEGREES), 0.0)
	_position_camera(0)

	print("[trailer] shot=%s window=%.3fs-%.3fs frames=%d-%d (%d frames) cues_in_range=%d" % [
			_shot, window[0], window[1], _shot_start_frame, _shot_end_frame,
			_shot_end_frame - _shot_start_frame, _cues_in_range(window[0], window[1]).size()])


func _process(_delta: float) -> void:
	if _finished:
		return

	_frame += 1
	_position_camera(_frame - _shot_start_frame)

	if _frame >= _shot_end_frame:
		_finished = true
		print("[trailer] shot=%s done at frame %d" % [_shot, _frame])
		get_tree().quit()


# Placeholder path until T3-T6 give each shot its real camera choreography: a straight,
# ground-hugging travelling shot over the terrain, reusing benchmark.gd's frame-step pattern.
func _position_camera(local_frame: int) -> void:
	var x := float(local_frame) * CAMERA_STEP
	var z := 0.0
	_camera.global_position = Vector3(x, _terrain.get_height_at(Vector2(x, z)) + CAMERA_ALTITUDE, z)


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
