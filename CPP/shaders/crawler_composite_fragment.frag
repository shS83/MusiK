#version 330
uniform sampler2D trail;
uniform vec2 render_resolution;
uniform vec2 trail_resolution;
uniform float elapsed;
uniform float energy;
uniform float mid;
uniform float treble;
uniform float bass;
uniform int plasma_on;
uniform int bloom_on;
uniform int scanlines_on;
uniform int vignette_on;
in vec2 uv;
out vec4 frag;

vec3 hsv(float h, float s, float v) {
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return v * mix(vec3(1.0), rgb, s);
}

void main() {
    float t = elapsed;
    vec2 p = (uv - 0.5) * vec2(render_resolution.x / max(render_resolution.y, 1.0), 1.0);
    vec3 background = vec3(0.006, 0.003, 0.014);
    if (plasma_on != 0) {
        float v = sin(p.x*5.1 + t*0.26) + sin(p.y*6.8 - t*0.29);
        v += sin((p.x + p.y)*4.2 + t*0.21);
        v += sin(length(p + 0.25*vec2(sin(t*0.13),cos(t*0.17)))*12.0 - t*0.32);
        float wave = clamp(0.5 + v*0.125, 0.0, 1.0);
        float hue = fract(t*0.012 + wave*0.30 + mid*0.12);
        float strength = (0.012 + 0.035*wave) * (0.7 + energy*0.7);
        background += hsv(hue, 0.72, strength);
    }

    vec3 rays = texture(trail, uv).rgb;
    if (bloom_on != 0) {
        vec2 d = 2.5 / trail_resolution;
        vec3 blur = vec3(0.0);
        blur += texture(trail, uv + vec2( d.x, 0.0)).rgb;
        blur += texture(trail, uv + vec2(-d.x, 0.0)).rgb;
        blur += texture(trail, uv + vec2(0.0,  d.y)).rgb;
        blur += texture(trail, uv + vec2(0.0, -d.y)).rgb;
        blur += texture(trail, uv + d).rgb;
        blur += texture(trail, uv - d).rgb;
        blur += texture(trail, uv + vec2(d.x, -d.y)).rgb;
        blur += texture(trail, uv + vec2(-d.x, d.y)).rgb;
        rays += blur * (0.040 + 0.020 * bass);
    }
    vec3 color = background + rays * (0.90 + 0.08*treble);
    if (vignette_on != 0) {
        float dist = length(p * vec2(0.90, 1.0));
        color *= 1.0 - 0.32*smoothstep(0.30, 1.2, dist);
    }
    if (scanlines_on != 0) {
        float line = 0.965 + 0.035*sin(uv.y * render_resolution.y * 3.14159265);
        color *= line;
    }
    // Reinhard-like soft clip retains saturated neon highlights.
    color = color / (1.0 + max(color - vec3(0.8), vec3(0.0))*0.30);
    frag = vec4(color, 1.0);
}
