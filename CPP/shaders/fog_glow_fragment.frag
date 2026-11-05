#version 330

uniform float u_time;
uniform float u_beat;
uniform vec3 u_camera_pos;

in vec3 world_pos;
in vec3 normal;

out vec4 fragColor;


void main()
{
    vec3 N =
        normalize(normal);

    vec3 V =
        normalize(
            u_camera_pos -
            world_pos
        );


    float edge =
        pow(
            1.0 -
            abs(
                dot(
                    N,
                    V
                )
            ),
            1.8
        );


    float pulse =
        0.85 +
        0.15 *
        sin(
            u_time * 2.0
        );


    // HDR-arvot yli 1.0 ovat tarkoituksellisia.
    vec3 glow =
        vec3(
            0.08,
            0.48,
            1.45
        );


    float intensity =
        (
            0.60 +
            edge * 1.8
        )
        *
        pulse;


    fragColor =
        vec4(
            glow * intensity * (1.0 + 0.80 * u_beat),
            1.0
        );
}
