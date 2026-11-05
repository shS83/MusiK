#version 330

uniform float u_time;
uniform float u_density;
uniform vec2 u_resolution;

in vec2 uv;

out vec4 fragColor;


float hash21(vec2 p)
{
    p = fract(
        p * vec2(
            123.34,
            456.21
        )
    );

    p += dot(
        p,
        p + 45.32
    );

    return fract(
        p.x * p.y
    );
}


float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);

    f =
        f * f *
        (3.0 - 2.0 * f);

    float a = hash21(i);

    float b = hash21(
        i + vec2(1, 0)
    );

    float c = hash21(
        i + vec2(0, 1)
    );

    float d = hash21(
        i + vec2(1, 1)
    );

    return mix(
        mix(a, b, f.x),
        mix(c, d, f.x),
        f.y
    );
}


float fbm(vec2 p)
{
    float value = 0.0;
    float amp = 0.5;

    mat2 rot = mat2(
         0.80, 0.60,
        -0.60, 0.80
    );

    for (
        int i = 0;
        i < 4;
        i++
    )
    {
        value += (
            amp * noise(p)
        );

        p = rot * p;

        p *= 2.03;

        p += vec2(
            17.1,
            9.2
        );

        amp *= 0.5;
    }

    return value;
}


float fog_layer(
    vec2 p,
    float scale,
    vec2 drift,
    float seed
)
{
    float t = u_time;

    p *= scale;

    p += drift * t;

    vec2 warp;

    warp.x = fbm(
        p * 0.72 +
        vec2(
            seed + t * 0.018,
            seed * 2.1 - t * 0.012
        )
    );

    warp.y = fbm(
        p * 0.72 +
        vec2(
            seed * 3.7 - t * 0.014,
            seed + 5.2 + t * 0.017
        )
    );

    warp -= 0.5;

    p += warp * 2.2;

    float cloud = fbm(
        p +
        vec2(
            t * 0.009,
            -t * 0.015
        )
    );

    float envelope = fbm(
        p * 0.31 +
        vec2(
            seed * 5.1,
            t * 0.006
        )
    );

    float density = smoothstep(
        0.42,
        0.72,
        cloud
    );

    density *= smoothstep(
        0.16,
        0.68,
        envelope
    );

    return pow(
        density,
        1.10
    );
}


void main()
{
    vec2 p =
        (
            gl_FragCoord.xy -
            u_resolution * 0.5
        )
        /
        u_resolution.y;

    p.y *= 0.82;


    float fog1 = fog_layer(
        p + vec2(-0.20, 0.05),
        1.15,
        vec2(0.020, 0.002),
        1.7
    );

    float fog2 = fog_layer(
        p + vec2(0.30, -0.12),
        1.75,
        vec2(-0.013, 0.005),
        7.2
    );

    float fog3 = fog_layer(
        p + vec2(-0.10, 0.18),
        2.55,
        vec2(0.008, -0.004),
        13.4
    );


    vec3 color =
        vec3(
            0.010,
            0.012,
            0.016
        );


    float a1 =
        fog1 *
        0.33 *
        u_density;

    float a2 =
        fog2 *
        0.24 *
        u_density;

    float a3 =
        fog3 *
        0.16 *
        u_density;


    // Vaaleammat, melko neutraalit harmaat
    vec3 fog_color1 =
        vec3(
            0.72,
            0.73,
            0.75
        );

    vec3 fog_color2 =
        vec3(
            0.78,
            0.79,
            0.81
        );

    vec3 fog_color3 =
        vec3(
            0.84,
            0.85,
            0.87
        );


    color = mix(
        color,
        fog_color1,
        clamp(
            a1,
            0.0,
            0.42
        )
    );

    color = mix(
        color,
        fog_color2,
        clamp(
            a2,
            0.0,
            0.32
        )
    );

    color = mix(
        color,
        fog_color3,
        clamp(
            a3,
            0.0,
            0.24
        )
    );


    float vignette =
        smoothstep(
            0.48,
            1.10,
            length(p)
        );

    color *= (
        1.0 -
        vignette * 0.30
    );


    fragColor = vec4(
        color,
        1.0
    );
}
