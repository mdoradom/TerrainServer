shader_type spatial;
render_mode cull_back;

// Physics parameters
uniform float height_scale = 100.0;
uniform float resolution = 64.0;
uniform float morph_band_start = 0.33;

// Noise parameters
uniform int octaves = 5;
uniform float base_frequency = 0.002;
uniform float lacunarity = 2.0;
uniform float gain = 0.5;

uniform float ridge_amount = 0.0;
uniform float ridge_offset = 1.0;
uniform float ridge_weight_gain = 1.5;
uniform float ridge_crest_rounding = 0.2;

uniform float warp_amount = 0.0;
uniform float warp_frequency = 0.001;

uniform float continent_frequency = 0.00025;
uniform float continent_influence = 0.0;
uniform float continent_contrast = 0.2;
uniform float continent_elevation = 0.0;
uniform float continent_sea_level = 0.4;
uniform float relief_floor = 1.0;

uniform float redistribution = 1.0;

uniform float temperature_frequency = 0.0004;
uniform vec2 temperature_offset = vec2(10000.0, -6000.0);
uniform float temperature_noise_influence = 0.4;
uniform float temperature_altitude_reference = 10.0;

uniform float moisture_frequency = 0.0006;
uniform vec2 moisture_offset = vec2(-4000.0, 9000.0);

uniform sampler2DArray biome_albedo_textures : source_color;
uniform sampler2DArray biome_normal_textures : hint_normal;
uniform sampler2DArray biome_roughness_textures;
uniform sampler2DArray biome_height_textures;
uniform sampler2DArray biome_ao_textures;
uniform int biome_layer_count = 0;

uniform float biome_min_temperature[32];
uniform float biome_max_temperature[32];
uniform float biome_min_moisture[32];
uniform float biome_max_moisture[32];
uniform float biome_uv_scale[32];
uniform float biome_pom_depth[32];
uniform float biome_blend_softness[32];
uniform float biome_slope_threshold[32];
uniform float biome_slope_blend_range[32];
uniform int biome_rock_index[32];
uniform float biome_rock_uv_scale[32];
uniform float biome_rock_pom_depth[32];

uniform sampler2DArray rock_albedo_textures : source_color;
uniform sampler2DArray rock_normal_textures : hint_normal;
uniform sampler2DArray rock_roughness_textures;
uniform sampler2DArray rock_height_textures;
uniform sampler2DArray rock_ao_textures;
uniform bool rock_layer_enabled = false;
uniform int rock_index = 0;
uniform float rock_slope_threshold = 0.6;
uniform float rock_slope_blend_range = 0.15;
uniform float rock_uv_scale = 0.1;
uniform float rock_pom_depth = 0.0;

uniform int pom_min_steps = 8;
uniform int pom_max_steps = 32;
uniform float pom_fade_start = 40.0;
uniform float pom_fade_end = 120.0;
uniform float triplanar_sharpness = 4.0;

// ---------------------------------------------------------------------------------------------
// Diagram overlay (tooling only -- the editor's debug views and the showreel trailer).
//
// Every uniform below is neutral at its default, so a material that never touches them renders
// exactly as it did before this block existed: both views are the shaded render, there is no
// wipe, and the wireframe and the reveal are off. The whole block sits behind one
// uniform-coherent branch at the end of fragment(), so it costs nothing while it is off.
//
// Views: 0 = the shaded render, 1 = the height field as grayscale, 2 = biome id as a flat color
// (splatmap look), 3 = the geometric normal as RGB, 4 = temperature, 5 = moisture, 6 = clay (flat
// albedo, real lighting), 7 = blank (unlit black -- what the wireframe is drawn on).
//
// Only view 0 shows the textured surface. Every other view describes the terrain the height field
// defines, so they all shade (or display) the geometric normal, with no biome or rock normal map,
// roughness or AO in the way.
//
// One wipe front carries three things: a change of view, a biome arriving (debug_biome_count),
// and the wireframe going out (debug_wire_opacity_b). All are optional and independent, and the
// front only exists while one of them is in play.
uniform int debug_view = 0;
uniform int debug_view_b = 0;
// Wipes debug_view across to debug_view_b along debug_wipe_dir: 0 = all A, 1 = all B. The front
// travels in a straight line from debug_wipe_start to debug_wipe_end (world-space distance along
// debug_wipe_dir, measured from debug_center), so a chapter picks exactly where it starts and ends
// rather than the front always spanning the clipmap's own outermost edge.
uniform float debug_wipe = 0.0;
uniform vec2 debug_wipe_dir = vec2(1.0, 0.0);
uniform float debug_wipe_start = -1024.0;
uniform float debug_wipe_end = 1024.0;
uniform float debug_wipe_glow = 1.0;

// The same wipe front, applied to how many biome layers the Whittaker blend may choose from:
// fragments behind it classify against the first debug_biome_count layers, ahead of it against
// the first debug_biome_count_b. That lets a biome arrive on a sweeping line rather than popping
// in everywhere at once. Negative (the default) leaves biome_layer_count alone.
uniform int debug_biome_count = -1;
uniform int debug_biome_count_b = -1;
// Scales the whole diagram, so a chapter can dim down to black without touching the wireframe.
uniform float debug_fill = 1.0;

// The patch the reveal is measured against, in world XZ. The wipe is measured from debug_center
// too, but along debug_wipe_dir out to debug_wipe_start/debug_wipe_end above rather than this.
uniform vec2 debug_center = vec2(0.0);

// Chebyshev radius around debug_center outside which fragments are dropped, so the terrain can
// draw itself outward from its centre. Negative disables it.
uniform float debug_reveal_radius = -1.0;
uniform float debug_reveal_edge = 0.0;

// Rings expanding out of debug_center on a beat -- raindrops hitting still water. A fixed number
// of independent slots rather than one, so a new ripple layers on top of an older one still fading
// instead of replacing it. A slot with negative radius or zero strength (the defaults) is off.
// Godot's shader uniform arrays cannot carry a default value, so these read as all-zero (radius 0,
// strength 0) until the trailer first pushes real slot data -- still off, since debug_ripple()
// requires strength > 0.
const int DEBUG_RIPPLE_MAX = 4;
uniform float debug_ripple_radius[DEBUG_RIPPLE_MAX];
uniform float debug_ripple_strength[DEBUG_RIPPLE_MAX];
uniform float debug_ripple_width = 70.0;
uniform float debug_ripple_lift = 0.0;

// Wireframe, measured on the clipmap mesh's own UV grid, so the lines are the real quad edges and
// every clipmap level shows its own cell size. Widths are in pixels, and a grid fades out once
// its on-screen spacing closes on debug_wire_min_spacing -- without that, a high mesh_resolution
// seen from a distance fills in to a solid wash instead of reading as a grid.
uniform float debug_wire_opacity = 0.0;
// The wireframe's opacity on the far side of the wipe front, so the lattice can ride the same
// sweep the views do instead of switching on or off everywhere at once: fragments behind the front
// fade to this, fragments ahead of it keep debug_wire_opacity. Negative (the default) means "the
// same on both sides", which is what leaves the front carrying nothing but a view or biome change.
uniform float debug_wire_opacity_b = -1.0;
// Linear, not sRGB: the trailer sets it through RenderingServer::material_set_param, where a
// source_color hint would not convert anything anyway.
uniform vec3 debug_wire_color = vec3(0.075, 0.64, 1.0);
uniform float debug_wire_intensity = 1.0;
uniform float debug_wire_width = 1.0;
uniform float debug_wire_diagonal = 0.0;
uniform float debug_wire_min_spacing = 7.0;
// Every nth cell line also drawn as a coarser grid, which survives at distances where the fine
// one has already faded. 1 or less turns it off.
uniform int debug_wire_major = 8;

uniform float debug_vignette = 0.0;

// Sum of every active ripple slot's Gaussian band at p_distance: 0 outside a ring's width, peaking
// at its strength on it. Slots are independent, so overlapping rings add rather than replace.
float debug_ripple(float p_distance) {
	float total = 0.0;
	for (int i = 0; i < DEBUG_RIPPLE_MAX; i++) {
		if (debug_ripple_strength[i] <= 0.0 || debug_ripple_radius[i] < 0.0) {
			continue;
		}
		float x = (p_distance - debug_ripple_radius[i]) / max(debug_ripple_width, 0.001);
		total += debug_ripple_strength[i] * exp(-x * x);
	}
	return total;
}

varying vec2 v_world_xz;
varying vec3 v_normal_ws;
varying float v_height;

const float NOISE_MAX_AMPLITUDE = 0.70710678;

const vec2 WARP_OFFSET_X = vec2(12700.0, -8300.0);
const vec2 WARP_OFFSET_Y = vec2(-9700.0, 15100.0);
const vec2 CONTINENT_OFFSET = vec2(-21300.0, 13700.0);

const mat2 OCTAVE_ROTATION = mat2(vec2(0.8, -0.6), vec2(0.6, 0.8));

const vec2 GRADIENTS[8] = vec2[8](
	vec2(1.0, 0.0), vec2(0.70710678, 0.70710678),
	vec2(0.0, 1.0), vec2(-0.70710678, 0.70710678),
	vec2(-1.0, 0.0), vec2(-0.70710678, -0.70710678),
	vec2(0.0, -1.0), vec2(0.70710678, -0.70710678));

uvec2 pcg2d(uvec2 v) {
	v = v * 1664525u + 1013904223u;
	v.x += v.y * 1664525u;
	v.y += v.x * 1664525u;
	v = v ^ (v >> 16u);
	v.x += v.y * 1664525u;
	v.y += v.x * 1664525u;
	v = v ^ (v >> 16u);
	return v;
}

// Hash function to generate a pseudo-random gradient vector based on a lattice cell
vec2 hash(ivec2 p) {
	uvec2 h = pcg2d(uvec2(p));
	return GRADIENTS[h.x % 8u];
}

// Perlin Noise 2D
float noise(in vec2 p) {
	vec2 i = floor(p);
	vec2 f = p - i;

	// Quintic curve
	vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

	ivec2 c = ivec2(i);

	return mix(mix(dot(hash(c + ivec2(0, 0)), f - vec2(0.0, 0.0)),
		dot(hash(c + ivec2(1, 0)), f - vec2(1.0, 0.0)), u.x),
		mix(dot(hash(c + ivec2(0, 1)), f - vec2(0.0, 1.0)),
			dot(hash(c + ivec2(1, 1)), f - vec2(1.0, 1.0)), u.x), u.y);
}

float ridge_fold(float p_value) {
	if (ridge_crest_rounding <= 0.0) {
		return abs(p_value);
	}
	return sqrt(p_value * p_value + ridge_crest_rounding * ridge_crest_rounding);
}

float relief_at(vec2 p, float p_ridge_blend) {
	vec2 q = p * base_frequency;
	float amplitude = 1.0;
	float max_amplitude = 0.0;

	float fbm = 0.0;
	float ridged = 0.0;
	float weight = 1.0;

	for(int i = 0; i < octaves; i++) {
		float n = noise(q);

		fbm += n * amplitude;

		if (p_ridge_blend > 0.0) {
			float signal = ridge_offset - ridge_fold(n / NOISE_MAX_AMPLITUDE);
			signal *= signal;
			ridged += signal * weight * amplitude;
			weight = clamp(signal * ridge_weight_gain, 0.0, 1.0);
		}

		max_amplitude += amplitude;
		amplitude *= gain;
		q = (OCTAVE_ROTATION * q) * lacunarity;
	}

	fbm /= max_amplitude;

	if (p_ridge_blend <= 0.0) {
		return fbm;
	}

	float peak = max(ridge_offset - ridge_crest_rounding, 0.01);
	ridged = (ridged / (max_amplitude * peak * peak)) * 2.0 - 1.0;
	return mix(fbm, ridged * NOISE_MAX_AMPLITUDE, p_ridge_blend);
}

float province_mask(vec2 world_xz) {
	if (continent_influence <= 0.0) {
		return 1.0;
	}
	float c = noise((world_xz + CONTINENT_OFFSET) * continent_frequency);
	c = clamp(c / NOISE_MAX_AMPLITUDE, -1.0, 1.0) * 0.5 + 0.5;
	float contrast = max(continent_contrast, 0.0001);
	return mix(1.0, smoothstep(0.5 - contrast, 0.5 + contrast, c), continent_influence);
}

// The relief in normalized units, before height_scale is applied. Split out of
// height_from_mask so a caller can keep the unscaled field: multiplying by height_scale is the
// last thing that happens to a height, so doing it later costs nothing and leaves the shape
// available while height_scale itself is being animated.
float height_norm_from_mask(vec2 world_xz, float mask) {
	vec2 p = world_xz;
	float warp = warp_amount * mask;
	if (warp > 0.0) {
		vec2 w = vec2(
			noise((world_xz + WARP_OFFSET_X) * warp_frequency),
			noise((world_xz + WARP_OFFSET_Y) * warp_frequency));
		p += w * warp;
	}

	float relief = relief_at(p, ridge_amount * mask);

	if (redistribution != 1.0) {
		relief = sign(relief) * pow(abs(relief), redistribution);
	}

	float h_norm = continent_elevation * (mask - continent_sea_level) + relief * mix(relief_floor, 1.0, mask);
	return clamp(h_norm, -1.5, 1.5);
}

float height_from_mask(vec2 world_xz, float mask) {
	return height_norm_from_mask(world_xz, mask) * height_scale;
}

float get_height_norm_at(vec2 world_xz) {
	return height_norm_from_mask(world_xz, province_mask(world_xz));
}

float get_height_at(vec2 world_xz) {
	return height_from_mask(world_xz, province_mask(world_xz));
}

const float TEMPERATURE_MIN_C = -10.0;
const float TEMPERATURE_MAX_C = 30.0;

float temperature_at(vec2 world_xz, float height) {
	float altitude_norm = 1.0 - clamp((height / temperature_altitude_reference) * 0.5 + 0.5, 0.0, 1.0);

	float n = noise((world_xz + temperature_offset) * temperature_frequency);
	float noise_norm = clamp(n / NOISE_MAX_AMPLITUDE, -1.0, 1.0) * 0.5 + 0.5;

	float combined_norm = clamp(altitude_norm + (noise_norm - 0.5) * temperature_noise_influence, 0.0, 1.0);

	return TEMPERATURE_MIN_C + combined_norm * (TEMPERATURE_MAX_C - TEMPERATURE_MIN_C);
}

float moisture_at(vec2 world_xz) {
	float n = noise((world_xz + moisture_offset) * moisture_frequency);
	return clamp(n / NOISE_MAX_AMPLITUDE, -1.0, 1.0) * 0.5 + 0.5;
}

void vertex() {
	vec2 local_xz = VERTEX.xz;
	float max_abs = max(abs(local_xz.x), abs(local_xz.y));
	float morph_factor = smoothstep(morph_band_start, 0.5, max_abs);

	// Height in World Space
	vec3 world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	v_world_xz = world_pos.xz;

	float lod_scale = length(MODEL_MATRIX[0].xyz);
	float final_height = get_height_at(v_world_xz);

	if (morph_factor > 0.0) {
		float coarse_step = 2.0 * lod_scale / resolution;
		vec2 c0 = floor(v_world_xz / coarse_step) * coarse_step;
		vec2 t = (v_world_xz - c0) / coarse_step;
		float h00 = get_height_at(c0);
		float h10 = get_height_at(c0 + vec2(coarse_step, 0.0));
		float h01 = get_height_at(c0 + vec2(0.0, coarse_step));
		float h11 = get_height_at(c0 + vec2(coarse_step, coarse_step));
		float coarse_height = mix(mix(h00, h10, t.x), mix(h01, h11, t.x), t.y);
		final_height = mix(final_height, coarse_height, morph_factor);
	}

	final_height += debug_ripple(length(v_world_xz - debug_center)) * debug_ripple_lift;

	VERTEX.y = final_height / length(MODEL_MATRIX[1].xyz);
	v_height = final_height;

	float vertex_spacing = lod_scale / resolution;
	float e = max(0.05, vertex_spacing * mix(1.0, 2.0, morph_factor));

	float mask = province_mask(v_world_xz);

	float hL = height_from_mask(v_world_xz + vec2(-e, 0.0), mask);
	float hR = height_from_mask(v_world_xz + vec2( e, 0.0), mask);
	float hD = height_from_mask(v_world_xz + vec2(0.0, -e), mask);
	float hU = height_from_mask(v_world_xz + vec2(0.0,  e), mask);

	vec3 normal_ws = normalize(vec3(hL - hR, 2.0 * e, hD - hU));

	v_normal_ws = normal_ws;

	// No TANGENT/BINORMAL: the triplanar whiteout blend has no single tangent frame, so
	// fragment() writes NORMAL in view space directly instead of going through NORMAL_MAP.
	mat3 inv_model = transpose(mat3(MODEL_MATRIX[0].xyz, MODEL_MATRIX[1].xyz, MODEL_MATRIX[2].xyz));
	NORMAL = inv_model * normal_ws;
}

// Only keeps smoothstep's two edges distinct -- GLSL leaves edge0 >= edge1 undefined. Both are
// far below a pixel's worth of climate variation, so blend_softness = 0 reads as a hard boundary.
const float WHITTAKER_TEMP_SOFTNESS_EPSILON = 0.01; // deg C
const float WHITTAKER_MOIST_SOFTNESS_EPSILON = 0.0005; // normalized [0,1]
const float WHITTAKER_WEIGHT_FLOOR = 0.0001;
const float POM_MIN_DEPTH = 0.00001;
const float POM_FADE_MIN_SPAN = 0.001;
const int POM_REFINE_STEPS = 3;
const float POM_GRAZING_CLAMP = 0.1;
const float POM_SECANT_EPSILON = 1e-6;
const float TRIPLANAR_WEIGHT_EPSILON = 0.001;
const float TRIPLANAR_WEIGHT_SUM_MIN = 0.00001;
const float LOD_GRADIENT_MIN = 1e-12;
const float POM_AXIS_WEIGHT_EPSILON = 0.05;
const float BIOME_WEIGHT_EPSILON = 0.004;
const float ROCK_ALPHA_OPAQUE = 0.996;

vec2 axis_uv(vec3 p_v, int p_axis) {
	return p_axis == 0 ? p_v.zy : (p_axis == 1 ? p_v.xz : p_v.xy);
}

float axis_weight(vec3 p_w, int p_axis) {
	return p_axis == 0 ? p_w.x : (p_axis == 1 ? p_w.y : p_w.z);
}

float uv_lod(vec2 p_duv_dx, vec2 p_duv_dy, vec2 p_tex_size) {
	vec2 dx = p_duv_dx * p_tex_size;
	vec2 dy = p_duv_dy * p_tex_size;
	return 0.5 * log2(max(max(dot(dx, dx), dot(dy, dy)), LOD_GRADIENT_MIN));
}

vec3 axis_lods(vec3 p_ddx, vec3 p_ddy, float p_scale, vec2 p_tex_size) {
	return vec3(
		uv_lod(axis_uv(p_ddx, 0) * p_scale, axis_uv(p_ddy, 0) * p_scale, p_tex_size),
		uv_lod(axis_uv(p_ddx, 1) * p_scale, axis_uv(p_ddy, 1) * p_scale, p_tex_size),
		uv_lod(axis_uv(p_ddx, 2) * p_scale, axis_uv(p_ddy, 2) * p_scale, p_tex_size));
}

float axis_lod(vec3 p_lods, int p_axis) {
	return p_axis == 0 ? p_lods.x : (p_axis == 1 ? p_lods.y : p_lods.z);
}

vec3 triplanar_weights(vec3 p_normal) {
	vec3 w = pow(abs(p_normal), vec3(triplanar_sharpness));
	return w / max(w.x + w.y + w.z, TRIPLANAR_WEIGHT_SUM_MIN);
}

vec3 whiteout_x(vec3 p_t, vec3 p_n) { return vec3(abs(p_t.z) * p_n.x, p_t.y + p_n.y, p_t.x + p_n.z); }
vec3 whiteout_y(vec3 p_t, vec3 p_n) { return vec3(p_t.x + p_n.x, abs(p_t.z) * p_n.y, p_t.y + p_n.z); }
vec3 whiteout_z(vec3 p_t, vec3 p_n) { return vec3(p_t.x + p_n.x, p_t.y + p_n.y, abs(p_t.z) * p_n.z); }

vec3 whiteout(vec3 p_t, vec3 p_n, int p_axis) {
	return p_axis == 0 ? whiteout_x(p_t, p_n) : (p_axis == 1 ? whiteout_y(p_t, p_n) : whiteout_z(p_t, p_n));
}

// Hue-only HSV->RGB (saturation = value = 1), used to give each biome id a distinct flat color.
vec3 debug_id_color(int p_id) {
	float hue = fract(float(p_id) * 0.61803398875);
	return clamp(abs(mod(hue * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
}

// Near-white and strictly neutral: the clay view exists to show shape, so anything tinted reads
// as a material choice instead. 1.0 would clip against the key light rather than shade.
const vec3 DEBUG_CLAY_ALBEDO = vec3(0.85);
const float DEBUG_FLAT_ROUGHNESS = 0.85;
const float DEBUG_GRID_EPSILON = 1e-8;

// The climate ramps colour a data field, not a shape. temperature_at() leans on altitude, so its
// ramp happens to double as a height map and the relief comes through by accident; moisture_at() is
// a pure world-XZ field with no height term at all, so it carries no shape whatsoever -- and since
// both views are emission, there is no lighting to supply what the data does not. The terrain stops
// reading as terrain and becomes a texture pasted over the frame.
//
// So the ramps are shaded by the geometric normal against a fixed diagram sun. Wrapped rather than
// clamped lambert: a face turned away darkens instead of going black, which keeps the ramp's own
// hue -- the thing the view exists to show -- readable at every slope. The direction is fixed, not
// the scene's key light: these views never see that light, and pinning the shading to the diagram
// keeps a chapter's relief steady while the real light swings underneath it. Pre-normalised because
// the sun is authored here and nowhere else.
const vec3 DEBUG_RELIEF_SUN = vec3(-0.449, 0.778, 0.439);
const float DEBUG_RELIEF_FLOOR = 0.45;

float debug_relief(vec3 p_geo_normal_ws) {
	float wrapped = clamp(dot(p_geo_normal_ws, DEBUG_RELIEF_SUN) * 0.5 + 0.5, 0.0, 1.0);
	return mix(DEBUG_RELIEF_FLOOR, 1.0, wrapped);
}

// How far past the wipe front this fragment lies: 0 entirely behind it (the A side), 1 entirely
// ahead of it (the B side), ramping across debug_wipe_glow in between. r_seam is 1 exactly on the
// front and 0 a band away, for the highlight that rides it.
//
// The front travels in a straight line from debug_wipe_start to debug_wipe_end as debug_wipe goes
// 0 to 1, both measured as world-space distance along debug_wipe_dir from debug_center -- picked
// per chapter rather than derived, so where the sweep starts, where it ends, and how far short of
// full completeness it lands (leaving unconverted fragments beyond debug_wipe_end, e.g. out of
// camera view) are all authored choices.
float debug_wipe_at(vec2 p_world_xz, out float r_seam) {
	vec2 dir = normalize(debug_wipe_dir + vec2(DEBUG_GRID_EPSILON, 0.0));
	float along = dot(p_world_xz - debug_center, dir);
	float band = max(debug_wipe_glow, DEBUG_GRID_EPSILON);
	float front = mix(debug_wipe_start, debug_wipe_end, clamp(debug_wipe, 0.0, 1.0));

	r_seam = 1.0 - clamp(abs(along - front) / band, 0.0, 1.0);
	return clamp((front - along) / band + 0.5, 0.0, 1.0);
}

vec3 debug_ramp(vec3 p_a, vec3 p_b, vec3 p_c, vec3 p_d, float p_t) {
	float t = clamp(p_t, 0.0, 1.0) * 3.0;
	if (t < 1.0) {
		return mix(p_a, p_b, t);
	}
	if (t < 2.0) {
		return mix(p_b, p_c, t - 1.0);
	}
	return mix(p_c, p_d, t - 2.0);
}

// Distance in pixels from this fragment to the nearest line of a grid spaced p_stride cells
// apart, plus that grid's own on-screen spacing (also in pixels) so the caller can fade it out
// before it turns into a solid fill. p_dcdx/p_dcdy are the screen-space derivatives of the cell
// coordinate: taking them from the smooth coordinate rather than from fract() of it is what
// keeps the lines an even width instead of flaring at every cell boundary.
void debug_grid_distance(vec2 p_cell, vec2 p_dcdx, vec2 p_dcdy, float p_stride,
		out float r_pixels, out float r_spacing) {
	vec2 c = p_cell / p_stride;
	vec2 w = (abs(p_dcdx) + abs(p_dcdy)) / p_stride;
	vec2 d = (0.5 - abs(fract(c) - 0.5)) / max(w, vec2(DEBUG_GRID_EPSILON));
	r_pixels = min(d.x, d.y);
	r_spacing = 1.0 / max(max(w.x, w.y), DEBUG_GRID_EPSILON);
}

// 1 inside a p_width-pixel line, ramping to 0 across the pixel just outside it.
float debug_line_coverage(float p_pixels, float p_width) {
	return clamp(p_width * 0.5 + 0.5 - p_pixels, 0.0, 1.0);
}

float debug_spacing_fade(float p_spacing) {
	return smoothstep(debug_wire_min_spacing * 0.5, debug_wire_min_spacing, p_spacing);
}

// The clipmap mesh's own quad edges. p_cell is UV * resolution, which TerrainGenerator lays out
// so that integer values land exactly on vertex rows on the block, ring-fixup and trim meshes
// alike -- so these are the real edges of the real mesh, at whatever cell size this level has.
float debug_wireframe(vec2 p_cell) {
	vec2 dcdx = dFdx(p_cell);
	vec2 dcdy = dFdy(p_cell);

	float fine_pixels;
	float fine_spacing;
	debug_grid_distance(p_cell, dcdx, dcdy, 1.0, fine_pixels, fine_spacing);
	float fine_fade = debug_spacing_fade(fine_spacing);
	float wire = debug_line_coverage(fine_pixels, debug_wire_width) * fine_fade;

	if (debug_wire_major > 1) {
		float major_pixels;
		float major_spacing;
		debug_grid_distance(p_cell, dcdx, dcdy, float(debug_wire_major), major_pixels, major_spacing);
		wire = max(wire, debug_line_coverage(major_pixels, debug_wire_width) *
				debug_spacing_fade(major_spacing));
	}

	if (debug_wire_diagonal > 0.0) {
		// Each quad's diagonal flips with (x + z) parity, exactly as TerrainGenerator's add_quad
		// emits it: even parity splits tl-br (f.x == f.y), odd splits tr-bl (f.x + f.y == 1).
		vec2 f = fract(p_cell);
		ivec2 q = ivec2(floor(p_cell));
		bool even = ((q.x + q.y) & 1) == 0;
		float flip = even ? -1.0 : 1.0;
		float signed_distance = f.x + flip * f.y - (even ? 0.0 : 1.0);
		float gradient = abs(dcdx.x + flip * dcdx.y) + abs(dcdy.x + flip * dcdy.y);
		float diagonal_pixels = abs(signed_distance) / max(gradient, DEBUG_GRID_EPSILON);
		wire = max(wire, debug_line_coverage(diagonal_pixels, debug_wire_width) *
				debug_wire_diagonal * fine_fade);
	}

	return wire;
}

// Splits one debug view into the (albedo, emission, unlit, flat) tuple that produces it.
//
// An unlit diagram view is a zero albedo plus an emission colour, so scene lighting cannot
// modulate it -- and r_unlit also drops SPECULAR, without which the sky still reflects off the
// surface and a "blank" view comes back dark grey rather than black.
//
// r_flat says to shade the geometry and nothing else: the geometric normal instead of the
// biome/rock normal maps, a neutral roughness, no AO. Only view 0 wants the textured surface, so
// every other view sets it -- and for the clay view that is the whole point. Overriding ALBEDO
// alone leaves the rock layer's normal map lighting up through it, which shows as rock and snow
// texture on what is supposed to be a bare shape.
void debug_view_sample(int p_view, vec3 p_albedo, vec3 p_geo_normal_ws, int p_biome_id,
		out vec3 r_albedo, out vec3 r_emission, out float r_unlit, out float r_flat) {
	r_albedo = vec3(0.0);
	r_emission = vec3(0.0);
	r_unlit = 1.0;
	r_flat = 1.0;

	if (p_view == 0) {
		r_albedo = p_albedo;
		r_unlit = 0.0;
		r_flat = 0.0;
	} else if (p_view == 1) {
		// Re-evaluated per fragment rather than interpolated from the vertices. It costs a full
		// height evaluation per pixel, which only a debug view can afford, but it buys two things
		// the mesh cannot give: the field stays sharp at any mesh_resolution, and it is still
		// there to look at when height_scale is zero and the surface is flat.
		r_emission = vec3(clamp(get_height_norm_at(v_world_xz) * 0.5 + 0.5, 0.0, 1.0));
	} else if (p_view == 2) {
		r_emission = debug_id_color(p_biome_id);
	} else if (p_view == 3) {
		// The geometric normal, the one vertex() finite-differences out of the height field -- not
		// the shading normal, which by this point has the biome and rock normal maps mixed into it
		// and would show their texture rather than the surface the field describes.
		r_emission = p_geo_normal_ws * 0.5 + 0.5;
	} else if (p_view == 4) {
		float t = clamp((temperature_at(v_world_xz, v_height) - TEMPERATURE_MIN_C) /
				(TEMPERATURE_MAX_C - TEMPERATURE_MIN_C), 0.0, 1.0);
		r_emission = debug_ramp(vec3(0.05, 0.16, 0.62), vec3(0.16, 0.68, 0.86),
				vec3(0.96, 0.86, 0.42), vec3(0.86, 0.18, 0.10), t) *
				debug_relief(p_geo_normal_ws);
	} else if (p_view == 5) {
		r_emission = debug_ramp(vec3(0.62, 0.44, 0.20), vec3(0.78, 0.72, 0.34),
				vec3(0.28, 0.72, 0.52), vec3(0.08, 0.34, 0.86), moisture_at(v_world_xz)) *
				debug_relief(p_geo_normal_ws);
	} else if (p_view == 6) {
		r_albedo = DEBUG_CLAY_ALBEDO;
		r_unlit = 0.0;
	}
	// 7 (blank) leaves albedo and emission at zero, which is the black the wireframe draws on.
}

vec4 triplanar_array(sampler2DArray p_tex, int p_layer, vec3 p_pos, vec3 p_ddx, vec3 p_ddy, float p_scale, vec3 p_w) {
	vec4 acc = vec4(0.0);
	for (int a = 0; a < 3; a++) {
		float w = axis_weight(p_w, a);
		if (w > TRIPLANAR_WEIGHT_EPSILON) {
			acc += w * textureGrad(p_tex, vec3(axis_uv(p_pos, a) * p_scale, float(p_layer)), axis_uv(p_ddx, a) * p_scale, axis_uv(p_ddy, a) * p_scale);
		}
	}
	return acc;
}

vec3 triplanar_normal_array(sampler2DArray p_tex, int p_layer, vec3 p_pos, vec3 p_ddx, vec3 p_ddy, float p_scale, vec3 p_w, vec3 p_n) {
	vec3 acc = vec3(0.0);
	for (int a = 0; a < 3; a++) {
		float w = axis_weight(p_w, a);
		if (w > TRIPLANAR_WEIGHT_EPSILON) {
			vec3 t = textureGrad(p_tex, vec3(axis_uv(p_pos, a) * p_scale, float(p_layer)), axis_uv(p_ddx, a) * p_scale, axis_uv(p_ddy, a) * p_scale).xyz * 2.0 - 1.0;
			acc += w * whiteout(t, p_n, a);
		}
	}
	return acc;
}

float triplanar_height_array(int p_layer, vec3 p_pos, float p_scale, vec3 p_lods, vec3 p_w) {
	float acc = 0.0;
	float kept = 0.0;
	for (int a = 0; a < 3; a++) {
		float w = axis_weight(p_w, a);
		if (w > POM_AXIS_WEIGHT_EPSILON) {
			acc += w * textureLod(biome_height_textures, vec3(axis_uv(p_pos, a) * p_scale, float(p_layer)), axis_lod(p_lods, a)).r;
			kept += w;
		}
	}
	return acc / max(kept, TRIPLANAR_WEIGHT_SUM_MIN);
}

float triplanar_rock_height(int p_layer, vec3 p_pos, float p_scale, vec3 p_lods, vec3 p_w) {
	float acc = 0.0;
	float kept = 0.0;
	for (int a = 0; a < 3; a++) {
		float w = axis_weight(p_w, a);
		if (w > POM_AXIS_WEIGHT_EPSILON) {
			acc += w * textureLod(rock_height_textures, vec3(axis_uv(p_pos, a) * p_scale, float(p_layer)), axis_lod(p_lods, a)).r;
			kept += w;
		}
	}
	return acc / max(kept, TRIPLANAR_WEIGHT_SUM_MIN);
}

float blended_height(vec3 p_pos, vec3 p_w, ivec2 p_layers, vec2 p_scales, vec3 p_lod0, vec3 p_lod1, float p_blend, float p_rock_alpha, int p_rock_layer, float p_rock_scale, vec3 p_rock_lod) {
	float h = 0.0;

	if (biome_layer_count > 0 && p_rock_alpha < ROCK_ALPHA_OPAQUE) {
		h = triplanar_height_array(p_layers.x, p_pos, p_scales.x, p_lod0, p_w);
		if (p_blend > BIOME_WEIGHT_EPSILON) {
			h = mix(h, triplanar_height_array(p_layers.y, p_pos, p_scales.y, p_lod1, p_w), p_blend);
		}
	}

	if (rock_layer_enabled && p_rock_alpha > 0.0) {
		h = mix(h, triplanar_rock_height(p_rock_layer, p_pos, p_rock_scale, p_rock_lod, p_w), p_rock_alpha);
	}
	return h;
}

void fragment()
{
	// Dropped first, before the parallax raymarch and the biome blend, so the part of the terrain
	// that has not been revealed yet costs nothing to leave out.
	if (debug_reveal_radius >= 0.0) {
		vec2 reveal_rel = v_world_xz - debug_center;
		if (max(abs(reveal_rel.x), abs(reveal_rel.y)) > debug_reveal_radius) {
			discard;
		}
	}

	vec3 geo_normal_ws = normalize(v_normal_ws);
	vec3 base_pos = vec3(v_world_xz.x, v_height, v_world_xz.y);

	vec3 pos_ddx = dFdx(base_pos);
	vec3 pos_ddy = dFdy(base_pos);

	vec3 tri_w = triplanar_weights(geo_normal_ws);

	// How many biome layers this fragment is allowed to classify against. The configuration's own
	// count unless the diagram overlay is sweeping a biome in, in which case the wipe front decides
	// per fragment -- one uniform-coherent branch, so a normal render is unaffected.
	int active_biome_count = biome_layer_count;
	if (debug_biome_count >= 0) {
		float unused_seam;
		active_biome_count = clamp(debug_wipe_at(v_world_xz, unused_seam) < 0.5
				? debug_biome_count : debug_biome_count_b, 0, biome_layer_count);
	}

	float pom_depth = 0.0;
	float weights[32];
	ivec2 pom_layers = ivec2(0, 0);
	float pom_blend = 0.0;
	if (active_biome_count > 0) {
		float t = temperature_at(v_world_xz, v_height);
		float m = moisture_at(v_world_xz);

		float total_weight = 0.0;
		for (int i = 0; i < active_biome_count; i++) {
			float softness_t = max(WHITTAKER_TEMP_SOFTNESS_EPSILON, (biome_max_temperature[i] - biome_min_temperature[i]) * biome_blend_softness[i]);
			float softness_m = max(WHITTAKER_MOIST_SOFTNESS_EPSILON, (biome_max_moisture[i] - biome_min_moisture[i]) * biome_blend_softness[i]);

			float wt = smoothstep(biome_min_temperature[i] - softness_t, biome_min_temperature[i], t) *
					(1.0 - smoothstep(biome_max_temperature[i], biome_max_temperature[i] + softness_t, t));
			float wm = smoothstep(biome_min_moisture[i] - softness_m, biome_min_moisture[i], m) *
					(1.0 - smoothstep(biome_max_moisture[i], biome_max_moisture[i] + softness_m, m));

			float w = max(wt * wm, WHITTAKER_WEIGHT_FLOOR);
			weights[i] = w;
			total_weight += w;
		}
		float inv_total = 1.0 / total_weight;
		float pom_w0 = 0.0;
		float pom_w1 = 0.0;
		for (int i = 0; i < active_biome_count; i++) {
			weights[i] *= inv_total;
			pom_depth += weights[i] * biome_pom_depth[i];

			if (weights[i] > pom_w0) {
				pom_w1 = pom_w0;
				pom_layers.y = pom_layers.x;
				pom_w0 = weights[i];
				pom_layers.x = i;
			} else if (weights[i] > pom_w1) {
				pom_w1 = weights[i];
				pom_layers.y = i;
			}
		}
		pom_blend = pom_w1 / max(pom_w0 + pom_w1, WHITTAKER_WEIGHT_FLOOR);
	}

	int rock_layer_a = 0;
	int rock_layer_b = 0;
	float rock_scale_a = rock_uv_scale;
	float rock_scale_b = rock_uv_scale;
	float rock_blend = 0.0;
	float rock_depth = rock_pom_depth;
	float rock_alpha = 0.0;

	if (rock_layer_enabled) {
		float threshold = rock_slope_threshold;
		float blend_range = rock_slope_blend_range;
		rock_layer_a = rock_index;
		rock_layer_b = rock_index;

		if (active_biome_count > 0) {
			rock_layer_a = biome_rock_index[pom_layers.x];
			rock_layer_b = rock_layer_a;
			rock_scale_a = biome_rock_uv_scale[pom_layers.x];
			rock_scale_b = rock_scale_a;
			rock_depth = biome_rock_pom_depth[pom_layers.x];
			threshold = biome_slope_threshold[pom_layers.x];
			blend_range = biome_slope_blend_range[pom_layers.x];

			if (pom_blend > BIOME_WEIGHT_EPSILON) {
				rock_layer_b = biome_rock_index[pom_layers.y];
				rock_scale_b = biome_rock_uv_scale[pom_layers.y];
				rock_depth = mix(rock_depth, biome_rock_pom_depth[pom_layers.y], pom_blend);
				threshold = mix(threshold, biome_slope_threshold[pom_layers.y], pom_blend);
				blend_range = mix(blend_range, biome_slope_blend_range[pom_layers.y], pom_blend);

				if (rock_layer_b != rock_layer_a || rock_scale_b != rock_scale_a) {
					rock_blend = pom_blend;
				}
			}
		}

		float slope = 1.0 - dot(geo_normal_ws, vec3(0.0, 1.0, 0.0));
		float safe_blend_range = max(blend_range, 0.001);
		rock_alpha = smoothstep(threshold - safe_blend_range, threshold + safe_blend_range, slope);
	}

	pom_depth = mix(pom_depth, rock_depth, rock_alpha);

	vec3 world_offset = vec3(0.0);

	// max(): GLSL leaves smoothstep undefined for edge0 >= edge1, so a configuration with
	// pom_fade_end at or below pom_fade_start -- which is how the parallax is switched off
	// outright -- would otherwise raymarch against a garbage fade rather than skipping.
	float pom_fade = 1.0 - smoothstep(pom_fade_start,
			max(pom_fade_end, pom_fade_start + POM_FADE_MIN_SPAN), length(VERTEX));

	if (pom_depth > POM_MIN_DEPTH && pom_fade > 0.0) {
		vec3 view_ws = mat3(INV_VIEW_MATRIX) * normalize(-VERTEX);
		float n_dot_v = dot(view_ws, geo_normal_ws);

		vec3 sweep = (view_ws - geo_normal_ws * n_dot_v) / max(abs(n_dot_v), POM_GRAZING_CLAMP) * pom_depth * pom_fade;

		vec2 biome_height_size = vec2(textureSize(biome_height_textures, 0).xy);
		vec2 pom_scales = vec2(biome_uv_scale[pom_layers.x], biome_uv_scale[pom_layers.y]);
		vec3 pom_lod0 = axis_lods(pos_ddx, pos_ddy, pom_scales.x, biome_height_size);
		vec3 pom_lod1 = pom_blend > BIOME_WEIGHT_EPSILON ? axis_lods(pos_ddx, pos_ddy, pom_scales.y, biome_height_size) : pom_lod0;
		vec3 pom_rock_lod = rock_alpha > 0.0 ? axis_lods(pos_ddx, pos_ddy, rock_scale_a, vec2(textureSize(rock_height_textures, 0).xy)) : vec3(0.0);

		float step_count = max(1.0, floor(mix(float(pom_max_steps), float(pom_min_steps), abs(n_dot_v)) * pom_fade));
		int steps = int(step_count);
		float layer_step = 1.0 / step_count;

		float ray_depth = 0.0;
		float cur_surface = 0.0;
		float prev_ray_depth = 0.0;
		float prev_surface = 0.0;
		bool hit = false;

		for (int s = 0; s <= steps; s++) {
			cur_surface = 1.0 - blended_height(base_pos - ray_depth * sweep, tri_w, pom_layers, pom_scales, pom_lod0, pom_lod1, pom_blend, rock_alpha, rock_layer_a, rock_scale_a, pom_rock_lod);
			if (ray_depth >= cur_surface) {
				hit = true;
				break;
			}

			prev_surface = cur_surface;
			prev_ray_depth = ray_depth;
			ray_depth += layer_step;
		}

		float final_depth = ray_depth;

		if (hit) {
			float above_depth = prev_ray_depth;
			float above_surface = prev_surface;
			float below_depth = ray_depth;
			float below_surface = cur_surface;

			for (int b = 0; b < POM_REFINE_STEPS; b++) {
				float mid_depth = mix(above_depth, below_depth, 0.5);
				float mid_surface = 1.0 - blended_height(base_pos - mid_depth * sweep, tri_w, pom_layers, pom_scales, pom_lod0, pom_lod1, pom_blend, rock_alpha, rock_layer_a, rock_scale_a, pom_rock_lod);

				if (mid_depth >= mid_surface) {
					below_depth = mid_depth;
					below_surface = mid_surface;
				} else {
					above_depth = mid_depth;
					above_surface = mid_surface;
				}
			}

			float after = below_surface - below_depth;
			float before = above_surface - above_depth;
			float denom = after - before;
			final_depth = abs(denom) > POM_SECANT_EPSILON ? mix(below_depth, above_depth, clamp(after / denom, 0.0, 1.0)) : below_depth;
		}

		world_offset = -final_depth * sweep;
	}

	vec3 sample_pos = base_pos + world_offset;
	vec3 normal_ws = geo_normal_ws;

	if (active_biome_count == 0) {
		// No biome layers configured, or a CPU-side size-mismatch fallback occurred.
		ALBEDO = vec3(1.0, 0.0, 1.0);
		ROUGHNESS = 0.8;
		AO = 1.0;
	} else {
		vec3 albedo_biome = vec3(0.0);
		vec3 normal_biome = vec3(0.0);
		float roughness_biome = 0.0;
		float ao_biome = 0.0;
		float kept_weight = 0.0;

		for (int i = 0; i < active_biome_count; i++) {
			float w = weights[i];
			if (w <= BIOME_WEIGHT_EPSILON) {
				continue;
			}
			float scale = biome_uv_scale[i];
			kept_weight += w;

			albedo_biome += triplanar_array(biome_albedo_textures, i, sample_pos, pos_ddx, pos_ddy, scale, tri_w).rgb * w;
			roughness_biome += triplanar_array(biome_roughness_textures, i, sample_pos, pos_ddx, pos_ddy, scale, tri_w).r * w;
			ao_biome += triplanar_array(biome_ao_textures, i, sample_pos, pos_ddx, pos_ddy, scale, tri_w).r * w;
			normal_biome += triplanar_normal_array(biome_normal_textures, i, sample_pos, pos_ddx, pos_ddy, scale, tri_w, geo_normal_ws) * w;
		}

		float inv_kept = 1.0 / max(kept_weight, WHITTAKER_WEIGHT_FLOOR);

		ALBEDO = albedo_biome * inv_kept;
		ROUGHNESS = roughness_biome * inv_kept;
		AO = ao_biome * inv_kept;
		normal_ws = normal_biome * inv_kept;
	}

	// Rock/cliff slope overlay -- independent of the biome blend above, applied on top
	// (including on top of the zero-biome-layers magenta debug guard).
	if (rock_layer_enabled && rock_alpha > 0.0) {
		vec3 rock_albedo = triplanar_array(rock_albedo_textures, rock_layer_a, sample_pos, pos_ddx, pos_ddy, rock_scale_a, tri_w).rgb;
		float rock_roughness = triplanar_array(rock_roughness_textures, rock_layer_a, sample_pos, pos_ddx, pos_ddy, rock_scale_a, tri_w).r;
		float rock_ao = triplanar_array(rock_ao_textures, rock_layer_a, sample_pos, pos_ddx, pos_ddy, rock_scale_a, tri_w).r;
		vec3 rock_normal = triplanar_normal_array(rock_normal_textures, rock_layer_a, sample_pos, pos_ddx, pos_ddy, rock_scale_a, tri_w, geo_normal_ws);

		if (rock_blend > 0.0) {
			rock_albedo = mix(rock_albedo, triplanar_array(rock_albedo_textures, rock_layer_b, sample_pos, pos_ddx, pos_ddy, rock_scale_b, tri_w).rgb, rock_blend);
			rock_roughness = mix(rock_roughness, triplanar_array(rock_roughness_textures, rock_layer_b, sample_pos, pos_ddx, pos_ddy, rock_scale_b, tri_w).r, rock_blend);
			rock_ao = mix(rock_ao, triplanar_array(rock_ao_textures, rock_layer_b, sample_pos, pos_ddx, pos_ddy, rock_scale_b, tri_w).r, rock_blend);
			rock_normal = mix(rock_normal, triplanar_normal_array(rock_normal_textures, rock_layer_b, sample_pos, pos_ddx, pos_ddy, rock_scale_b, tri_w, geo_normal_ws), rock_blend);
		}

		ALBEDO = mix(ALBEDO, rock_albedo, rock_alpha);
		ROUGHNESS = mix(ROUGHNESS, rock_roughness, rock_alpha);
		AO = mix(AO, rock_ao, rock_alpha);
		normal_ws = mix(normal_ws, rock_normal, rock_alpha);
	}

	NORMAL = normalize((VIEW_MATRIX * vec4(normal_ws, 0.0)).xyz);

	// Diagram overlay. One uniform-coherent branch, so nothing below runs in a normal render.
	if (debug_view != 0 || debug_view_b != 0 || debug_wire_opacity > 0.0 || debug_wire_opacity_b > 0.0) {
		vec3 albedo;
		vec3 emission;
		float unlit;
		float flat_surface;
		debug_view_sample(debug_view, ALBEDO, geo_normal_ws, pom_layers.x, albedo, emission, unlit, flat_surface);

		// Negative means the wireframe does not take part in the wipe at all, which is what keeps
		// a front that carries only a view or biome change identical to before this existed.
		float wire_opacity = debug_wire_opacity;
		float wire_opacity_b = debug_wire_opacity_b < 0.0 ? debug_wire_opacity : debug_wire_opacity_b;

		// A line sweeping across the patch rather than a crossfade, so what is behind it reads as
		// replacing what is ahead of it instead of the two being visible at once. The front is
		// live whenever it is carrying something -- a change of view, a biome arriving, or the
		// lattice going out -- and the seam rides it either way.
		if (debug_view_b != debug_view || debug_biome_count != debug_biome_count_b ||
				wire_opacity_b != debug_wire_opacity) {
			float seam;
			float wiped = debug_wipe_at(v_world_xz, seam);

			// The lattice belongs to the side of the front it is describing, so it goes out with
			// the view it was drawn under rather than cutting to black a chapter at a time: ahead
			// of the front the grid is still standing, behind it the surface has taken over.
			wire_opacity = mix(debug_wire_opacity, wire_opacity_b, wiped);

			if (debug_view_b != debug_view) {
				vec3 albedo_b;
				vec3 emission_b;
				float unlit_b;
				float flat_b;
				debug_view_sample(debug_view_b, ALBEDO, geo_normal_ws, pom_layers.x, albedo_b, emission_b, unlit_b, flat_b);

				albedo = mix(albedo, albedo_b, wiped);
				emission = mix(emission, emission_b, wiped);
				unlit = mix(unlit, unlit_b, wiped);
				flat_surface = mix(flat_surface, flat_b, wiped);
			}

			emission += debug_wire_color * debug_wire_intensity * seam * seam;
		}

		// Emission only. The flat views are all emission, so this is the knob that grades them,
		// while a lit view's albedo is left exactly as the biome blend produced it -- which keeps
		// debug_view 0 identical to the normal render at any debug_fill.
		emission *= debug_fill;

		if (wire_opacity > 0.0) {
			float wire = debug_wireframe(UV * resolution) * wire_opacity;
			// The line masks the surface under it rather than adding to it, so it stays readable
			// over a bright fill as well as over black.
			albedo *= 1.0 - wire;
			emission += debug_wire_color * debug_wire_intensity * wire;

			if (debug_reveal_radius >= 0.0 && debug_reveal_edge > 0.0) {
				vec2 rel = v_world_xz - debug_center;
				float inside = debug_reveal_radius - max(abs(rel.x), abs(rel.y));
				emission += debug_wire_color * debug_wire_intensity * wire_opacity *
						(1.0 - clamp(inside / debug_reveal_edge, 0.0, 1.0));
			}

			emission += debug_wire_color * debug_wire_intensity *
					debug_ripple(length(v_world_xz - debug_center));
		}

		if (debug_vignette > 0.0) {
			float falloff = 1.0 - debug_vignette *
					smoothstep(0.35, 1.0, length(SCREEN_UV - vec2(0.5)) * 1.41421356);
			albedo *= falloff;
			emission *= falloff;
		}

		ALBEDO = albedo;
		EMISSION = emission;
		SPECULAR = 0.5 * (1.0 - unlit);
		// flat_surface is set by every view but the shaded one, and is always at least as large as
		// unlit, so this covers the unlit views' roughness and AO too.
		NORMAL = normalize((VIEW_MATRIX * vec4(mix(normal_ws, geo_normal_ws, flat_surface), 0.0)).xyz);
		ROUGHNESS = mix(ROUGHNESS, DEBUG_FLAT_ROUGHNESS, flat_surface);
		AO = mix(AO, 1.0, flat_surface);
	}
}
