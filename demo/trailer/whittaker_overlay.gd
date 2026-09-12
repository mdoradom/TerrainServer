extends Control

# The biome chapter's 2D companion (see TODO_TRAILER.md T3): the Whittaker chart the plugin actually
# classifies with. Each dot is a real sample of the terrain in view, placed by its temperature and
# moisture; each rectangle is one biome layer's climate window, straight off the TerrainBiomeLayer
# resource. Dots and rectangles light up on the same cues as the biomes on the terrain, so the chart
# reads as the explanation of what the viewer is watching happen in 3D.

const TEMPERATURE_MIN := -10.0
const TEMPERATURE_MAX := 30.0
const SAMPLE_COUNT := 900
const SAMPLE_SEED := 20260911
const CHART_SIZE := Vector2(470.0, 470.0)
const CHART_MARGIN := Vector2(96.0, 0.0)
const PLOT_INSET := 52.0
const DOT_RADIUS := 2.8
const GRID_COLOR := Color(0.30, 0.82, 1.0, 1.0)
const TEXT_COLOR := Color(0.72, 0.92, 1.0)

var _biome_colors: Array[Color] = []
var _biome_names := PackedStringArray()
var _biome_rects: Array[Rect2] = []
var _samples: Array[Vector3] = [] # x = moisture, y = temperature, z = biome index (-1 = none)
var _fade := 0.0
var _reveals := PackedFloat32Array()
var _slope_reveal := 0.0
var _font: Font


func setup(p_config: TerrainConfiguration, p_terrain: Terrain3D, p_colors: Array[Color],
		p_names: PackedStringArray, p_center: Vector2, p_half_extent: float) -> void:
	_font = ThemeDB.fallback_font
	_biome_colors = p_colors
	_biome_names = p_names

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

	var layers: Array[TerrainBiomeLayer] = p_config.biome_layers
	for layer in layers:
		_biome_rects.append(Rect2(
				layer.min_moisture,
				layer.min_temperature,
				maxf(layer.max_moisture - layer.min_moisture, 0.001),
				maxf(layer.max_temperature - layer.min_temperature, 0.001)))

	_build_samples(p_terrain, p_center, p_half_extent)


# Samples the same patch of terrain the diorama shows, through the plugin's own CPU climate port, so
# the scatter matches the biome colours on screen rather than being decorative noise.
func _build_samples(p_terrain: Terrain3D, p_center: Vector2, p_half_extent: float) -> void:
	if p_terrain == null:
		return

	var rng := RandomNumberGenerator.new()
	rng.seed = SAMPLE_SEED

	for i in SAMPLE_COUNT:
		var world_xz := p_center + Vector2(
				rng.randf_range(-p_half_extent, p_half_extent),
				rng.randf_range(-p_half_extent, p_half_extent))
		var temperature: float = p_terrain.get_temperature_at(world_xz)
		var moisture: float = p_terrain.get_moisture_at(world_xz)
		var biome: TerrainBiomeLayer = p_terrain.get_biome_at(world_xz)
		var index := -1
		if biome != null:
			index = _biome_names.find(biome.biome_name)
		_samples.append(Vector3(moisture, temperature, float(index)))


func set_state(p_fade: float, p_reveals: PackedFloat32Array, p_slope_reveal: float) -> void:
	if is_equal_approx(p_fade, _fade) and p_reveals == _reveals and is_equal_approx(p_slope_reveal, _slope_reveal):
		return
	_fade = p_fade
	_reveals = p_reveals
	_slope_reveal = p_slope_reveal
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

	for i in _biome_rects.size():
		var reveal := _reveal(i)
		if reveal <= 0.001:
			continue
		var color := _biome_colors[i] if i < _biome_colors.size() else Color.WHITE
		var rect := _to_plot(plot, _biome_rects[i])
		draw_rect(rect, Color(color.r, color.g, color.b, 0.34 * reveal * _fade))
		draw_rect(rect, Color(color.r, color.g, color.b, reveal * _fade), false, 2.0)
		if i < _biome_names.size():
			draw_string(_font, rect.position + Vector2(9.0, 21.0), _biome_names[i].to_upper(),
					HORIZONTAL_ALIGNMENT_LEFT, -1, 16, Color(color.r, color.g, color.b, reveal * _fade))

	for sample in _samples:
		var index := int(sample.z)
		var reveal := _reveal(index) if index >= 0 else 0.0
		var color := Color(0.65, 0.85, 1.0, 0.55 * _fade)
		if reveal > 0.001 and index < _biome_colors.size():
			var biome_color := _biome_colors[index]
			color = Color(biome_color.r, biome_color.g, biome_color.b, (0.55 + 0.45 * reveal) * _fade)
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


func _reveal(p_index: int) -> float:
	if p_index < 0 or p_index >= _reveals.size():
		return 0.0
	return clampf(_reveals[p_index], 0.0, 1.0)


func _to_plot_point(p_plot: Rect2, p_moisture: float, p_temperature: float) -> Vector2:
	var x := clampf(p_moisture, 0.0, 1.0)
	var y := clampf((p_temperature - TEMPERATURE_MIN) / (TEMPERATURE_MAX - TEMPERATURE_MIN), 0.0, 1.0)
	return Vector2(lerpf(p_plot.position.x, p_plot.end.x, x), lerpf(p_plot.end.y, p_plot.position.y, y))


func _to_plot(p_plot: Rect2, p_rect: Rect2) -> Rect2:
	var top_left := _to_plot_point(p_plot, p_rect.position.x, p_rect.position.y + p_rect.size.y)
	var bottom_right := _to_plot_point(p_plot, p_rect.position.x + p_rect.size.x, p_rect.position.y)
	return Rect2(top_left, bottom_right - top_left)
