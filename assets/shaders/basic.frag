#version 450 core
precision highp float;

in vec2 v_TexCoords;
in vec4 v_Color;
flat in float v_Solid;

layout(location = 0) out vec4 o_Color;

layout(binding = 0) uniform sampler2D u_Tex;

void main()
{
    o_Color = v_Color;
    if(v_Solid == 0.0f)
    {
        float alpha = texture(u_Tex, v_TexCoords).r;
        o_Color.a *= alpha;
    }
}
