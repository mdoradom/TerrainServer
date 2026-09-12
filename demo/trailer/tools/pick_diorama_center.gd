extends SceneTree

# Picks the world position the trailer's diorama should sit on (trailer_rig.gd's DIORAMA_CENTER).
#
# The terrain is infinite and homogeneous in feel but not in content: temperature and moisture are
# their own noise fields (wavelengths on the order of a single diorama), so most spots land inside
# one climate band and render as one biome. The biome beat needs a patch that straddles several.
# This scores candidate centres on real samples and prints the best ones.
#
#   godot --path demo --headless --script res://trailer/tools/pick_diorama_center.gd
#
# Terrain3D is used outside the scene tree on purpose: its climate/height sampling is the CPU port
# in TerrainNoise, which only needs the configuration, so no rendering or physics has to start.

const CONFIG_PATH := "res://addons/terrain_server/assets/terrains/demo_terrain_configuration.tres"
const DIORAMA_SIZE := 1536.0
const SEARCH_RADIUS := 6144.0
const SEARCH_STEP := 512.0
const SAMPLES_PER_CANDIDATE := 24 # per axis, so this squared in total
const TOP_RESULTS := 8


func _initialize() -> void:
	var config: TerrainConfiguration = load(CONFIG_PATH)
	if config == null:
		printerr("could not load %s" % CONFIG_PATH)
		quit(1)
		return

	var terrain := Terrain3D.new()
	terrain.configuration = config

	var results: Array[Dictionary] = []
	var x := -SEARCH_RADIUS
	while x <= SEARCH_RADIUS:
		var z := -SEARCH_RADIUS
		while z <= SEARCH_RADIUS:
			results.append(_score(terrain, Vector2(x, z)))
			z += SEARCH_STEP
		x += SEARCH_STEP

	results.sort_custom(func(a, b): return a["score"] > b["score"])

	print("Scored %d candidates (%.0f-unit patches, %d samples each)" % [
			results.size(), DIORAMA_SIZE, SAMPLES_PER_CANDIDATE * SAMPLES_PER_CANDIDATE])
	print("score  center                 biomes  height range     temperature range   mix")
	for i in mini(TOP_RESULTS, results.size()):
		var r: Dictionary = results[i]
		print("%5.3f  (%7.0f, %7.0f)  %d       %6.0f..%-6.0f  %5.1f..%-5.1f C     %s" % [
				r["score"], r["center"].x, r["center"].y, r["biomes"],
				r["height_min"], r["height_max"], r["temp_min"], r["temp_max"], r["mix"]])

	terrain.free()
	quit()


# Rewards a patch that shows several biomes in balance (Shannon evenness over the biome histogram)
# and has enough vertical range to read as mountains rather than as a plain.
func _score(p_terrain: Terrain3D, p_center: Vector2) -> Dictionary:
	var half := DIORAMA_SIZE * 0.5
	var histogram := {}
	var height_min := INF
	var height_max := -INF
	var temp_min := INF
	var temp_max := -INF
	var total := 0

	for ix in SAMPLES_PER_CANDIDATE:
		for iz in SAMPLES_PER_CANDIDATE:
			var world_xz := p_center + Vector2(
					lerpf(-half, half, float(ix) / float(SAMPLES_PER_CANDIDATE - 1)),
					lerpf(-half, half, float(iz) / float(SAMPLES_PER_CANDIDATE - 1)))

			var height := p_terrain.get_height_at(world_xz)
			height_min = minf(height_min, height)
			height_max = maxf(height_max, height)

			var temperature := p_terrain.get_temperature_at(world_xz)
			temp_min = minf(temp_min, temperature)
			temp_max = maxf(temp_max, temperature)

			var biome: TerrainBiomeLayer = p_terrain.get_biome_at(world_xz)
			var name: String = biome.biome_name if biome != null else "none"
			histogram[name] = int(histogram.get(name, 0)) + 1
			total += 1

	var evenness := 0.0
	for count in histogram.values():
		var p := float(count) / float(total)
		if p > 0.0:
			evenness -= p * log(p)
	# Normalised against a perfectly even split of the biomes actually present, then scaled by how
	# many there are: three balanced biomes should beat two balanced ones.
	if histogram.size() > 1:
		evenness /= log(float(histogram.size()))
	var diversity := evenness * (float(histogram.size()) / 4.0)

	var relief := clampf((height_max - height_min) / 400.0, 0.0, 1.0)

	var mix := ""
	for name in histogram:
		mix += "%s:%d%% " % [name, roundi(float(histogram[name]) / float(total) * 100.0)]

	return {
		"center": p_center,
		"score": diversity * 0.75 + relief * 0.25,
		"biomes": histogram.size(),
		"height_min": height_min,
		"height_max": height_max,
		"temp_min": temp_min,
		"temp_max": temp_max,
		"mix": mix.strip_edges(),
	}
