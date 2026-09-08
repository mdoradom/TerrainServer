#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <godot_cpp/variant/vector2.hpp>

/**
 * CPU port of the noise/fBm functions in demo/addons/terrain_server/shaders/terrain.gdshader.
 */
namespace ts::TerrainNoise {

struct FbmParams {
	int octaves = 5;
	float base_frequency = 0.002f;
	float lacunarity = 2.0f;
	float gain = 0.5f;
	float height_scale = 10.0f;

	float ridge_amount = 0.0f;
	float ridge_offset = 1.0f;
	float ridge_weight_gain = 1.5f;
	float ridge_crest_rounding = 0.2f;

	float warp_amount = 0.0f;
	float warp_frequency = 0.001f;

	float continent_frequency = 0.00025f;
	float continent_influence = 0.0f;
	float continent_contrast = 0.2f;
	float continent_elevation = 0.0f;
	float continent_sea_level = 0.4f;
	float relief_floor = 1.0f;

	float redistribution = 1.0f;
};

inline bool operator==(const FbmParams &p_a, const FbmParams &p_b) {
	return p_a.octaves == p_b.octaves &&
			p_a.base_frequency == p_b.base_frequency &&
			p_a.lacunarity == p_b.lacunarity &&
			p_a.gain == p_b.gain &&
			p_a.height_scale == p_b.height_scale &&
			p_a.ridge_amount == p_b.ridge_amount &&
			p_a.ridge_offset == p_b.ridge_offset &&
			p_a.ridge_weight_gain == p_b.ridge_weight_gain &&
			p_a.ridge_crest_rounding == p_b.ridge_crest_rounding &&
			p_a.warp_amount == p_b.warp_amount &&
			p_a.warp_frequency == p_b.warp_frequency &&
			p_a.continent_frequency == p_b.continent_frequency &&
			p_a.continent_influence == p_b.continent_influence &&
			p_a.continent_contrast == p_b.continent_contrast &&
			p_a.continent_elevation == p_b.continent_elevation &&
			p_a.continent_sea_level == p_b.continent_sea_level &&
			p_a.relief_floor == p_b.relief_floor &&
			p_a.redistribution == p_b.redistribution;
}

inline bool operator!=(const FbmParams &p_a, const FbmParams &p_b) {
	return !(p_a == p_b);
}

struct TemperatureMoistureParams {
	float temperature_frequency = 0.0004f;
	godot::Vector2 temperature_offset = { 10000.0f, -6000.0f };
	float temperature_noise_influence = 0.4f;
	float temperature_altitude_reference = 10.0f;
	float moisture_frequency = 0.0006f;
	godot::Vector2 moisture_offset = { -4000.0f, 9000.0f };
};

inline constexpr float NOISE_MAX_AMPLITUDE = 0.70710678f;

inline constexpr float WARP_OFFSET_X[2] = { 3100.0f, -1700.0f };
inline constexpr float WARP_OFFSET_Y[2] = { -5300.0f, 2900.0f };
inline constexpr float CONTINENT_OFFSET[2] = { -8100.0f, 4700.0f };

inline constexpr float TEMPERATURE_MIN_C = -10.0f;
inline constexpr float TEMPERATURE_MAX_C = 30.0f;

inline constexpr float GRADIENTS[8][2] = {
	{ 1.0f, 0.0f }, { 0.70710678f, 0.70710678f },
	{ 0.0f, 1.0f }, { -0.70710678f, 0.70710678f },
	{ -1.0f, 0.0f }, { -0.70710678f, -0.70710678f },
	{ 0.0f, -1.0f }, { 0.70710678f, -0.70710678f }
};

inline float fract(const float p_x) {
	return p_x - floorf(p_x);
}

inline void pcg2d(uint32_t &p_x, uint32_t &p_y) {
	p_x = p_x * 1664525u + 1013904223u;
	p_y = p_y * 1664525u + 1013904223u;
	p_x += p_y * 1664525u;
	p_y += p_x * 1664525u;
	p_x ^= (p_x >> 16);
	p_y ^= (p_y >> 16);
	p_x += p_y * 1664525u;
	p_y += p_x * 1664525u;
	p_x ^= (p_x >> 16);
	p_y ^= (p_y >> 16);
}

inline godot::Vector2 hash(const godot::Vector2 p_p) {
	auto hx = static_cast<uint32_t>(static_cast<int32_t>(p_p.x));
	auto hy = static_cast<uint32_t>(static_cast<int32_t>(p_p.y));
	pcg2d(hx, hy);
	const uint32_t idx = hx % 8u;
	return { GRADIENTS[idx][0], GRADIENTS[idx][1] };
}

inline float noise(const godot::Vector2 p_p) {
	const godot::Vector2 i(floorf(p_p.x), floorf(p_p.y));
	const godot::Vector2 f(fract(p_p.x), fract(p_p.y));

	const godot::Vector2 u(
			f.x * f.x * f.x * (f.x * (f.x * 6.0f - 15.0f) + 10.0f),
			f.y * f.y * f.y * (f.y * (f.y * 6.0f - 15.0f) + 10.0f));

	const float n00 = hash(i + godot::Vector2(0.0f, 0.0f)).dot(f - godot::Vector2(0.0f, 0.0f));
	const float n10 = hash(i + godot::Vector2(1.0f, 0.0f)).dot(f - godot::Vector2(1.0f, 0.0f));
	const float n01 = hash(i + godot::Vector2(0.0f, 1.0f)).dot(f - godot::Vector2(0.0f, 1.0f));
	const float n11 = hash(i + godot::Vector2(1.0f, 1.0f)).dot(f - godot::Vector2(1.0f, 1.0f));

	const float nx0 = n00 + (n10 - n00) * u.x;
	const float nx1 = n01 + (n11 - n01) * u.x;
	return nx0 + (nx1 - nx0) * u.y;
}

inline float mix(const float p_a, const float p_b, const float p_t) {
	return p_a + (p_b - p_a) * p_t;
}

inline float smoothstep(const float p_edge0, const float p_edge1, const float p_x) {
	const float t = std::clamp((p_x - p_edge0) / (p_edge1 - p_edge0), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

inline float sign_pow(const float p_x, const float p_exponent) {
	const float s = (p_x > 0.0f) ? 1.0f : ((p_x < 0.0f) ? -1.0f : 0.0f);
	return s * powf(fabsf(p_x), p_exponent);
}

inline float fbm_at(const godot::Vector2 p_p, const FbmParams &p_params) {
	float h = 0.0f;
	float amplitude = 1.0f;
	float frequency = p_params.base_frequency;
	float max_amplitude = 0.0f;

	for (int i = 0; i < p_params.octaves; i++) {
		h += noise(p_p * frequency) * amplitude;
		max_amplitude += amplitude;
		amplitude *= p_params.gain;
		frequency *= p_params.lacunarity;
	}

	if (max_amplitude <= 0.0f) {
		return 0.0f;
	}

	return h / max_amplitude;
}

inline float ridge_fold(const float p_value, const float p_rounding) {
	if (p_rounding <= 0.0f) {
		return fabsf(p_value);
	}
	return sqrtf(p_value * p_value + p_rounding * p_rounding);
}

inline float ridged_at(const godot::Vector2 p_p, const FbmParams &p_params) {
	godot::Vector2 q = p_p * p_params.base_frequency;

	float signal = p_params.ridge_offset - ridge_fold(noise(q) / NOISE_MAX_AMPLITUDE, p_params.ridge_crest_rounding);
	signal *= signal;

	float result = signal;
	float amplitude = 1.0f;
	float max_amplitude = 1.0f;

	for (int i = 1; i < p_params.octaves; i++) {
		q = godot::Vector2(0.8f * q.x + 0.6f * q.y, -0.6f * q.x + 0.8f * q.y) * p_params.lacunarity;
		amplitude *= p_params.gain;

		const float weight = std::clamp(signal * p_params.ridge_weight_gain, 0.0f, 1.0f);
		signal = p_params.ridge_offset - ridge_fold(noise(q) / NOISE_MAX_AMPLITUDE, p_params.ridge_crest_rounding);
		signal *= signal;

		result += signal * weight * amplitude;
		max_amplitude += amplitude;
	}

	const float peak = std::max(p_params.ridge_offset - p_params.ridge_crest_rounding, 0.01f);
	result /= max_amplitude * peak * peak;
	return result * 2.0f - 1.0f;
}

inline float get_height_at(const godot::Vector2 p_world_xz, const FbmParams &p_params) {
	float mask = 1.0f;
	if (p_params.continent_influence > 0.0f) {
		const godot::Vector2 scaled = p_world_xz * p_params.continent_frequency;
		float c = noise(scaled + godot::Vector2(CONTINENT_OFFSET[0], CONTINENT_OFFSET[1]));
		c = std::clamp(c / NOISE_MAX_AMPLITUDE, -1.0f, 1.0f) * 0.5f + 0.5f;
		const float contrast = std::max(p_params.continent_contrast, 0.0001f);
		mask = mix(1.0f, smoothstep(0.5f - contrast, 0.5f + contrast, c), p_params.continent_influence);
	}

	godot::Vector2 p = p_world_xz;
	const float warp = p_params.warp_amount * mask;
	if (warp > 0.0f) {
		const godot::Vector2 scaled = p_world_xz * p_params.warp_frequency;
		const godot::Vector2 w(
				noise(scaled + godot::Vector2(WARP_OFFSET_X[0], WARP_OFFSET_X[1])),
				noise(scaled + godot::Vector2(WARP_OFFSET_Y[0], WARP_OFFSET_Y[1])));
		p += w * warp;
	}

	float relief = fbm_at(p, p_params);
	if (p_params.ridge_amount > 0.0f) {
		relief = mix(relief, ridged_at(p, p_params) * NOISE_MAX_AMPLITUDE, p_params.ridge_amount * mask);
	}

	if (p_params.redistribution != 1.0f) {
		relief = sign_pow(relief, p_params.redistribution);
	}

	const float h_norm = p_params.continent_elevation * (mask - p_params.continent_sea_level) +
			relief * mix(p_params.relief_floor, 1.0f, mask);
	return std::clamp(h_norm, -1.5f, 1.5f) * p_params.height_scale;
}

inline float temperature_at(const godot::Vector2 p_world_xz, const float p_height, const TemperatureMoistureParams &p_params) {
	const float altitude_norm = 1.0f - std::clamp((p_height / p_params.temperature_altitude_reference) * 0.5f + 0.5f, 0.0f, 1.0f);

	const float n = noise(p_world_xz * p_params.temperature_frequency + p_params.temperature_offset);
	const float noise_norm = std::clamp(n / NOISE_MAX_AMPLITUDE, -1.0f, 1.0f) * 0.5f + 0.5f;

	const float combined_norm = std::clamp(altitude_norm + (noise_norm - 0.5f) * p_params.temperature_noise_influence, 0.0f, 1.0f);

	return TEMPERATURE_MIN_C + combined_norm * (TEMPERATURE_MAX_C - TEMPERATURE_MIN_C);
}

inline float moisture_at(const godot::Vector2 p_world_xz, const TemperatureMoistureParams &p_params) {
	const float n = noise(p_world_xz * p_params.moisture_frequency + p_params.moisture_offset);
	return std::clamp(n / NOISE_MAX_AMPLITUDE, -1.0f, 1.0f) * 0.5f + 0.5f;
}

} //namespace ts::TerrainNoise