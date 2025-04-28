#version 450 core
precision highp float;

in vec2 v_TexCoords;

layout(location = 0) out vec4 o_Color;

layout(binding = 0) uniform sampler2D u_Tex;

void main()
{
    o_Color = vec4(texture(u_Tex, v_TexCoords).r);
}
