#version 330
uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;
uniform sampler2D u_audio;
uniform float u_wave_strength;
uniform float u_time;
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_edge;
layout(location = 3) in vec2 in_phase_weight;
out vec3 world_pos;
out vec3 normal;
void main() {
    // The actual glyph outline vertices are the oscilloscopes. Each point
    // samples a different position in the live 512-sample waveform, then
    // moves along its local 2D outward contour normal and a little in Z.
    float waveform = texture(u_audio, vec2(fract(in_phase_weight.x), 0.5)).r;
    float offset = clamp(waveform, -1.0, 1.0) * u_wave_strength * in_phase_weight.y;
    vec3 point = in_pos + vec3(in_edge * offset,
        offset * 0.32 * sin(u_time * 1.3 + in_phase_weight.x * 29.0));
    vec4 wp = u_model * vec4(point, 1.0);
    world_pos = wp.xyz;
    normal = normalize(mat3(transpose(inverse(u_model))) * in_normal);
    gl_Position = u_proj * u_view * wp;
}
