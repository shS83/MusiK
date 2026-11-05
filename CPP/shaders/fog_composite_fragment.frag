#version 330

uniform sampler2D u_scene;
uniform sampler2D u_bloom;

uniform float u_bloom_strength;

in vec2 uv;

out vec4 fragColor;


void main()
{
    vec3 scene =
        texture(
            u_scene,
            uv
        ).rgb;


    vec3 bloom =
        texture(
            u_bloom,
            uv
        ).rgb;


    vec3 color =
        scene +
        bloom *
        u_bloom_strength;


    // Kevyt filmic tone map:
    // estää glowta palamasta täysin valkoiseksi.
    color =
        color /
        (
            color +
            vec3(1.0)
        );


    // Hieman nostetaan takaisin kirkkautta
    color =
        pow(
            color,
            vec3(0.90)
        );


    fragColor =
        vec4(
            color,
            1.0
        );
}
