// Trailer diorama shader body (see TODO_TRAILER.md T3).
//
// This file is NOT a standalone shader and is never #include'd either: diorama.gd concatenates
// the *prefix* of the shipped terrain shader (every uniform, varying and noise/climate function
// declared before its `void vertex()`) with this body, and hands the result to Shader.code. That
// way the diorama displaces its vertices with the exact same noise the production shader and the
// CPU port use -- animating a trailer parameter is then literally animating the plugin's own
// parameter -- without a shared .gdshaderinc refactor of the benchmarked production shader, and
// without a copy of the noise that could silently drift from it.
//
// Everything the trailer animates is a `d_`-prefixed uniform driven per frame by trailer_rig.gd.
// The terrain's own uniforms (octaves, ridge_amount, warp_amount, ...) are driven directly.

varying vec3 v_bary;
varying float v_skirt;

// Diorama placement: the patch is a finite square of `d_extent` half-size centred on d_center in
// world XZ, so the noise sampled here matches the same spot of the infinite terrain.
uniform vec2 d_center = vec2(0.0);
uniform float d_extent = 768.0;
uniform float d_cell = 12.0;

// Vertical build-up. Heights are always evaluated at full strength (so the grayscale/climate views
// stay stable while the mesh is still flat); d_height_mix only scales the displacement.
uniform float d_height_mix = 0.0;
uniform float d_skirt_mix = 0.0;
uniform float d_skirt_depth = 260.0;

// Grid draw-in: nothing outside this Chebyshev radius from d_center exists yet.
uniform float d_reveal_radius = 100000.0;
uniform float d_reveal_edge = 90.0;

// Wireframe.
uniform vec3 d_wire_color = vec3(0.35, 0.85, 1.0);
uniform float d_wire_intensity = 1.15;
uniform float d_wire_opacity = 1.0;
uniform float d_wire_width = 1.0;
uniform float d_diagonal_intensity = 0.22;

// Beat juice: d_pulse is a per-frame envelope off the audio cues, d_ripple_* an expanding ring.
uniform float d_pulse = 0.0;
uniform float d_pulse_gain = 2.5;
uniform float d_ripple_radius = -1.0;
uniform float d_ripple_strength = 0.0;
uniform float d_ripple_width = 70.0;
uniform float d_ripple_lift = 0.0;

// Fill: a cross-fade between two views, wiped across the patch by a moving scan line.
// 0 = nothing (black), 1 = height grayscale, 2 = world normals, 3 = lit clay,
// 4 = temperature, 5 = moisture, 6 = biomes.
uniform int d_view_a = 0;
uniform int d_view_b = 0;
uniform float d_sweep = 0.0;
uniform vec2 d_sweep_dir = vec2(1.0, 0.0);
uniform float d_sweep_glow_width = 26.0;
uniform float d_vignette = 0.0;
uniform float d_fill_intensity = 1.0;
uniform float d_shade_amount = 0.0;
uniform vec3 d_light_dir = vec3(-0.5, 0.7, -0.5);

// Biome diagram colours, revealed one at a time as the music introduces them.
uniform vec3 d_biome_color[32];
uniform float d_biome_reveal[32];
uniform vec3 d_rock_color = vec3(0.42, 0.40, 0.44);
uniform float d_rock_reveal = 0.0;

// Per-clipmap-level dimming, so the outer rings can sit behind level 0 without competing.
instance uniform float d_level_intensity = 1.0;

// Normalised height clusters tightly around mid-grey, which reads as flat white once the wireframe
// is drawn over it; the height view stretches it around 0.5 so the noise field is actually legible.
const float DIORAMA_HEIGHT_CONTRAST = 1.9;
const float DIORAMA_SWEEP_BLEND = 9.0;
const float DIORAMA_STRATA_SPACING = 38.0;
const float DIORAMA_MIN_SHADE = 0.22;

// Mirrors of the production shader's Whittaker constants. They are declared *after* its
// `void vertex()`, so they fall outside the prefix diorama.gd splices in and have to be restated
// here; the names differ so a future move of the splice point fails to compile instead of
// silently redefining them.
const float DIORAMA_TEMP_SOFTNESS_EPSILON = 0.01; // deg C
const float DIORAMA_MOIST_SOFTNESS_EPSILON = 0.0005; // normalized [0,1]
const float DIORAMA_WEIGHT_FLOOR = 0.0001;

float diorama_ripple(float p_distance) {
	if (d_ripple_strength <= 0.0 || d_ripple_radius < 0.0) {
		return 0.0;
	}
	float x = (p_distance - d_ripple_radius) / max(d_ripple_width, 0.001);
	return d_ripple_strength * exp(-x * x);
}

void vertex() {
	vec3 world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	v_world_xz = world_pos.xz;
	v_bary = COLOR.rgb;
	v_skirt = UV.y;

	float mask = province_mask(v_world_xz);
	float h = height_from_mask(v_world_xz, mask);
	v_height = h;

	float surface = h * d_height_mix + diorama_ripple(length(v_world_xz - d_center)) * d_ripple_lift;
	// UV.x flags the lower row of the skirt walls, which hang below the surface to read as a solid
	// slab rather than an infinitely thin sheet.
	float y = mix(surface, surface - d_skirt_depth, UV.x * d_skirt_mix);

	// The node carries no rotation or scale, so world Y maps straight onto local Y.
	VERTEX.y = y - MODEL_MATRIX[3].y;

	float e = max(d_cell, 0.05);
	float hL = height_from_mask(v_world_xz + vec2(-e, 0.0), mask);
	float hR = height_from_mask(v_world_xz + vec2(e, 0.0), mask);
	float hD = height_from_mask(v_world_xz + vec2(0.0, -e), mask);
	float hU = height_from_mask(v_world_xz + vec2(0.0, e), mask);

	v_normal_ws = normalize(vec3((hL - hR) * d_height_mix, 2.0 * e, (hD - hU) * d_height_mix));
	NORMAL = v_normal_ws;
}

vec3 diorama_ramp(vec3 p_a, vec3 p_b, vec3 p_c, vec3 p_d, float p_x) {
	float x = clamp(p_x, 0.0, 1.0) * 3.0;
	if (x < 1.0) {
		return mix(p_a, p_b, x);
	}
	if (x < 2.0) {
		return mix(p_b, p_c, x - 1.0);
	}
	return mix(p_c, p_d, x - 2.0);
}

vec3 diorama_biome_color(vec3 p_normal) {
	if (biome_layer_count <= 0) {
		return vec3(0.5);
	}

	float t = temperature_at(v_world_xz, v_height);
	float m = moisture_at(v_world_xz);

	float weights[32];
	float total_weight = 0.0;
	for (int i = 0; i < biome_layer_count; i++) {
		float softness_t = max(DIORAMA_TEMP_SOFTNESS_EPSILON, (biome_max_temperature[i] - biome_min_temperature[i]) * biome_blend_softness[i]);
		float softness_m = max(DIORAMA_MOIST_SOFTNESS_EPSILON, (biome_max_moisture[i] - biome_min_moisture[i]) * biome_blend_softness[i]);

		float wt = smoothstep(biome_min_temperature[i] - softness_t, biome_min_temperature[i], t) *
				(1.0 - smoothstep(biome_max_temperature[i], biome_max_temperature[i] + softness_t, t));
		float wm = smoothstep(biome_min_moisture[i] - softness_m, biome_min_moisture[i], m) *
				(1.0 - smoothstep(biome_max_moisture[i], biome_max_moisture[i] + softness_m, m));

		float w = max(wt * wm, DIORAMA_WEIGHT_FLOOR);
		weights[i] = w;
		total_weight += w;
	}

	float inv_total = 1.0 / total_weight;
	vec3 color = vec3(0.0);
	float slope_threshold = 0.0;
	float slope_range = 0.0;
	for (int i = 0; i < biome_layer_count; i++) {
		float w = weights[i] * inv_total;
		color += d_biome_color[i] * w * clamp(d_biome_reveal[i], 0.0, 1.0);
		slope_threshold += w * biome_slope_threshold[i];
		slope_range += w * biome_slope_blend_range[i];
	}

	// The slope/cliff overlay, blended over the whole biome field rather than picked from the
	// dominant layer: at diagram scale the difference is invisible and this keeps it branchless.
	if (d_rock_reveal > 0.0) {
		float slope = 1.0 - dot(p_normal, vec3(0.0, 1.0, 0.0));
		float rock_alpha = smoothstep(slope_threshold - max(slope_range, 0.001), slope_threshold + max(slope_range, 0.001), slope);
		color = mix(color, d_rock_color, rock_alpha * clamp(d_rock_reveal, 0.0, 1.0));
	}

	return color;
}

vec3 diorama_view(int p_view, vec3 p_normal, float p_shade) {
	if (p_view == 1) {
		float g = clamp(v_height / max(height_scale, 0.0001) * 0.5 + 0.5, 0.0, 1.0);
		return vec3(clamp((g - 0.5) * DIORAMA_HEIGHT_CONTRAST + 0.5, 0.0, 1.0));
	}
	if (p_view == 2) {
		return p_normal * 0.5 + 0.5;
	}
	if (p_view == 3) {
		return vec3(0.72, 0.70, 0.68) * p_shade;
	}
	if (p_view == 4) {
		float t = clamp((temperature_at(v_world_xz, v_height) + 10.0) / 40.0, 0.0, 1.0);
		return diorama_ramp(vec3(0.05, 0.16, 0.62), vec3(0.16, 0.68, 0.86), vec3(0.96, 0.86, 0.42), vec3(0.86, 0.18, 0.10), t);
	}
	if (p_view == 5) {
		float m = clamp(moisture_at(v_world_xz), 0.0, 1.0);
		return diorama_ramp(vec3(0.62, 0.44, 0.20), vec3(0.78, 0.72, 0.34), vec3(0.28, 0.72, 0.52), vec3(0.08, 0.34, 0.86), m);
	}
	if (p_view == 6) {
		return diorama_biome_color(p_normal);
	}
	return vec3(0.0);
}

void fragment() {
	vec2 rel = v_world_xz - d_center;
	float chebyshev = max(abs(rel.x), abs(rel.y));
	if (chebyshev > d_reveal_radius) {
		discard;
	}

	vec3 normal_ws = normalize(v_normal_ws);
	float shade = max(DIORAMA_MIN_SHADE, dot(normal_ws, normalize(d_light_dir)));

	// Scan-line wipe between the outgoing and incoming view.
	float s = dot(rel, normalize(d_sweep_dir));
	float boundary = mix(-d_extent * 1.5, d_extent * 1.5, clamp(d_sweep, 0.0, 1.0));
	float wiped = 1.0 - smoothstep(boundary - DIORAMA_SWEEP_BLEND, boundary + DIORAMA_SWEEP_BLEND, s);

	vec3 fill = mix(diorama_view(d_view_a, normal_ws, shade), diorama_view(d_view_b, normal_ws, shade), wiped);
	fill *= mix(1.0, shade, clamp(d_shade_amount, 0.0, 1.0));
	fill *= d_fill_intensity;

	float skirt = step(0.5, v_skirt);
	if (skirt > 0.5) {
		// Cross-section walls: darker, with faint strata lines so the slab reads as "material".
		float strata = smoothstep(0.45, 0.5, abs(fract(v_height * d_height_mix / DIORAMA_STRATA_SPACING) - 0.5));
		fill = fill * 0.12 + d_wire_color * 0.03 * strata;
	}

	// Wireframe from barycentric coordinates: x/y hit the quad edges, z the shared diagonal, which
	// is drawn fainter so the grid reads as quads while still showing the real triangulation.
	vec3 bary = v_bary;
	vec3 width = fwidth(bary) * max(d_wire_width, 0.01);
	vec3 aa = smoothstep(vec3(0.0), width, bary);
	float wire = max(1.0 - min(aa.x, aa.y), (1.0 - aa.z) * d_diagonal_intensity);
	// Once a cell is only a few pixels wide the lines merge into a solid sheet; fade them out
	// instead of letting the grid alias into noise.
	wire *= clamp(1.0 - (max(width.x, width.y) - 0.22) * 3.5, 0.12, 1.0);
	// The skirt's walls are one tall quad per border segment: at full strength their edges ring the
	// whole patch in a neon frame that outshines the terrain inside it, and at a grazing angle they
	// alias into vertical stripes. The walls read as a solid slab from their fill alone.
	wire *= mix(1.0, 0.1, skirt);

	float ripple = diorama_ripple(length(rel));
	vec3 wire_color = d_wire_color * (d_wire_intensity + d_pulse * d_pulse_gain + ripple * 3.0);

	vec3 color = mix(fill, wire_color, clamp(wire * d_wire_opacity, 0.0, 1.0));

	// Leading edge of the grid draw-in.
	float edge_glow = d_reveal_radius < d_extent * 4.0
			? smoothstep(d_reveal_radius - d_reveal_edge, d_reveal_radius, chebyshev)
			: 0.0;
	// Leading edge of the view wipe, only while one is actually running.
	float sweep_glow = step(0.001, d_sweep) * step(d_sweep, 0.999) *
			exp(-abs(s - boundary) / max(d_sweep_glow_width, 0.001));

	color += d_wire_color * (edge_glow * 1.1 + sweep_glow * 1.2 + ripple * 0.5);

	// Screen-space radial falloff. The diagram sits in black, so this reads as the frame closing in
	// around level 0 rather than as a darkened edge, and it keeps the outer clipmap rings from
	// competing with the centre. 0 disables it outright.
	if (d_vignette > 0.0) {
		vec2 offset = SCREEN_UV - vec2(0.5);
		color *= 1.0 - d_vignette * clamp(dot(offset, offset) * 2.0, 0.0, 1.0);
	}

	ALBEDO = color * d_level_intensity;
}
