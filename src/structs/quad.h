#pragma once

typedef struct
{
    float x;
    float y;
} vec2pos;

typedef struct
{
    float r;
    float g;
    float b;
    float a;
} vec4color;

typedef struct
{
    vec2pos bl;
    vec2pos tr;
} GemQuad;

static inline GemQuad make_quad(float x1, float y1, float x2, float y2)
{
    GemQuad res;
    res.bl.x = x1;
    res.bl.y = y1;
    res.tr.x = x2;
    res.tr.y = y2;
    return res;
}
