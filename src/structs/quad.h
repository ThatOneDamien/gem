#pragma once

#include <stdbool.h>

typedef struct vec2pos vec2pos;
typedef struct GemQuad GemQuad;
typedef struct GemPadding GemPadding;

struct vec2pos
{
    int x;
    int y;
};

struct GemQuad
{
    vec2pos bl;
    vec2pos tr;
};

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

static inline bool quad_contains(const GemQuad* quad, int x, int y)
{
    return quad->bl.x <= x && quad->tr.x > x && 
           quad->tr.y <= y && quad->bl.y > y;
}

static inline bool quad_contains_vec(const GemQuad* quad, vec2pos pos) 
{ 
    return quad_contains(quad, pos.x, pos.y);
}
