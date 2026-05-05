shader_type spatial;
render_mode cull_back;

// Physic parameters
uniform float height_scale = 100.0;
uniform float resolution = 64.0;

// Noise parameters
uniform int octaves = 5;
uniform float base_frequency = 0.002;
uniform float lacunarity = 2.0;
uniform float gain = 0.5;

uniform sampler2D albedo_texture : source_color;

varying vec2 v_world_xz;

// Hash function to generate a pseudo-random gradient vector based on the input position
vec2 hash(vec2 p) {
	p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
	return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
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
}

void fragment() {
	// Finite difference to compute normal
	float e = max(0.1, length(CAMERA_POSITION_WORLD - (INV_VIEW_MATRIX * vec4(VERTEX, 1.0)).xyz) * 0.005);

	vec3 n;
	n.x = get_height_at(v_world_xz + vec2(-e, 0.0)) - get_height_at(v_world_xz + vec2(e, 0.0));
	n.z = get_height_at(v_world_xz + vec2(0.0, -e)) - get_height_at(v_world_xz + vec2(0.0, e));
	n.y = 2.0 * e;

	vec3 world_normal = normalize(n);

	// TODO Basic visual parameters (will be replaced by layer system in the biomes implementation). Replace with multi-layer material system
	ALBEDO = vec3(1.0, 1.0, 1.0);
	ROUGHNESS = 0.8;

	NORMAL = (VIEW_MATRIX * vec4(world_normal, 0.0)).xyz;

	vec4 tex_color = texture(albedo_texture, v_world_xz * 0.1);
	ALBEDO = tex_color.rgb; //vec3(0.2, 0.6, 0.2); // Basic green color
	ROUGHNESS = 0.8;
}
