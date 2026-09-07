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

uniform sampler2D rock_albedo_texture : source_color;
uniform sampler2D rock_normal_texture : hint_normal;
uniform sampler2D rock_roughness_texture;
uniform sampler2D rock_height_texture;
uniform sampler2D rock_ao_texture;
uniform bool rock_layer_enabled = false;
uniform float rock_slope_threshold = 0.6;
uniform float rock_slope_blend_range = 0.15;
uniform float rock_uv_scale = 0.1;
uniform float rock_pom_depth = 0.0;

uniform int pom_min_steps = 8;
uniform int pom_max_steps = 32;
uniform float pom_fade_start = 40.0;
uniform float pom_fade_end = 120.0;
uniform float triplanar_sharpness = 4.0;

varying vec2 v_world_xz;
varying vec3 v_normal_ws;
varying float v_height;

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

// Hash function to generate a pseudo-random gradient vector based on the input position
vec2 hash(vec2 p) {
	uvec2 h = pcg2d(uvec2(ivec2(p)));
	return GRADIENTS[h.x % 8u];
}

// Perlin Noise 2D
float noise(in vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);

	// Quintic curve
	vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

	return mix(mix(dot(hash(i + vec2(0.0, 0.0)), f - vec2(0.0, 0.0)),
		dot(hash(i + vec2(1.0, 0.0)), f - vec2(1.0, 0.0)), u.x),
		mix(dot(hash(i + vec2(0.0, 1.0)), f - vec2(0.0, 1.0)),
			dot(hash(i + vec2(1.0, 1.0)), f - vec2(1.0, 1.0)), u.x), u.y);
}

// Fractal Brownian Motion (fBm)
float get_height_at(vec2 world_xz) {
	float h = 0.0;
	float amplitude = 1.0;
	float frequency = base_frequency;
	float max_amplitude = 0.0;

	// Iterate to add detail (octaves)
	for(int i = 0; i < octaves; i++) {
		h += noise(world_xz * frequency) * amplitude;
		max_amplitude += amplitude;
		amplitude *= gain;
		frequency *= lacunarity;
	}

	// Normalize to [-1, 1] range
	h = h / max_amplitude;
	return h * height_scale;
}

const float NOISE_MAX_AMPLITUDE = 0.70710678;
const float TEMPERATURE_MIN_C = -10.0;
const float TEMPERATURE_MAX_C = 30.0;

float temperature_at(vec2 world_xz, float height) {
	float altitude_norm = 1.0 - clamp((height / temperature_altitude_reference) * 0.5 + 0.5, 0.0, 1.0);

	float n = noise(world_xz * temperature_frequency + temperature_offset);
	float noise_norm = clamp(n / NOISE_MAX_AMPLITUDE, -1.0, 1.0) * 0.5 + 0.5;

	float combined_norm = clamp(altitude_norm + (noise_norm - 0.5) * temperature_noise_influence, 0.0, 1.0);

	return TEMPERATURE_MIN_C + combined_norm * (TEMPERATURE_MAX_C - TEMPERATURE_MIN_C);
}

float moisture_at(vec2 world_xz) {
	float n = noise(world_xz * moisture_frequency + moisture_offset);
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

	VERTEX.y = final_height / length(MODEL_MATRIX[1].xyz);
	v_height = final_height;

	float e = max(0.05, lod_scale * mix(0.5, 1.0, morph_factor) / resolution);

	float hL = get_height_at(v_world_xz + vec2(-e, 0.0));
	float hR = get_height_at(v_world_xz + vec2( e, 0.0));
	float hD = get_height_at(v_world_xz + vec2(0.0, -e));
	float hU = get_height_at(v_world_xz + vec2(0.0,  e));

	vec3 normal_ws = normalize(vec3(hL - hR, 2.0 * e, hD - hU));

	v_normal_ws = normal_ws;

	// No TANGENT/BINORMAL: the triplanar whiteout blend has no single tangent frame, so
	// fragment() writes NORMAL in view space directly instead of going through NORMAL_MAP.
	mat3 inv_model = transpose(mat3(MODEL_MATRIX[0].xyz, MODEL_MATRIX[1].xyz, MODEL_MATRIX[2].xyz));
	NORMAL = inv_model * normal_ws;
}

const float WHITTAKER_TEMP_SOFTNESS_MIN = 0.5; // deg C
const float WHITTAKER_MOIST_SOFTNESS_MIN = 0.02; // normalized [0,1]
const float WHITTAKER_WEIGHT_FLOOR = 0.0001;
const float POM_MIN_DEPTH = 0.00001;
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

vec4 triplanar_2d(sampler2D p_tex, vec3 p_pos, vec3 p_ddx, vec3 p_ddy, float p_scale, vec3 p_w) {
	vec4 acc = vec4(0.0);
	for (int a = 0; a < 3; a++) {
		float w = axis_weight(p_w, a);
		if (w > TRIPLANAR_WEIGHT_EPSILON) {
			acc += w * textureGrad(p_tex, axis_uv(p_pos, a) * p_scale, axis_uv(p_ddx, a) * p_scale, axis_uv(p_ddy, a) * p_scale);
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

vec3 triplanar_normal_2d(sampler2D p_tex, vec3 p_pos, vec3 p_ddx, vec3 p_ddy, float p_scale, vec3 p_w, vec3 p_n) {
	vec3 acc = vec3(0.0);
	for (int a = 0; a < 3; a++) {
		float w = axis_weight(p_w, a);
		if (w > TRIPLANAR_WEIGHT_EPSILON) {
			vec3 t = textureGrad(p_tex, axis_uv(p_pos, a) * p_scale, axis_uv(p_ddx, a) * p_scale, axis_uv(p_ddy, a) * p_scale).xyz * 2.0 - 1.0;
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

float triplanar_height_2d(vec3 p_pos, float p_scale, vec3 p_lods, vec3 p_w) {
	float acc = 0.0;
	float kept = 0.0;
	for (int a = 0; a < 3; a++) {
		float w = axis_weight(p_w, a);
		if (w > POM_AXIS_WEIGHT_EPSILON) {
			acc += w * textureLod(rock_height_texture, axis_uv(p_pos, a) * p_scale, axis_lod(p_lods, a)).r;
			kept += w;
		}
	}
	return acc / max(kept, TRIPLANAR_WEIGHT_SUM_MIN);
}

float blended_height(vec3 p_pos, vec3 p_w, ivec2 p_layers, vec2 p_scales, vec3 p_lod0, vec3 p_lod1, float p_blend, float p_rock_alpha, vec3 p_rock_lod) {
	float h = 0.0;

	if (biome_layer_count > 0 && p_rock_alpha < ROCK_ALPHA_OPAQUE) {
		h = triplanar_height_array(p_layers.x, p_pos, p_scales.x, p_lod0, p_w);
		if (p_blend > BIOME_WEIGHT_EPSILON) {
			h = mix(h, triplanar_height_array(p_layers.y, p_pos, p_scales.y, p_lod1, p_w), p_blend);
		}
	}

	if (rock_layer_enabled && p_rock_alpha > 0.0) {
		h = mix(h, triplanar_height_2d(p_pos, rock_uv_scale, p_rock_lod, p_w), p_rock_alpha);
	}
	return h;
}

void fragment()
{
	vec3 geo_normal_ws = normalize(v_normal_ws);
	vec3 base_pos = vec3(v_world_xz.x, v_height, v_world_xz.y);

	vec3 pos_ddx = dFdx(base_pos);
	vec3 pos_ddy = dFdy(base_pos);

	vec3 tri_w = triplanar_weights(geo_normal_ws);

	float rock_alpha = 0.0;
	if (rock_layer_enabled) {
		float slope = 1.0 - dot(geo_normal_ws, vec3(0.0, 1.0, 0.0));
		float safe_blend_range = max(rock_slope_blend_range, 0.001);
		rock_alpha = smoothstep(rock_slope_threshold - safe_blend_range, rock_slope_threshold + safe_blend_range, slope);
	}

	float pom_depth = 0.0;
	float weights[32];
	ivec2 pom_layers = ivec2(0, 0);
	float pom_blend = 0.0;
	if (biome_layer_count > 0) {
		float t = temperature_at(v_world_xz, v_height);
		float m = moisture_at(v_world_xz);

		float total_weight = 0.0;
		for (int i = 0; i < biome_layer_count; i++) {
			float softness_t = max(WHITTAKER_TEMP_SOFTNESS_MIN, (biome_max_temperature[i] - biome_min_temperature[i]) * 0.25);
			float softness_m = max(WHITTAKER_MOIST_SOFTNESS_MIN, (biome_max_moisture[i] - biome_min_moisture[i]) * 0.25);

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
		for (int i = 0; i < biome_layer_count; i++) {
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
	pom_depth = mix(pom_depth, rock_pom_depth, rock_alpha);

	vec3 world_offset = vec3(0.0);

	float pom_fade = 1.0 - smoothstep(pom_fade_start, pom_fade_end, length(VERTEX));

	if (pom_depth > POM_MIN_DEPTH && pom_fade > 0.0) {
		vec3 view_ws = mat3(INV_VIEW_MATRIX) * normalize(-VERTEX);
		float n_dot_v = dot(view_ws, geo_normal_ws);

		vec3 sweep = (view_ws - geo_normal_ws * n_dot_v) / max(abs(n_dot_v), POM_GRAZING_CLAMP) * pom_depth * pom_fade;

		vec2 biome_height_size = vec2(textureSize(biome_height_textures, 0).xy);
		vec2 pom_scales = vec2(biome_uv_scale[pom_layers.x], biome_uv_scale[pom_layers.y]);
		vec3 pom_lod0 = axis_lods(pos_ddx, pos_ddy, pom_scales.x, biome_height_size);
		vec3 pom_lod1 = pom_blend > BIOME_WEIGHT_EPSILON ? axis_lods(pos_ddx, pos_ddy, pom_scales.y, biome_height_size) : pom_lod0;
		vec3 pom_rock_lod = rock_alpha > 0.0 ? axis_lods(pos_ddx, pos_ddy, rock_uv_scale, vec2(textureSize(rock_height_texture, 0))) : vec3(0.0);

		float step_count = max(1.0, floor(mix(float(pom_max_steps), float(pom_min_steps), abs(n_dot_v)) * pom_fade));
		int steps = int(step_count);
		float layer_step = 1.0 / step_count;

		float ray_depth = 0.0;
		float cur_surface = 0.0;
		float prev_ray_depth = 0.0;
		float prev_surface = 0.0;
		bool hit = false;

		for (int s = 0; s <= steps; s++) {
			cur_surface = 1.0 - blended_height(base_pos - ray_depth * sweep, tri_w, pom_layers, pom_scales, pom_lod0, pom_lod1, pom_blend, rock_alpha, pom_rock_lod);
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
				float mid_surface = 1.0 - blended_height(base_pos - mid_depth * sweep, tri_w, pom_layers, pom_scales, pom_lod0, pom_lod1, pom_blend, rock_alpha, pom_rock_lod);

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

	if (biome_layer_count == 0) {
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

		for (int i = 0; i < biome_layer_count; i++) {
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
		vec3 rock_albedo = triplanar_2d(rock_albedo_texture, sample_pos, pos_ddx, pos_ddy, rock_uv_scale, tri_w).rgb;
		float rock_roughness = triplanar_2d(rock_roughness_texture, sample_pos, pos_ddx, pos_ddy, rock_uv_scale, tri_w).r;
		float rock_ao = triplanar_2d(rock_ao_texture, sample_pos, pos_ddx, pos_ddy, rock_uv_scale, tri_w).r;
		vec3 rock_normal = triplanar_normal_2d(rock_normal_texture, sample_pos, pos_ddx, pos_ddy, rock_uv_scale, tri_w, geo_normal_ws);

		ALBEDO = mix(ALBEDO, rock_albedo, rock_alpha);
		ROUGHNESS = mix(ROUGHNESS, rock_roughness, rock_alpha);
		AO = mix(AO, rock_ao, rock_alpha);
		normal_ws = mix(normal_ws, rock_normal, rock_alpha);
	}

	NORMAL = normalize((VIEW_MATRIX * vec4(normal_ws, 0.0)).xyz);
}
