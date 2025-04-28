#version 450 core
precision highp float;

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoords;

out vec2 v_TexCoords;

layout(std140, binding = 0) uniform u_Buffer
{
    mat4 u_Proj;
};

void main()
{
    v_TexCoords = a_TexCoords;
    gl_Position = u_Proj * vec4(a_Position, 0.0f, 1.0f);
}
