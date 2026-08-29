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
};

struct TemperatureMoistureParams {
	float temperature_frequency = 0.0004f;
	godot::Vector2 temperature_offset = { 10000.0f, -6000.0f };
	float temperature_noise_influence = 0.4f;
	float temperature_altitude_reference = 10.0f;
	float moisture_frequency = 0.0006f;
	godot::Vector2 moisture_offset = { -4000.0f, 9000.0f };
};

inline constexpr float NOISE_MAX_AMPLITUDE = 0.70710678f;

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

inline float get_height_at(const godot::Vector2 p_world_xz, const FbmParams &p_params) {
	float h = 0.0f;
	float amplitude = 1.0f;
	float frequency = p_params.base_frequency;
	float max_amplitude = 0.0f;

	for (int i = 0; i < p_params.octaves; i++) {
		h += noise(p_world_xz * frequency) * amplitude;
		max_amplitude += amplitude;
		amplitude *= p_params.gain;
		frequency *= p_params.lacunarity;
	}

	if (max_amplitude <= 0.0f) {
		return 0.0f;
	}

	return (h / max_amplitude) * p_params.height_scale;
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