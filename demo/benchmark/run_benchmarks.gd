extends Node

# Benchmark runner. Spawns one Godot process per measurement point and collects the results.
#
#   scons benchmark scenarios=@baseline rounds=2
#
# Rounds are interleaved (the whole scenario list runs, then runs again) so machine load
# drifting over a session shows up as a spread between rounds rather than a trend along the
# list. One process per point is deliberate: it keeps shader caches and driver state from one
# measurement out of the next.
#
# Arguments (all optional):
#   --scenarios=a,b,c   scenario names or @groups (default @baseline)
#   --rounds=N          interleaved rounds (default 2)
#   --out=PATH          output directory (default <repo>/.benchmarks)
#   --device=NAME       label recorded in the results (default: hostname)

const BENCH_SCENE := "res://benchmark/benchmark.tscn"
const RESOLUTION := "1920x1080"
const LOG_TAIL_LINES := 40

var _out_dir := ""
var _device := ""
var _rounds := 2
var _scenarios := PackedStringArray()


func _ready() -> void:
	_run.call_deferred()


func _run() -> void:
	_parse_args()
	if _scenarios.is_empty():
		return

	var dir_error := DirAccess.make_dir_recursive_absolute(_out_dir)
	if dir_error != OK and not DirAccess.dir_exists_absolute(_out_dir):
		_abort("could not create output directory %s (error %d)" % [_out_dir, dir_error])
		return

	var csv_path := _out_dir.path_join("results.csv")
	var json_path := _out_dir.path_join("results.jsonl")
	var csv := FileAccess.open(csv_path, FileAccess.WRITE)
	var jsonl := FileAccess.open(json_path, FileAccess.WRITE)
	if csv == null or jsonl == null:
		_abort("could not write to %s" % _out_dir)
		return

	print("device=%s driver=%s rounds=%d scenarios=%d" % [
			_device, _driver(), _rounds, _scenarios.size()])
	print("results -> %s" % csv_path)

	var runs := 0
	for round_index in range(1, _rounds + 1):
		for scenario in _scenarios:
			var output := _measure(scenario, round_index)
			if output.is_empty():
				return

			# The harness emits the header itself, so the column list has one definition.
			if runs == 0:
				csv.store_line(_extract(output, "BENCHCSVHEADER "))
			csv.store_line(_extract(output, "BENCHCSV "))
			jsonl.store_line(_extract(output, "BENCHRESULT "))
			csv.flush()
			jsonl.flush()
			runs += 1

	print("done: %d runs in %s" % [runs, csv_path])
	get_tree().quit()


# Returns the child's full output, or an empty string once it has reported the failure.
func _measure(scenario: String, round_index: int) -> String:
	var arguments := PackedStringArray([
		"--path", ProjectSettings.globalize_path("res://"),
		"--rendering-driver", _driver(),
		"--resolution", RESOLUTION,
		"--disable-vsync",
		BENCH_SCENE,
		"--",
		"--scenario=%s" % scenario,
		"--device=%s" % _device,
		"--round=%d" % round_index,
	])

	var executable := OS.get_executable_path()
	if _needs_virtual_display():
		arguments.insert(0, executable)
		arguments.insert(0, "-a")
		executable = "xvfb-run"

	printraw("round %d  %-24s " % [round_index, scenario])

	var captured := []
	var exit_code := OS.execute(executable, arguments, captured, true)
	var output := "\n".join(PackedStringArray(captured))

	# The full log is kept: a GDScript parse error leaves the scene empty and the run never
	# quits, and that is only visible in the lines a search for the result line would discard.
	var log_path := _out_dir.path_join("%s.r%d.log" % [scenario, round_index])
	var log_file := FileAccess.open(log_path, FileAccess.WRITE)
	if log_file != null:
		log_file.store_string(output)
		log_file.close()

	if _extract(output, "BENCHCSV ").is_empty():
		print("FAILED (exit %d)" % exit_code)
		printerr("--- tail of %s ---" % log_path)
		var lines := output.split("\n")
		for i in range(maxi(0, lines.size() - LOG_TAIL_LINES), lines.size()):
			printerr(lines[i])
		_abort("benchmark run '%s' produced no result" % scenario)
		return ""

	print(_extract(output, "[bench] done: "))
	return output


func _parse_args() -> void:
	var requested := PackedStringArray(["@baseline"])
	_out_dir = ProjectSettings.globalize_path("res://..").simplify_path().path_join(".benchmarks")
	_device = _default_device()

	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--scenarios="):
			requested = arg.substr("--scenarios=".length()).split(",", false)
		elif arg.begins_with("--rounds="):
			_rounds = maxi(1, int(arg.substr("--rounds=".length())))
		elif arg.begins_with("--out="):
			_out_dir = arg.substr("--out=".length())
		elif arg.begins_with("--device="):
			_device = arg.substr("--device=".length())
		else:
			_abort("unknown argument '%s'" % arg)
			return

	_scenarios = _expand(requested)


# Expands @groups and drops duplicates, so @all does not measure pom_off twice.
func _expand(requested: PackedStringArray) -> PackedStringArray:
	var groups := _scenario_groups()
	var expanded := PackedStringArray()

	for name in requested:
		var entries := PackedStringArray([name])
		if name.begins_with("@"):
			if not groups.has(name):
				_abort("unknown scenario group '%s'; known groups are %s" % [
						name, ", ".join(PackedStringArray(groups.keys()))])
				return PackedStringArray()
			entries = groups[name]

		for entry in entries:
			if not expanded.has(entry):
				expanded.append(entry)

	return expanded


func _scenario_groups() -> Dictionary:
	var groups := {
		"@baseline": PackedStringArray(["baseline", "pom_off"]),
		"@clipmap": _series("clipmap_levels_%d", range(1, 11)),
		"@mesh": _series("mesh_resolution_%d", [32, 64, 128, 256, 512]),
		"@octaves": _series("noise_octaves_%d", range(1, 11)),
		"@physics": _series("physics_range_%d", [8, 64, 128, 256, 512]),
		"@pom": PackedStringArray(["pom_off", "pom_1_1", "pom_8_16", "pom_16_32", "pom_32_64", "pom_64_128"]),
	}

	var sweep := PackedStringArray()
	for key in ["@clipmap", "@mesh", "@octaves", "@physics", "@pom"]:
		sweep.append_array(groups[key])
	groups["@sweep"] = sweep

	var everything := PackedStringArray(["baseline"])
	everything.append_array(sweep)
	everything.append("stress")
	groups["@all"] = everything

	return groups


func _series(format: String, values) -> PackedStringArray:
	var names := PackedStringArray()
	for value in values:
		names.append(format % value)
	return names


func _driver() -> String:
	# Metal on macOS; do not force Vulkan on Apple silicon.
	return "metal" if OS.get_name() == "macOS" else "vulkan"


# Only Linux can be without a display server. macOS and Windows always have one and never set
# DISPLAY, so testing it there would send every run through an xvfb that does not exist.
func _needs_virtual_display() -> bool:
	if OS.get_name() != "Linux":
		return false
	return OS.get_environment("BENCH_XVFB") == "1" or OS.get_environment("DISPLAY").is_empty()


func _default_device() -> String:
	for variable in ["BENCH_DEVICE", "HOSTNAME", "COMPUTERNAME"]:
		var value := OS.get_environment(variable)
		if not value.is_empty():
			return value
	return OS.get_name().to_lower()


func _extract(output: String, prefix: String) -> String:
	for line in output.split("\n"):
		if line.begins_with(prefix):
			return line.substr(prefix.length()).strip_edges()
	return ""


func _abort(message: String) -> void:
	printerr("[runner] %s" % message)
	get_tree().quit(1)
