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
uniform int biome_layer_count = 0;

uniform float biome_min_temperature[32];
uniform float biome_max_temperature[32];
uniform float biome_min_moisture[32];
uniform float biome_max_moisture[32];

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

void fragment()
{
	NORMAL = normalize(NORMAL);

	if (biome_layer_count == 0) {
		// No biome layers configured, or a CPU-side size-mismatch fallback occurred.
		ALBEDO = vec3(1.0, 0.0, 1.0);
		ROUGHNESS = 0.8;
	} else {
		vec2 uv = v_world_xz * 0.1;
		float t = temperature_at(v_world_xz, v_height);
		float m = moisture_at(v_world_xz);

		float total_weight = 0.0;
		vec3 albedo_biome = vec3(0.0);
		vec3 normal_biome = vec3(0.0);
		float roughness_biome = 0.0;

		for (int i = 0; i < biome_layer_count; i++) {
			float softness_t = max(WHITTAKER_TEMP_SOFTNESS_MIN, (biome_max_temperature[i] - biome_min_temperature[i]) * 0.25);
			float softness_m = max(WHITTAKER_MOIST_SOFTNESS_MIN, (biome_max_moisture[i] - biome_min_moisture[i]) * 0.25);

			float wt = smoothstep(biome_min_temperature[i] - softness_t, biome_min_temperature[i], t) *
					(1.0 - smoothstep(biome_max_temperature[i], biome_max_temperature[i] + softness_t, t));
			float wm = smoothstep(biome_min_moisture[i] - softness_m, biome_min_moisture[i], m) *
					(1.0 - smoothstep(biome_max_moisture[i], biome_max_moisture[i] + softness_m, m));

			float w = max(wt * wm, WHITTAKER_WEIGHT_FLOOR);
			total_weight += w;

			albedo_biome += texture(biome_albedo_textures, vec3(uv, float(i))).rgb * w;
			normal_biome += texture(biome_normal_textures, vec3(uv, float(i))).rgb * w;
			roughness_biome += texture(biome_roughness_textures, vec3(uv, float(i))).r * w;
		}

		albedo_biome /= total_weight;
		normal_biome /= total_weight;
		roughness_biome /= total_weight;

		ALBEDO = albedo_biome;
		NORMAL_MAP = normal_biome;
		ROUGHNESS = roughness_biome;
	}
}
