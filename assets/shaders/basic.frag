#version 450 core

layout(location = 0) out vec4 o_Color;

in vec2 v_TexCoords;

uniform sampler2D u_Tex;

void main()
{

    // o_Color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    o_Color = vec4(texture(u_Tex, v_TexCoords).r);
}
