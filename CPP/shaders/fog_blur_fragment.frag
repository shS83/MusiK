#version 330

uniform sampler2D u_texture;
uniform vec2 u_direction;

in vec2 uv;

out vec4 fragColor;


void main()
{
    vec3 result =
        texture(
            u_texture,
            uv
        ).rgb
        *
        0.227027;


    result +=
        texture(
            u_texture,
            uv +
            u_direction * 1.384615
        ).rgb
        *
        0.316216;


    result +=
        texture(
            u_texture,
            uv -
            u_direction * 1.384615
        ).rgb
        *
        0.316216;


    result +=
        texture(
            u_texture,
            uv +
            u_direction * 3.230769
        ).rgb
        *
        0.070270;


    result +=
        texture(
            u_texture,
            uv -
            u_direction * 3.230769
        ).rgb
        *
        0.070270;


    fragColor =
        vec4(
            result,
            1.0
        );
}
