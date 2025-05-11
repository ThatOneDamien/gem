#version 450 core
precision highp float;

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoords;
layout(location = 2) in vec4 a_Color;
layout(location = 3) in float a_Solid;

out vec2 v_TexCoords;
out vec4 v_Color;
flat out float v_Solid;

layout(std140, binding = 0) uniform u_Buffer
{
    mat4 u_Proj;
};

void main()
{
    v_TexCoords = a_TexCoords;
    v_Color = a_Color;
    v_Solid = a_Solid;
    gl_Position = u_Proj * vec4(a_Position, 0.0f, 1.0f);
}
