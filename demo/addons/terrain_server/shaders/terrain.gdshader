shader_type spatial;
render_mode cull_back;

uniform float height_scale = 100.0;
uniform sampler2D heightmap : filter_linear, repeat_enable;
uniform float terrain_size = 1024.0;
uniform float resolution = 64.0;

varying vec2 v_world_xz;

float get_height_at(vec2 world_xz) {
	vec2 uv = world_xz / terrain_size;
	float h = texture(heightmap, uv).r;
	return (h * 2.0 - 1.0) * height_scale;
}

void vertex() {
	// 1. Local x/z morphing
	vec2 local_xz = VERTEX.xz;
	float max_abs = max(abs(local_xz.x), abs(local_xz.y));

	// Detect if we are in the outermost 20% of the ring
	float morph_factor = smoothstep(0.4, 0.5, max_abs);

	// Calculate where the upper LOD grid is
	float coarser_step = 2.0 / resolution;
	vec2 morphed_local_xz = round(local_xz / coarser_step) * coarser_step;

	VERTEX.xz = mix(local_xz, morphed_local_xz, morph_factor);

	// 2. Position in world space
	vec3 world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	v_world_xz = world_pos.xz;

	float final_height = get_height_at(v_world_xz);
	VERTEX.y = final_height / length(MODEL_MATRIX[1].xyz);
}

void fragment() {
	// 3. Finite Differences per-pixel (Phong Shading)
	float e = 1.0;
	vec3 n;
	n.x = get_height_at(v_world_xz + vec2(-e, 0.0)) - get_height_at(v_world_xz + vec2(e, 0.0));
	n.z = get_height_at(v_world_xz + vec2(0.0, -e)) - get_height_at(v_world_xz + vec2(0.0, e));
	n.y = 2.0 * e;

	vec3 world_normal = normalize(n);

	// TODO Basic visual parameters (will be replaced by layer system in the biomes implementation). Replace with multi-layer material system
	ALBEDO = vec3(1.0, 1.0, 1.0);
	ROUGHNESS = 0.8;

	NORMAL = (VIEW_MATRIX * vec4(world_normal, 0.0)).xyz;
}

