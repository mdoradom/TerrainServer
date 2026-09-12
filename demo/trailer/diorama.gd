extends Node3D

# The trailer's "diorama": a finite, wireframe-rendered patch of the real terrain, used by the
# build-up shot (see TODO_TRAILER.md T3). It is deliberately NOT the Terrain3D node -- the clipmap
# renderer rebuilds its whole mesh on any TerrainConfiguration edit, which rules out animating a
# noise parameter per frame, and it owns its RenderingServer material, which rules out the extra
# diagram-only uniforms the shot needs.
#
# What it is instead: clipmap-shaped geometry (a centre block plus concentric rings, each twice the
# size and half the vertex density of the last, exactly like TerrainRenderer's levels) built here in
# GDScript, drawn with a shader whose noise half is spliced straight out of the production
# .gdshader. So every height on screen is the plugin's own height, and every parameter the shot
# animates is the plugin's own parameter.

const TERRAIN_SHADER_PATH := "res://addons/terrain_server/shaders/terrain.gdshader"
const BODY_PATH := "res://trailer/diorama_body.gdshaderinc"

# The diorama renders on its own visual layer so the shot can cut between it and the real Terrain3D
# with the camera's cull mask, without tearing either one down.
const VISUAL_LAYER := 2

# Diagram colours per biome_name, keyed to the demo config's biomes. Anything unknown falls back to
# a golden-ratio hue so an edited config still reads as distinct bands.
const BIOME_COLORS := {
	"snow": Color(0.86, 0.92, 1.0),
	"ice_cliff": Color(0.56, 0.84, 0.96),
	"rock": Color(0.46, 0.45, 0.52),
	"grass": Color(0.30, 0.70, 0.26),
	"dirt": Color(0.72, 0.47, 0.22),
}
const ROCK_SLOPE_COLOR := Color(0.42, 0.40, 0.44)

var material: ShaderMaterial
var extent := 0.0

var _levels: Array[MeshInstance3D] = []
var _biome_colors: Array[Color] = []
var _biome_names: PackedStringArray = []


# Builds the geometry and the shader, and pushes p_config into the shader's terrain uniforms.
# p_size is the level-0 patch width in world units; p_resolution its quad count per side.
func setup(p_config: TerrainConfiguration, p_size: float, p_resolution: int, p_ring_levels: int) -> bool:
	var shader := _build_shader()
	if shader == null:
		return false

	material = ShaderMaterial.new()
	material.shader = shader

	extent = p_size * 0.5

	var center := Vector2(global_position.x, global_position.z)
	material.set_shader_parameter("d_center", center)
	material.set_shader_parameter("d_extent", extent)
	# Normals are sampled finer than the (deliberately coarse, readable) wireframe cell, so the
	# shaded and normal views still show the detail the noise actually has.
	material.set_shader_parameter("d_cell", clampf(p_size / float(p_resolution) * 0.35, 2.0, 12.0))

	_add_level(_build_patch(p_size, p_resolution, 0.0, true), p_size, "Level0")
	for level in range(1, p_ring_levels + 1):
		var size := p_size * pow(2.0, float(level))
		_add_level(_build_patch(size, p_resolution, 0.25, false), size, "Level%d" % level)

	apply_config(p_config)
	return true


# Mirrors TerrainRenderer::rebuild_mesh's material_set_param block for the uniforms the diorama
# actually uses. Individual values are then overridden per frame by the shot's choreography.
func apply_config(p_config: TerrainConfiguration) -> void:
	set_param("height_scale", float(p_config.height_scale))
	set_param("octaves", p_config.noise_octaves)
	set_param("base_frequency", p_config.noise_base_frequency)
	set_param("lacunarity", p_config.noise_lacunarity)
	set_param("gain", p_config.noise_gain)
	set_param("ridge_amount", p_config.noise_ridge_amount)
	set_param("ridge_offset", p_config.noise_ridge_offset)
	set_param("ridge_weight_gain", p_config.noise_ridge_weight_gain)
	set_param("ridge_crest_rounding", p_config.noise_ridge_crest_rounding)
	set_param("warp_amount", p_config.noise_warp_amount)
	set_param("warp_frequency", p_config.noise_warp_frequency)
	set_param("continent_frequency", p_config.noise_continent_frequency)
	set_param("continent_influence", p_config.noise_continent_influence)
	set_param("continent_contrast", p_config.noise_continent_contrast)
	set_param("continent_elevation", p_config.noise_continent_elevation)
	set_param("continent_sea_level", p_config.noise_continent_sea_level)
	set_param("relief_floor", p_config.noise_relief_floor)
	set_param("redistribution", p_config.noise_redistribution)

	set_param("temperature_frequency", p_config.temperature_frequency)
	set_param("temperature_offset", p_config.temperature_offset)
	set_param("temperature_noise_influence", p_config.temperature_noise_influence)
	set_param("temperature_altitude_reference", p_config.temperature_altitude_reference)
	set_param("moisture_frequency", p_config.moisture_frequency)
	set_param("moisture_offset", p_config.moisture_offset)

	_apply_biomes(p_config)


func set_param(p_name: String, p_value: Variant) -> void:
	material.set_shader_parameter(p_name, p_value)


func set_level_intensity(p_level: int, p_intensity: float) -> void:
	if p_level < 0 or p_level >= _levels.size():
		return
	_levels[p_level].set_instance_shader_parameter("d_level_intensity", p_intensity)


func set_ring_levels_visible(p_visible: bool) -> void:
	for level in range(1, _levels.size()):
		_levels[level].visible = p_visible


func level_count() -> int:
	return _levels.size()


func biome_colors() -> Array[Color]:
	return _biome_colors


func biome_names() -> PackedStringArray:
	return _biome_names


# Terrain uniforms + noise functions come from the production shader; vertex()/fragment() from the
# trailer body. See diorama_body.gdshaderinc's header for why it is spliced rather than included.
func _build_shader() -> Shader:
	var terrain_source := FileAccess.get_file_as_string(TERRAIN_SHADER_PATH)
	if terrain_source.is_empty():
		printerr("[diorama] could not read %s" % TERRAIN_SHADER_PATH)
		return null

	var body := FileAccess.get_file_as_string(BODY_PATH)
	if body.is_empty():
		printerr("[diorama] could not read %s" % BODY_PATH)
		return null

	var render_mode_end := terrain_source.find(";", terrain_source.find("render_mode"))
	var vertex_start := terrain_source.find("void vertex()")
	if render_mode_end < 0 or vertex_start < 0 or vertex_start <= render_mode_end:
		printerr("[diorama] %s no longer has the expected `render_mode ...;` ... `void vertex()` " % TERRAIN_SHADER_PATH +
				"layout the diorama splices between; update diorama.gd")
		return null

	var prefix := terrain_source.substr(render_mode_end + 1, vertex_start - render_mode_end - 1)

	var shader := Shader.new()
	shader.code = "shader_type spatial;\nrender_mode unshaded, cull_disabled;\n" + prefix + body
	return shader


func _apply_biomes(p_config: TerrainConfiguration) -> void:
	var layers: Array[TerrainBiomeLayer] = p_config.biome_layers
	var count: int = layers.size()

	var min_temperature := PackedFloat32Array()
	var max_temperature := PackedFloat32Array()
	var min_moisture := PackedFloat32Array()
	var max_moisture := PackedFloat32Array()
	var blend_softness := PackedFloat32Array()
	var slope_threshold := PackedFloat32Array()
	var slope_blend_range := PackedFloat32Array()
	var colors := PackedVector3Array()
	var reveal := PackedFloat32Array()

	_biome_colors.clear()
	_biome_names.clear()

	var rock_layer: TerrainSlopeLayer = p_config.rock_layer

	for i in count:
		var layer: TerrainBiomeLayer = layers[i]
		min_temperature.append(layer.min_temperature)
		max_temperature.append(layer.max_temperature)
		min_moisture.append(layer.min_moisture)
		max_moisture.append(layer.max_moisture)
		blend_softness.append(layer.blend_softness)

		# Same precedence as TerrainRenderer: a layer's own slope values only win when it sets
		# slope_override, otherwise the (biome or global) slope layer's values are used.
		var slope: TerrainSlopeLayer = layer.slope_layer if layer.slope_layer != null else rock_layer
		if layer.slope_override or slope == null:
			slope_threshold.append(layer.slope_threshold)
			slope_blend_range.append(layer.slope_blend_range)
		else:
			slope_threshold.append(slope.slope_threshold)
			slope_blend_range.append(slope.slope_blend_range)

		var color := _biome_color(layer.biome_name, i)
		_biome_colors.append(color)
		_biome_names.append(layer.biome_name)
		var linear := color.srgb_to_linear()
		colors.append(Vector3(linear.r, linear.g, linear.b))
		reveal.append(0.0)

	set_param("biome_layer_count", count)
	set_param("biome_min_temperature", min_temperature)
	set_param("biome_max_temperature", max_temperature)
	set_param("biome_min_moisture", min_moisture)
	set_param("biome_max_moisture", max_moisture)
	set_param("biome_blend_softness", blend_softness)
	set_param("biome_slope_threshold", slope_threshold)
	set_param("biome_slope_blend_range", slope_blend_range)
	set_param("d_biome_color", colors)
	set_param("d_biome_reveal", reveal)

	var rock_linear := ROCK_SLOPE_COLOR.srgb_to_linear()
	set_param("d_rock_color", Vector3(rock_linear.r, rock_linear.g, rock_linear.b))


func _biome_color(p_name: String, p_index: int) -> Color:
	if BIOME_COLORS.has(p_name):
		return BIOME_COLORS[p_name]
	return Color.from_hsv(fmod(float(p_index) * 0.61803398875, 1.0), 0.6, 0.95)


func _add_level(p_mesh: ArrayMesh, p_size: float, p_name: String) -> void:
	var instance := MeshInstance3D.new()
	instance.name = p_name
	instance.mesh = p_mesh
	instance.material_override = material
	instance.layers = VISUAL_LAYER
	instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	# The mesh is flat until the vertex shader displaces it, so without a custom AABB the whole
	# patch is culled the moment the camera looks at where the mountains ended up.
	instance.custom_aabb = AABB(
			Vector3(-p_size * 0.5, -4096.0, -p_size * 0.5),
			Vector3(p_size, 8192.0, p_size))
	add_child(instance)
	_levels.append(instance)


# One clipmap level: a p_resolution x p_resolution grid of quads spanning p_size, optionally with a
# square hole of p_inner_fraction * p_size half-size punched out of the middle (that is where the
# finer level sits), and optionally with skirt walls hanging off its outer border.
#
# Vertices are unindexed and carry barycentric coordinates in COLOR so the fragment shader can draw
# the wireframe itself -- Viewport.DEBUG_DRAW_WIREFRAME cannot be coloured, faded, or pulsed to the
# music. The third barycentric axis is always assigned to the vertex opposite the quad's diagonal,
# which lets the shader draw that shared edge fainter than the quad edges.
static func _build_patch(p_size: float, p_resolution: int, p_inner_fraction: float, p_skirt: bool) -> ArrayMesh:
	var vertices := PackedVector3Array()
	var colors := PackedColorArray()
	var uvs := PackedVector2Array()

	var step := p_size / float(p_resolution)
	var half := p_size * 0.5
	var inner := p_size * p_inner_fraction

	for z in p_resolution:
		for x in p_resolution:
			var x0 := -half + float(x) * step
			var z0 := -half + float(z) * step
			var x1 := x0 + step
			var z1 := z0 + step

			if inner > 0.0 and absf(x0 + step * 0.5) < inner and absf(z0 + step * 0.5) < inner:
				continue

			var tl := Vector3(x0, 0.0, z0)
			var tr := Vector3(x1, 0.0, z0)
			var bl := Vector3(x0, 0.0, z1)
			var br := Vector3(x1, 0.0, z1)

			if (x + z) % 2 == 0:
				# Diagonal tl-br: the opposite vertex of each triangle takes the third axis.
				_add_triangle(vertices, colors, uvs, tl, tr, br, 1, Vector2.ZERO)
				_add_triangle(vertices, colors, uvs, tl, br, bl, 2, Vector2.ZERO)
			else:
				# Diagonal tr-bl.
				_add_triangle(vertices, colors, uvs, tl, tr, bl, 0, Vector2.ZERO)
				_add_triangle(vertices, colors, uvs, tr, br, bl, 1, Vector2.ZERO)

	if p_skirt:
		for i in p_resolution:
			var a := -half + float(i) * step
			var b := a + step
			_add_skirt_quad(vertices, colors, uvs, Vector3(a, 0.0, -half), Vector3(b, 0.0, -half))
			_add_skirt_quad(vertices, colors, uvs, Vector3(b, 0.0, half), Vector3(a, 0.0, half))
			_add_skirt_quad(vertices, colors, uvs, Vector3(-half, 0.0, b), Vector3(-half, 0.0, a))
			_add_skirt_quad(vertices, colors, uvs, Vector3(half, 0.0, a), Vector3(half, 0.0, b))

	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_COLOR] = colors
	arrays[Mesh.ARRAY_TEX_UV] = uvs

	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return mesh


# p_diagonal_opposite selects which of the three vertices is opposite the quad diagonal, i.e. which
# one gets barycentric z = 1 (the edge where z = 0 is then the diagonal itself).
static func _add_triangle(p_vertices: PackedVector3Array, p_colors: PackedColorArray, p_uvs: PackedVector2Array,
		p_a: Vector3, p_b: Vector3, p_c: Vector3, p_diagonal_opposite: int, p_uv: Vector2) -> void:
	const AXES := [
		[Color(0.0, 0.0, 1.0), Color(1.0, 0.0, 0.0), Color(0.0, 1.0, 0.0)],
		[Color(1.0, 0.0, 0.0), Color(0.0, 0.0, 1.0), Color(0.0, 1.0, 0.0)],
		[Color(1.0, 0.0, 0.0), Color(0.0, 1.0, 0.0), Color(0.0, 0.0, 1.0)],
	]
	var axes: Array = AXES[p_diagonal_opposite]
	p_vertices.append(p_a)
	p_vertices.append(p_b)
	p_vertices.append(p_c)
	p_colors.append(axes[0])
	p_colors.append(axes[1])
	p_colors.append(axes[2])
	p_uvs.append(p_uv)
	p_uvs.append(p_uv)
	p_uvs.append(p_uv)


# A wall quad along one border segment. UV.y = 1 marks a skirt fragment, UV.x = 1 marks the lower
# pair of vertices, which the vertex shader drops by d_skirt_depth.
static func _add_skirt_quad(p_vertices: PackedVector3Array, p_colors: PackedColorArray, p_uvs: PackedVector2Array,
		p_a: Vector3, p_b: Vector3) -> void:
	const TOP := Vector2(0.0, 1.0)
	const BOTTOM := Vector2(1.0, 1.0)
	p_vertices.append_array([p_a, p_b, p_b])
	p_colors.append_array([Color(1.0, 0.0, 0.0), Color(0.0, 0.0, 1.0), Color(0.0, 1.0, 0.0)])
	p_uvs.append_array([TOP, TOP, BOTTOM])
	p_vertices.append_array([p_a, p_b, p_a])
	p_colors.append_array([Color(1.0, 0.0, 0.0), Color(0.0, 1.0, 0.0), Color(0.0, 0.0, 1.0)])
	p_uvs.append_array([TOP, BOTTOM, BOTTOM])
