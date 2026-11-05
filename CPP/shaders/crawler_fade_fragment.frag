#version 330
uniform sampler2D previous_frame;
uniform float decay;
in vec2 uv;
out vec4 frag;
void main() {
    frag = vec4(texture(previous_frame, uv).rgb * decay, 1.0);
}
