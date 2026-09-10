extends Node3D

# Benchmark harness for TerrainServer.
#
# One process = one measurement point. The camera walks a fixed path over a dedicated
# terrain scene while this script samples Godot's per-viewport GPU/CPU timers, the
# collision regrid latency and the memory counters, then prints a single BENCHRESULT
# JSON line and quits. Driven by run_benchmarks.gd; see docs/benchmarks.md.

const CONFIG_PATH := "res://addons/terrain_server/assets/terrains/demo_terrain_configuration.tres"

const WARMUP_FRAMES := 90
const SAMPLE_FRAMES := 240
const FRAME_CAP := 600

# Indexed by frame number, never by delta, so a slower machine walks the identical path and the regrid count stays comparable.
const CAMERA_STEP := 2.0
const CAMERA_ALTITUDE := 20.0
const CAMERA_PITCH_DEGREES := -15.0
const CAMERA_YAW_DEGREES := -90.0

# Bounds are the PROPERTY_HINT_RANGE declarations in src/core/terrain_configuration.cpp.
# That is the source of truth; do not invent one here.
const SWEEPABLE := {
	"terrain_size": [1.0, 10000.0],
	"height_scale": [0.1, 1000.0],
	"mesh_resolution": [1.0, 512.0],
	"clipmap_levels": [1.0, 10.0],
	"noise_octaves": [1.0, 10.0],
	"physics_range": [8.0, 512.0],
	"pom_min_steps": [1.0, 64.0],
	"pom_max_steps": [1.0, 128.0],
}
const INT_PROPERTIES := ["mesh_resolution", "clipmap_levels", "noise_octaves", "pom_min_steps", "pom_max_steps"]

const ECHOED_PROPERTIES := ["terrain_size", "height_scale", "mesh_resolution", "clipmap_levels",
		"noise_octaves", "physics_range", "pom_min_steps", "pom_max_steps",
		"pom_fade_start", "pom_fade_end"]

# One CSV row per run. Kept in one place so the header and the row cannot drift apart.
const CSV_COLUMNS := [
	"device", "round", "scenario", "os", "cpu_name", "gpu_name", "godot", "frames", "watchdog",
	"terrain_size", "height_scale", "mesh_resolution", "clipmap_levels", "noise_octaves",
	"physics_range", "pom_min_steps", "pom_max_steps", "pom_fade_start", "pom_fade_end",
	"collision_range", "collision_resolution",
	"gpu_p10", "gpu_p50", "gpu_mean", "gpu_p90", "gpu_min", "gpu_max",
	"cpu_p10", "cpu_p50", "cpu_mean",
	"draw_calls", "primitives",
	"regrid_count", "regrid_mean", "regrid_p90",
	"video_mem_mib", "static_mem_mib",
]

var _scenario := "baseline"
var _device := "unknown"
var _round := 1

var _config: TerrainConfiguration = null
var _terrain: Terrain3D = null
var _camera: Camera3D = null
var _viewport_rid := RID()

var _frame := 0
var _finished := false

var _gpu_samples := PackedFloat64Array()
var _cpu_samples := PackedFloat64Array()
var _draw_calls := PackedFloat64Array()
var _primitives := PackedFloat64Array()
var _video_mem_peak := 0.0
var _static_mem_peak := 0.0

var _regrid_pending := false
var _regrid_start_usec := 0
var _regrid_start_frame := 0
var _regrid_latencies := PackedFloat64Array()


func _ready() -> void:
	_terrain = $Terrain3D as Terrain3D
	_camera = $Camera3D as Camera3D

	_parse_args()

	var base: TerrainConfiguration = load(CONFIG_PATH)
	if base == null:
		_abort("could not load %s" % CONFIG_PATH)
		return

	# duplicate() before any override, so a sweep never dirties the shared cached resource.
	_config = base.duplicate() as TerrainConfiguration
	if not _apply_scenario(_config):
		return

	_terrain.configuration = _config

	_camera.rotation = Vector3(deg_to_rad(CAMERA_PITCH_DEGREES), deg_to_rad(CAMERA_YAW_DEGREES), 0.0)
	_position_camera(0)

	_viewport_rid = get_viewport().get_viewport_rid()
	RenderingServer.viewport_set_measure_render_time(_viewport_rid, true)

	print("[bench] scenario=%s device=%s adapter=%s driver=%s godot=%s" % [
		_scenario, _device, RenderingServer.get_video_adapter_name(),
		ProjectSettings.get_setting("rendering/rendering_device/driver", "?"),
		Engine.get_version_info()["string"]])
	print("[bench] config %s" % _echo_config())


func _process(_delta: float) -> void:
	if _finished:
		return

	_frame += 1
	_position_camera(_frame)
	_sample_regrid()

	if _frame > WARMUP_FRAMES:
		_collect_sample()

	if _gpu_samples.size() >= SAMPLE_FRAMES or _frame >= FRAME_CAP:
		_finish()


func _parse_args() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--scenario="):
			_scenario = arg.substr("--scenario=".length())
		elif arg.begins_with("--device="):
			_device = arg.substr("--device=".length())
		elif arg.begins_with("--round="):
			_round = int(arg.substr("--round=".length()))


# baseline | pom_off | stress | pom_<min>_<max> | <property>_<value>
func _apply_scenario(cfg: TerrainConfiguration) -> bool:
	if _scenario == "baseline":
		return true

	if _scenario == "pom_off":
		cfg.pom_fade_start = 0.0
		cfg.pom_fade_end = 0.001
		return true

	if _scenario == "stress":
		cfg.clipmap_levels = 10
		cfg.mesh_resolution = 512
		cfg.noise_octaves = 10
		cfg.physics_range = 512.0
		cfg.pom_min_steps = 64
		cfg.pom_max_steps = 128
		return true

	for key in SWEEPABLE:
		if _scenario.begins_with("%s_" % key):
			return _apply_value(cfg, key, _scenario.substr(key.length() + 1))

	var parts := _scenario.split("_")
	if parts.size() == 3 and parts[0] == "pom":
		return _apply_value(cfg, "pom_min_steps", parts[1]) and _apply_value(cfg, "pom_max_steps", parts[2])

	_abort("unknown scenario '%s'" % _scenario)
	return false


func _apply_value(cfg: TerrainConfiguration, key: String, raw: String) -> bool:
	if not raw.is_valid_float():
		_abort("scenario '%s': '%s' is not a number" % [_scenario, raw])
		return false

	var value := raw.to_float()
	var bounds: Array = SWEEPABLE[key]
	if value < bounds[0] or value > bounds[1]:
		_abort("scenario '%s': %s=%s is outside the declared range %s..%s" % [
				_scenario, key, raw, bounds[0], bounds[1]])
		return false

	cfg.set(key, int(value) if key in INT_PROPERTIES else value)
	return true


func _position_camera(frame: int) -> void:
	var x := float(frame) * CAMERA_STEP
	var z := 0.0
	_camera.global_position = Vector3(x, _terrain.get_height_at(Vector2(x, z)) + CAMERA_ALTITUDE, z)


func _sample_regrid() -> void:
	var pending: bool = _terrain.is_collision_rebuild_pending()
	if pending and not _regrid_pending:
		_regrid_start_usec = Time.get_ticks_usec()
		_regrid_start_frame = _frame
	elif _regrid_pending and not pending:
		# Polled once per frame, so the granularity of this is one frame.
		if _regrid_start_frame > WARMUP_FRAMES:
			_regrid_latencies.append(float(Time.get_ticks_usec() - _regrid_start_usec) / 1000.0)
	_regrid_pending = pending


func _collect_sample() -> void:
	_gpu_samples.append(RenderingServer.viewport_get_measured_render_time_gpu(_viewport_rid))
	_cpu_samples.append(RenderingServer.viewport_get_measured_render_time_cpu(_viewport_rid))
	_draw_calls.append(Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME))
	_primitives.append(Performance.get_monitor(Performance.RENDER_TOTAL_PRIMITIVES_IN_FRAME))
	_video_mem_peak = maxf(_video_mem_peak, Performance.get_monitor(Performance.RENDER_VIDEO_MEM_USED))
	_static_mem_peak = maxf(_static_mem_peak, Performance.get_monitor(Performance.MEMORY_STATIC))


func _echo_config() -> Dictionary:
	var echoed := {}
	for key in ECHOED_PROPERTIES:
		echoed[key] = _config.get(key)
	return echoed


func _finish() -> void:
	if _finished:
		return
	_finished = true

	var result := {
		"scenario": _scenario,
		"device": _device,
		"adapter": RenderingServer.get_video_adapter_name(),
		"godot": Engine.get_version_info()["string"],
		"os": OS.get_name(),
		"cpu": OS.get_processor_name(),
		"frames": _frame,
		"watchdog": _gpu_samples.size() < SAMPLE_FRAMES,
		"config": _echo_config(),
		"collision_range": _terrain.get_collision_range(),
		"collision_resolution": _terrain.get_collision_resolution(),
		"gpu_ms": _stats(_gpu_samples),
		"cpu_ms": _stats(_cpu_samples),
		"draw_calls": _stats(_draw_calls),
		"primitives": _stats(_primitives),
		"regrid_ms": _stats(_regrid_latencies),
		"video_mem_mib": _video_mem_peak / 1048576.0,
		"static_mem_mib": _static_mem_peak / 1048576.0,
	}

	print("BENCHRESULT %s" % JSON.stringify(result))
	print("BENCHCSVHEADER %s" % ",".join(CSV_COLUMNS))
	print("BENCHCSV %s" % _csv_row(result))
	print("[bench] done: gpu p10 %.3f ms | cpu p10 %.3f ms | %d regrids" % [
			result["gpu_ms"]["p10"], result["cpu_ms"]["p10"], result["regrid_ms"]["n"]])
	get_tree().quit()


func _csv_row(result: Dictionary) -> String:
	var gpu: Dictionary = result["gpu_ms"]
	var cpu: Dictionary = result["cpu_ms"]
	var regrid: Dictionary = result["regrid_ms"]
	var cfg: Dictionary = result["config"]

	var flat := {
		"device": result["device"], "round": _round, "scenario": result["scenario"],
		"os": result["os"], "cpu_name": result["cpu"], "gpu_name": result["adapter"],
		"godot": result["godot"], "frames": result["frames"], "watchdog": result["watchdog"],
		"collision_range": result["collision_range"],
		"collision_resolution": result["collision_resolution"],
		"gpu_p10": gpu["p10"], "gpu_p50": gpu["p50"], "gpu_mean": gpu["mean"],
		"gpu_p90": gpu["p90"], "gpu_min": gpu["min"], "gpu_max": gpu["max"],
		"cpu_p10": cpu["p10"], "cpu_p50": cpu["p50"], "cpu_mean": cpu["mean"],
		"draw_calls": int(result["draw_calls"]["p50"]), "primitives": int(result["primitives"]["p50"]),
		"regrid_count": regrid["n"],
		"regrid_mean": regrid.get("mean", ""), "regrid_p90": regrid.get("p90", ""),
		"video_mem_mib": result["video_mem_mib"], "static_mem_mib": result["static_mem_mib"],
	}
	flat.merge(cfg)

	var cells := PackedStringArray()
	for column in CSV_COLUMNS:
		cells.append(_csv_cell(flat.get(column, "")))
	return ",".join(cells)


func _csv_cell(value: Variant) -> String:
	if value is float:
		return "%.4f" % value
	if value is int or value is bool:
		return str(value)

	var text := str(value)
	if text.contains(",") or text.contains("\"") or text.contains("\n"):
		return "\"%s\"" % text.replace("\"", "\"\"")
	return text


func _stats(values: PackedFloat64Array) -> Dictionary:
	if values.is_empty():
		return {"n": 0}

	var sorted := values.duplicate()
	sorted.sort()

	var sum := 0.0
	for v in sorted:
		sum += v

	return {
		"n": sorted.size(),
		"mean": sum / float(sorted.size()),
		"p10": _percentile(sorted, 0.10),
		"p50": _percentile(sorted, 0.50),
		"p90": _percentile(sorted, 0.90),
		"min": sorted[0],
		"max": sorted[sorted.size() - 1],
	}


func _percentile(sorted: PackedFloat64Array, q: float) -> float:
	var idx := int(round(q * float(sorted.size() - 1)))
	return sorted[clampi(idx, 0, sorted.size() - 1)]


func _abort(message: String) -> void:
	_finished = true
	printerr("[bench] %s" % message)
	get_tree().quit(1)
