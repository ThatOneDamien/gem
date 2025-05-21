#pragma once

typedef struct
{
    int x;
    int y;
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

typedef struct
{
    int bottom;
    int top;
    int left;
    int right;
} GemPadding;

static inline GemQuad make_quad(int x1, int y1, int x2, int y2)
{
    GemQuad res;
    res.bl.x = x1;
    res.bl.y = y1;
    res.tr.x = x2;
    res.tr.y = y2;
    return res;
}
