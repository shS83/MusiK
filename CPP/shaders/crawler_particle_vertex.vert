#version 330
in vec2 in_position;
in vec2 in_local;
in vec3 in_color;
out vec2 local_uv;
out vec3 color;
void main() {
    gl_Position = vec4(in_position, 0.0, 1.0);
    local_uv = in_local;
    color = in_color;
}
