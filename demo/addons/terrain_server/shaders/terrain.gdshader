shader_type spatial;
render_mode cull_back;

// Physics parameters
uniform float height_scale = 100.0;
uniform float resolution = 64.0;

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
varying float v_world_y;

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
	// Local x/z morphing
	vec2 local_xz = VERTEX.xz;
	float max_abs = max(abs(local_xz.x), abs(local_xz.y));

	// Detect if we are in the outermost 20% of the ring
	float morph_factor = smoothstep(0.4, 0.5, max_abs);

	// Calculate where the upper LOD grid is
	float coarser_step = 2.0 / resolution;
	vec2 morphed_local_xz = round(local_xz / coarser_step) * coarser_step;

	VERTEX.xz = mix(local_xz, morphed_local_xz, morph_factor);

	// Height in World Space
	vec3 world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	v_world_xz = world_pos.xz;

	float final_height = get_height_at(v_world_xz);
	VERTEX.y = final_height / length(MODEL_MATRIX[1].xyz);
	v_height = final_height;

	v_world_y = (MODEL_MATRIX * vec4(VERTEX, 1.0)).y;

	float lod_scale = length(MODEL_MATRIX[0].xyz);
	float e = max(0.05, lod_scale * 0.5 / resolution);

	float hL = get_height_at(v_world_xz + vec2(-e, 0.0));
	float hR = get_height_at(v_world_xz + vec2( e, 0.0));
	float hD = get_height_at(v_world_xz + vec2(0.0, -e));
	float hU = get_height_at(v_world_xz + vec2(0.0,  e));

	vec3 normal_ws   = normalize(vec3(hL - hR, 2.0 * e, hD - hU));
	vec3 tangent_ws  = normalize(vec3(2.0 * e, hR - hL, 0.0));
	vec3 binormal_ws = normalize(vec3(0.0, hU - hD, 2.0 * e));

	v_normal_ws = normal_ws;

	mat3 inv_model = transpose(mat3(MODEL_MATRIX[0].xyz, MODEL_MATRIX[1].xyz, MODEL_MATRIX[2].xyz));
	NORMAL   = inv_model * normal_ws;
	TANGENT  = inv_model * tangent_ws;
	BINORMAL = inv_model * binormal_ws;
}

const float WHITTAKER_TEMP_SOFTNESS_MIN = 0.5; // deg C
const float WHITTAKER_MOIST_SOFTNESS_MIN = 0.02; // normalized [0,1]
const float WHITTAKER_WEIGHT_FLOOR = 0.0001;
// Below this blended depth the parallax march is skipped entirely.
const float POM_MIN_DEPTH = 0.00001;
// Triplanar planes contributing less than this are skipped outright. On level ground the Y
// plane carries all the weight, so flat terrain costs the same single fetch per map that the
// old top-down-only projection did -- only slopes pay for a second or third plane.
const float TRIPLANAR_WEIGHT_EPSILON = 0.001;

vec3 triplanar_weights(vec3 p_normal) {
	vec3 w = pow(abs(p_normal), vec3(triplanar_sharpness));
	return w / max(w.x + w.y + w.z, 0.00001);
}

// Colour/scalar triplanar fetch. `p_pos` is the parallax-resolved world position; `p_ddx` and
// `p_ddy` are derivatives of the *base* world position, taken once in uniform control flow so
// the mip level stays stable inside the raymarch.
vec4 triplanar_array(sampler2DArray p_tex, int p_layer, vec3 p_pos, vec3 p_ddx, vec3 p_ddy, float p_scale, vec3 p_w) {
	vec4 acc = vec4(0.0);
	if (p_w.x > TRIPLANAR_WEIGHT_EPSILON) {
		acc += p_w.x * textureGrad(p_tex, vec3(p_pos.zy * p_scale, float(p_layer)), p_ddx.zy * p_scale, p_ddy.zy * p_scale);
	}
	if (p_w.y > TRIPLANAR_WEIGHT_EPSILON) {
		acc += p_w.y * textureGrad(p_tex, vec3(p_pos.xz * p_scale, float(p_layer)), p_ddx.xz * p_scale, p_ddy.xz * p_scale);
	}
	if (p_w.z > TRIPLANAR_WEIGHT_EPSILON) {
		acc += p_w.z * textureGrad(p_tex, vec3(p_pos.xy * p_scale, float(p_layer)), p_ddx.xy * p_scale, p_ddy.xy * p_scale);
	}
	return acc;
}

vec4 triplanar_2d(sampler2D p_tex, vec3 p_pos, vec3 p_ddx, vec3 p_ddy, float p_scale, vec3 p_w) {
	vec4 acc = vec4(0.0);
	if (p_w.x > TRIPLANAR_WEIGHT_EPSILON) {
		acc += p_w.x * textureGrad(p_tex, p_pos.zy * p_scale, p_ddx.zy * p_scale, p_ddy.zy * p_scale);
	}
	if (p_w.y > TRIPLANAR_WEIGHT_EPSILON) {
		acc += p_w.y * textureGrad(p_tex, p_pos.xz * p_scale, p_ddx.xz * p_scale, p_ddy.xz * p_scale);
	}
	if (p_w.z > TRIPLANAR_WEIGHT_EPSILON) {
		acc += p_w.z * textureGrad(p_tex, p_pos.xy * p_scale, p_ddx.xy * p_scale, p_ddy.xy * p_scale);
	}
	return acc;
}

// Whiteout blend: each plane's tangent-space normal is folded into world space and summed
// with the geometric normal's contribution before weighting. Blending the encoded normals
// directly instead would let the dominant plane wash the other two flat.
vec3 whiteout_x(vec3 p_t, vec3 p_n) { return vec3(abs(p_t.z) * p_n.x, p_t.y + p_n.y, p_t.x + p_n.z); }
vec3 whiteout_y(vec3 p_t, vec3 p_n) { return vec3(p_t.x + p_n.x, abs(p_t.z) * p_n.y, p_t.y + p_n.z); }
vec3 whiteout_z(vec3 p_t, vec3 p_n) { return vec3(p_t.x + p_n.x, p_t.y + p_n.y, abs(p_t.z) * p_n.z); }

vec3 triplanar_normal_array(sampler2DArray p_tex, int p_layer, vec3 p_pos, vec3 p_ddx, vec3 p_ddy, float p_scale, vec3 p_w, vec3 p_n) {
	vec3 acc = vec3(0.0);
	if (p_w.x > TRIPLANAR_WEIGHT_EPSILON) {
		vec3 t = textureGrad(p_tex, vec3(p_pos.zy * p_scale, float(p_layer)), p_ddx.zy * p_scale, p_ddy.zy * p_scale).xyz * 2.0 - 1.0;
		acc += p_w.x * whiteout_x(t, p_n);
	}
	if (p_w.y > TRIPLANAR_WEIGHT_EPSILON) {
		vec3 t = textureGrad(p_tex, vec3(p_pos.xz * p_scale, float(p_layer)), p_ddx.xz * p_scale, p_ddy.xz * p_scale).xyz * 2.0 - 1.0;
		acc += p_w.y * whiteout_y(t, p_n);
	}
	if (p_w.z > TRIPLANAR_WEIGHT_EPSILON) {
		vec3 t = textureGrad(p_tex, vec3(p_pos.xy * p_scale, float(p_layer)), p_ddx.xy * p_scale, p_ddy.xy * p_scale).xyz * 2.0 - 1.0;
		acc += p_w.z * whiteout_z(t, p_n);
	}
	return acc;
}

vec3 triplanar_normal_2d(sampler2D p_tex, vec3 p_pos, vec3 p_ddx, vec3 p_ddy, float p_scale, vec3 p_w, vec3 p_n) {
	vec3 acc = vec3(0.0);
	if (p_w.x > TRIPLANAR_WEIGHT_EPSILON) {
		vec3 t = textureGrad(p_tex, p_pos.zy * p_scale, p_ddx.zy * p_scale, p_ddy.zy * p_scale).xyz * 2.0 - 1.0;
		acc += p_w.x * whiteout_x(t, p_n);
	}
	if (p_w.y > TRIPLANAR_WEIGHT_EPSILON) {
		vec3 t = textureGrad(p_tex, p_pos.xz * p_scale, p_ddx.xz * p_scale, p_ddy.xz * p_scale).xyz * 2.0 - 1.0;
		acc += p_w.y * whiteout_y(t, p_n);
	}
	if (p_w.z > TRIPLANAR_WEIGHT_EPSILON) {
		vec3 t = textureGrad(p_tex, p_pos.xy * p_scale, p_ddx.xy * p_scale, p_ddy.xy * p_scale).xyz * 2.0 - 1.0;
		acc += p_w.z * whiteout_z(t, p_n);
	}
	return acc;
}

void fragment()
{
	vec3 geo_normal_ws = normalize(v_normal_ws);
	vec3 base_pos = vec3(v_world_xz.x, v_world_y, v_world_xz.y);

	// Derivatives of the *base* world position, taken once in uniform control flow. Every
	// fetch below goes through textureGrad with these: sampling inside the raymarch leaves
	// implicit derivatives undefined, and the parallax-resolved position is too discontinuous
	// to derive a sane mip from anyway.
	vec3 pos_ddx = dFdx(base_pos);
	vec3 pos_ddy = dFdy(base_pos);

	// Projecting on all three world planes rather than only top-down is what keeps steep
	// terrain from smearing: a top-down-only projection stretches its texture by 1/cos(slope),
	// and the rock layer's slope threshold means it only ever draws where that factor is
	// already 2x or worse.
	vec3 tri_w = triplanar_weights(geo_normal_ws);

	// Rock coverage is resolved before the march so the height field being marched, and its
	// depth, can be blended exactly the way the final material is mixed further down.
	float rock_alpha = 0.0;
	if (rock_layer_enabled) {
		float slope = 1.0 - dot(geo_normal_ws, vec3(0.0, 1.0, 0.0));
		float safe_blend_range = max(rock_slope_blend_range, 0.001);
		rock_alpha = smoothstep(rock_slope_threshold - safe_blend_range, rock_slope_threshold + safe_blend_range, slope);
	}

	// Whittaker weights depend only on the climate fields, whose noise frequencies (0.002-0.01)
	// are effectively constant across the few centimetres a parallax offset spans. So they are
	// resolved once here, normalized, and reused by both the march and the final sampling --
	// which is what keeps the march single-pass instead of one march per biome.
	float weights[32];
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
		for (int i = 0; i < biome_layer_count; i++) {
			weights[i] /= total_weight;
		}
	}

	// ---- Parallax occlusion mapping ------------------------------------------------------
	// One steep raymarch over the weighted blend of every layer's height field, stepping
	// through world space in metres. The offset stays a full 3D world vector so it feeds the
	// triplanar projection directly, and pom_depth stays an authorable world quantity that is
	// independent of each layer's tiling.
	vec3 world_offset = vec3(0.0);

	float pom_depth = 0.0;
	for (int i = 0; i < biome_layer_count; i++) {
		pom_depth += weights[i] * biome_pom_depth[i];
	}
	pom_depth = mix(pom_depth, rock_pom_depth, rock_alpha);

	float pom_fade = 1.0 - smoothstep(pom_fade_start, pom_fade_end, length(VERTEX));

	if (pom_depth > POM_MIN_DEPTH && pom_fade > 0.0) {
		// Any orthonormal frame around the geometric normal will do -- the march is no longer
		// tied to a UV axis now that sampling is triplanar. Pick the world axis furthest from
		// the normal so the Gram-Schmidt step never degenerates on near-vertical terrain.
		vec3 axis_ref = abs(geo_normal_ws.x) < 0.9 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 0.0, 1.0);
		vec3 t_ws = normalize(axis_ref - geo_normal_ws * dot(geo_normal_ws, axis_ref));
		vec3 b_ws = cross(geo_normal_ws, t_ws);

		vec3 view_ws = mat3(INV_VIEW_MATRIX) * normalize(-VERTEX);
		vec3 view_ts = vec3(dot(view_ws, t_ws), dot(view_ws, b_ws), dot(view_ws, geo_normal_ws));

		// Tangential sweep for a full depth range. The divisor is clamped so grazing views
		// don't blow the offset up to infinity.
		vec3 sweep = (t_ws * view_ts.x + b_ws * view_ts.y) / max(abs(view_ts.z), 0.1) * pom_depth * pom_fade;

		// More steps at grazing angles, where each step covers more ground; fewer looking
		// straight down. Scaled by the distance fade so far tiles stop paying before it ends.
		float step_count = max(1.0, floor(mix(float(pom_max_steps), float(pom_min_steps), abs(view_ts.z)) * pom_fade));
		int steps = int(step_count);
		float layer_step = 1.0 / step_count;
		vec3 delta = sweep / step_count;

		vec3 cur_offset = vec3(0.0);
		float ray_depth = 0.0;
		float cur_surface = 0.0;
		vec3 prev_offset = vec3(0.0);
		float prev_surface = 0.0;
		float prev_ray_depth = 0.0;

		// Walk down the ray until it passes below the height field. Sampling at the top of the
		// body means the previous step's values are still live for the interpolation afterwards,
		// so resolving the hit costs no extra blended fetch.
		for (int s = 0; s <= steps; s++) {
			vec3 p = base_pos + cur_offset;
			float h = 0.0;
			for (int i = 0; i < biome_layer_count; i++) {
				h += triplanar_array(biome_height_textures, i, p, pos_ddx, pos_ddy, biome_uv_scale[i], tri_w).r * weights[i];
			}
			if (rock_layer_enabled) {
				h = mix(h, triplanar_2d(rock_height_texture, p, pos_ddx, pos_ddy, rock_uv_scale, tri_w).r, rock_alpha);
			}

			cur_surface = 1.0 - h;
			if (ray_depth >= cur_surface) {
				break;
			}

			prev_offset = cur_offset;
			prev_surface = cur_surface;
			prev_ray_depth = ray_depth;
			cur_offset -= delta;
			ray_depth += layer_step;
		}

		// Interpolate between the last two samples so the relief doesn't stair-step.
		float after = cur_surface - ray_depth;
		float before = prev_surface - prev_ray_depth;
		float denom = after - before;
		world_offset = abs(denom) > 1e-6 ? mix(cur_offset, prev_offset, clamp(after / denom, 0.0, 1.0)) : cur_offset;
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

		for (int i = 0; i < biome_layer_count; i++) {
			float scale = biome_uv_scale[i];

			albedo_biome += triplanar_array(biome_albedo_textures, i, sample_pos, pos_ddx, pos_ddy, scale, tri_w).rgb * weights[i];
			roughness_biome += triplanar_array(biome_roughness_textures, i, sample_pos, pos_ddx, pos_ddy, scale, tri_w).r * weights[i];
			ao_biome += triplanar_array(biome_ao_textures, i, sample_pos, pos_ddx, pos_ddy, scale, tri_w).r * weights[i];
			normal_biome += triplanar_normal_array(biome_normal_textures, i, sample_pos, pos_ddx, pos_ddy, scale, tri_w, geo_normal_ws) * weights[i];
		}

		ALBEDO = albedo_biome;
		ROUGHNESS = roughness_biome;
		AO = ao_biome;
		normal_ws = normal_biome;
	}

	// Rock/cliff slope overlay -- independent of the biome blend above, applied on top
	// (including on top of the zero-biome-layers magenta debug guard).
	if (rock_layer_enabled) {
		vec3 rock_albedo = triplanar_2d(rock_albedo_texture, sample_pos, pos_ddx, pos_ddy, rock_uv_scale, tri_w).rgb;
		float rock_roughness = triplanar_2d(rock_roughness_texture, sample_pos, pos_ddx, pos_ddy, rock_uv_scale, tri_w).r;
		float rock_ao = triplanar_2d(rock_ao_texture, sample_pos, pos_ddx, pos_ddy, rock_uv_scale, tri_w).r;
		vec3 rock_normal = triplanar_normal_2d(rock_normal_texture, sample_pos, pos_ddx, pos_ddy, rock_uv_scale, tri_w, geo_normal_ws);

		ALBEDO = mix(ALBEDO, rock_albedo, rock_alpha);
		ROUGHNESS = mix(ROUGHNESS, rock_roughness, rock_alpha);
		AO = mix(AO, rock_ao, rock_alpha);
		normal_ws = mix(normal_ws, rock_normal, rock_alpha);
	}

	// Triplanar produces a world-space normal directly, so it is written to NORMAL (view space
	// in a Godot fragment shader) rather than handed to NORMAL_MAP -- which would re-apply it
	// through the vertex TANGENT/BINORMAL frame, the same skewed finite-difference basis the
	// parallax march deliberately avoids.
	NORMAL = normalize((VIEW_MATRIX * vec4(normalize(normal_ws), 0.0)).xyz);
}
