#version 330

uniform float u_time;
uniform float u_beat;

uniform vec3 u_camera_pos;
uniform vec3 u_light_pos;

in vec3 world_pos;
in vec3 normal;

out vec4 fragColor;


void main()
{
    vec3 N =
        normalize(normal);

    vec3 L =
        normalize(
            u_light_pos -
            world_pos
        );

    vec3 V =
        normalize(
            u_camera_pos -
            world_pos
        );

    vec3 H =
        normalize(
            L + V
        );


    float diffuse =
        max(
            dot(N, L),
            0.0
        );


    float specular =
        pow(
            max(
                dot(N, H),
                0.0
            ),
            90.0
        );


    float fresnel =
        pow(
            1.0 -
            abs(
                dot(N, V)
            ),
            2.3
        );


    float pulse =
        0.5 +
        0.5 *
        sin(
            u_time * 2.0
        );


    // Tummansininen runko
    vec3 base =
        vec3(
            0.008,
            0.025,
            0.10
        );


    // Kirkas sähkönsininen
    vec3 blue =
        vec3(
            0.04,
            0.32,
            1.00
        );


    // UV-henkinen vaaleansininen
    vec3 electric =
        vec3(
            0.15,
            0.72,
            1.00
        );


    vec3 color =
        base * 0.50;


    color +=
        blue *
        (
            0.24 +
            diffuse * 0.70
        );


    color +=
        vec3(
            0.70,
            0.88,
            1.00
        )
        *
        specular
        *
        (1.25 + 0.45 * u_beat);


    color +=
        electric
        *
        fresnel
        *
        (
            0.65 +
            pulse * 0.20
        );


    // Pieni oma emissive hohto jo itse pinnassa
    color +=
        electric *
        (
            0.10 +
            pulse * 0.05
        );


    fragColor =
        vec4(
            color,
            1.0
        );
}
