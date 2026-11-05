#version 330
in vec2 local_uv;
in vec3 color;
out vec4 frag;
void main() {
    float r = length(local_uv);
    if (r >= 1.0) discard;
    float halo = exp(-r*r*13.0) * 0.25;
    float inner = (1.0 - smoothstep(0.09, 0.30, r)) * 0.72;
    float hot = (1.0 - smoothstep(0.015, 0.095, r)) * 0.75;
    vec3 rgb = color * (halo + inner) + vec3(hot);
    frag = vec4(rgb, 1.0);
}
