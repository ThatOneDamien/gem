#pragma once

typedef struct vec2pos vec2pos;
struct vec2pos
{
    int x;
    int y;
};

typedef struct vec4color vec4color;
struct vec4color
{
    float r;
    float g;
    float b;
    float a;
};

typedef struct GemQuad GemQuad;
struct GemQuad
{
    vec2pos bl;
    vec2pos tr;
};

typedef struct GemPadding GemPadding;
struct GemPadding
{
    int bottom;
    int top;
    int left;
    int right;
};

static inline GemQuad make_quad(int x1, int y1, int x2, int y2)
{
    GemQuad res;
    res.bl.x = x1;
    res.bl.y = y1;
    res.tr.x = x2;
    res.tr.y = y2;
    return res;
}
