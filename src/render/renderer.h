#pragma once

#include <cglm/cglm.h>

typedef struct
{
    vec2 bottom_left;
    vec2 top_right;
} GemQuad;

void gem_renderer_init(void);
void gem_renderer_cleanup(void);
void gem_draw_str(const char* str, const GemQuad* bounding_box);
