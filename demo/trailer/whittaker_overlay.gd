extends Control

# The biome chapter's 2D companion (see TODO_TRAILER.md T3): the Whittaker chart the plugin actually
# classifies with. Each dot is a real sample of the terrain in view, placed by its temperature and
# moisture; each rectangle is one biome layer's climate window, straight off the TerrainBiomeLayer
# resource. Dots and rectangles light up on the same cues as the biomes on the terrain, so the chart
# reads as the explanation of what the viewer is watching happen in 3D.
#
# The chapter reveals biomes by stepping the shader's `biome_layer_count`, so "revealed" here means
# exactly the same thing it means on the terrain: the first N layers are the ones the blend is
# allowed to choose from. Each sample is pre-classified for every N with the same Whittaker weights
# fragment() uses, so a dot changes colour on the frame its biome starts winning on screen.

const TEMPERATURE_MIN := -10.0
const TEMPERATURE_MAX := 30.0
# fragment()'s own floors, so this classification cannot disagree with the shader's.
const WEIGHT_FLOOR := 0.0001
const TEMP_SOFTNESS_EPSILON := 0.01
const MOIST_SOFTNESS_EPSILON := 0.0005
const SAMPLE_COUNT := 900
const SAMPLE_SEED := 20260911
const CHART_SIZE := Vector2(470.0, 470.0)
const CHART_MARGIN := Vector2(96.0, 0.0)
const PLOT_INSET := 52.0
const DOT_RADIUS := 2.8
const GRID_COLOR := Color(0.30, 0.82, 1.0, 1.0)
const TEXT_COLOR := Color(0.72, 0.92, 1.0)

var _layers: Array[TerrainBiomeLayer] = []
var _biome_colors: Array[Color] = []
var _biome_names := PackedStringArray()
var _biome_rects: Array[Rect2] = []
var _samples: Array[Vector2] = [] # x = moisture, y = temperature
# Per sample, the winning biome index for each visible layer count 1..N. Indexed
# [sample * layer_count + (count - 1)].
var _winners := PackedByteArray()
var _fade := 0.0
var _visible_count := 0
var _font: Font


func setup(p_config: TerrainConfiguration, p_terrain: Terrain3D, p_center: Vector2,
		p_half_extent: float) -> void:
	_font = ThemeDB.fallback_font

	# Anchored to the right edge, vertically centred, so the panel lands in the same place at any
	# recording resolution.
	anchor_left = 1.0
	anchor_right = 1.0
	anchor_top = 0.5
	anchor_bottom = 0.5
	offset_left = -CHART_SIZE.x - CHART_MARGIN.x
	offset_right = -CHART_MARGIN.x
	offset_top = -CHART_SIZE.y * 0.5
	offset_bottom = CHART_SIZE.y * 0.5

	_layers.assign(p_config.biome_layers)
	for layer in _layers:
		_biome_names.append(layer.biome_name)
		_biome_colors.append(debug_id_color(_biome_colors.size()))
		_biome_rects.append(Rect2(
				layer.min_moisture,
				layer.min_temperature,
				maxf(layer.max_moisture - layer.min_moisture, 0.001),
				maxf(layer.max_temperature - layer.min_temperature, 0.001)))

	_build_samples(p_terrain, p_center, p_half_extent)


# terrain.gdshader's debug_id_color(), so a dot is drawn in exactly the colour the biome-id view
# gives that layer on the terrain behind it.
static func debug_id_color(p_index: int) -> Color:
	return Color.from_hsv(fmod(float(p_index) * 0.61803398875, 1.0), 1.0, 1.0)


# Samples the patch of terrain the shot shows, through the plugin's own CPU climate port, so the
# scatter matches what is on screen rather than being decorative noise.
func _build_samples(p_terrain: Terrain3D, p_center: Vector2, p_half_extent: float) -> void:
	if p_terrain == null or _layers.is_empty():
		return

	var rng := RandomNumberGenerator.new()
	rng.seed = SAMPLE_SEED
	_winners.resize(SAMPLE_COUNT * _layers.size())

	for i in SAMPLE_COUNT:
		var world_xz := p_center + Vector2(
				rng.randf_range(-p_half_extent, p_half_extent),
				rng.randf_range(-p_half_extent, p_half_extent))
		var temperature: float = p_terrain.get_temperature_at(world_xz)
		var moisture: float = p_terrain.get_moisture_at(world_xz)
		_samples.append(Vector2(moisture, temperature))
		for count in range(1, _layers.size() + 1):
			_winners[i * _layers.size() + count - 1] = _classify(temperature, moisture, count)


# The index of the heaviest of the first p_count biome layers at this climate, by the same weights
# fragment() computes. Mirrored rather than asked of the plugin because get_biome_at() always
# classifies against every layer, and the reveal's whole point is that it does not.
func _classify(p_temperature: float, p_moisture: float, p_count: int) -> int:
	var best := 0
	var best_weight := -1.0
	for i in p_count:
		var layer: TerrainBiomeLayer = _layers[i]
		var softness_t := maxf(TEMP_SOFTNESS_EPSILON,
				(layer.max_temperature - layer.min_temperature) * layer.blend_softness)
		var softness_m := maxf(MOIST_SOFTNESS_EPSILON,
				(layer.max_moisture - layer.min_moisture) * layer.blend_softness)
		var wt := smoothstep(layer.min_temperature - softness_t, layer.min_temperature, p_temperature) * \
				(1.0 - smoothstep(layer.max_temperature, layer.max_temperature + softness_t, p_temperature))
		var wm := smoothstep(layer.min_moisture - softness_m, layer.min_moisture, p_moisture) * \
				(1.0 - smoothstep(layer.max_moisture, layer.max_moisture + softness_m, p_moisture))
		var weight := maxf(wt * wm, WEIGHT_FLOOR)
		if weight > best_weight:
			best_weight = weight
			best = i
	return best


# p_visible_count is the shader's biome_layer_count: the first N layers are in play.
func set_state(p_fade: float, p_visible_count: int) -> void:
	if is_equal_approx(p_fade, _fade) and p_visible_count == _visible_count:
		return
	_fade = p_fade
	_visible_count = p_visible_count
	queue_redraw()


func _draw() -> void:
	if _fade <= 0.001:
		return

	var plot := Rect2(Vector2(PLOT_INSET, PLOT_INSET * 0.5), size - Vector2(PLOT_INSET * 1.3, PLOT_INSET * 1.6))

	draw_rect(Rect2(Vector2.ZERO, size), Color(0.02, 0.04, 0.06, 0.96 * _fade))
	draw_rect(Rect2(Vector2.ZERO, size), Color(GRID_COLOR.r, GRID_COLOR.g, GRID_COLOR.b, 0.5 * _fade), false, 1.5)
	draw_rect(plot, Color(GRID_COLOR.r, GRID_COLOR.g, GRID_COLOR.b, 0.85 * _fade), false, 2.0)

	for i in 5:
		var f := float(i) / 4.0
		draw_line(Vector2(plot.position.x, lerpf(plot.position.y, plot.end.y, f)),
				Vector2(plot.end.x, lerpf(plot.position.y, plot.end.y, f)),
				Color(GRID_COLOR.r, GRID_COLOR.g, GRID_COLOR.b, 0.3 * _fade), 1.0)
		draw_line(Vector2(lerpf(plot.position.x, plot.end.x, f), plot.position.y),
				Vector2(lerpf(plot.position.x, plot.end.x, f), plot.end.y),
				Color(GRID_COLOR.r, GRID_COLOR.g, GRID_COLOR.b, 0.3 * _fade), 1.0)

	for i in mini(_visible_count, _biome_rects.size()):
		var color := _biome_colors[i]
		var rect := _to_plot(plot, _biome_rects[i])
		draw_rect(rect, Color(color.r, color.g, color.b, 0.34 * _fade))
		draw_rect(rect, Color(color.r, color.g, color.b, _fade), false, 2.0)
		draw_string(_font, rect.position + Vector2(9.0, 21.0), _biome_names[i].to_upper(),
				HORIZONTAL_ALIGNMENT_LEFT, -1, 16, Color(color.r, color.g, color.b, _fade))

	for i in _samples.size():
		var sample := _samples[i]
		var index := _winner(i)
		var color := Color(0.65, 0.85, 1.0, 0.55 * _fade)
		if index >= 0:
			var biome_color := _biome_colors[index]
			color = Color(biome_color.r, biome_color.g, biome_color.b, _fade)
		draw_circle(_to_plot_point(plot, sample.x, sample.y), DOT_RADIUS, color)

	var text := Color(TEXT_COLOR.r, TEXT_COLOR.g, TEXT_COLOR.b, _fade)
	draw_string(_font, Vector2(PLOT_INSET, 28.0), "WHITTAKER CLASSIFICATION",
			HORIZONTAL_ALIGNMENT_LEFT, -1, 17, text)
	draw_string(_font, Vector2(plot.position.x, plot.end.y + 28.0), "MOISTURE  0.0",
			HORIZONTAL_ALIGNMENT_LEFT, -1, 15, text)
	draw_string(_font, Vector2(plot.end.x - 26.0, plot.end.y + 28.0), "1.0",
			HORIZONTAL_ALIGNMENT_LEFT, -1, 15, text)
	draw_string(_font, Vector2(8.0, plot.position.y + 12.0), "%d C" % int(TEMPERATURE_MAX),
			HORIZONTAL_ALIGNMENT_LEFT, -1, 15, text)
	draw_string(_font, Vector2(8.0, plot.end.y), "%d C" % int(TEMPERATURE_MIN),
			HORIZONTAL_ALIGNMENT_LEFT, -1, 15, text)


# Which layer this sample classifies as at the current visible count, or -1 before any layer is in
# play (drawn pale then, since the terrain has no biome colours yet either).
func _winner(p_sample: int) -> int:
	var count := mini(_visible_count, _layers.size())
	if count <= 0 or _winners.is_empty():
		return -1
	return _winners[p_sample * _layers.size() + count - 1]


func _to_plot_point(p_plot: Rect2, p_moisture: float, p_temperature: float) -> Vector2:
	var x := clampf(p_moisture, 0.0, 1.0)
	var y := clampf((p_temperature - TEMPERATURE_MIN) / (TEMPERATURE_MAX - TEMPERATURE_MIN), 0.0, 1.0)
	return Vector2(lerpf(p_plot.position.x, p_plot.end.x, x), lerpf(p_plot.end.y, p_plot.position.y, y))


func _to_plot(p_plot: Rect2, p_rect: Rect2) -> Rect2:
	var top_left := _to_plot_point(p_plot, p_rect.position.x, p_rect.position.y + p_rect.size.y)
	var bottom_right := _to_plot_point(p_plot, p_rect.position.x + p_rect.size.x, p_rect.position.y)
	return Rect2(top_left, bottom_right - top_left)
