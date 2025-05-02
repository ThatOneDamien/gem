#pragma once
#include "core/gap.h"

#include <cglm/cglm.h>

typedef struct
{
    vec2 bottom_left;
    vec2 top_right;
} GemQuad;

void gem_renderer_init(void);
void gem_renderer_cleanup(void);
void gem_draw_str(const char* str, size_t count, const GemQuad* bounding_box);
void gem_draw_buffer(GapBuffer buf, const GemQuad* bounding_box);
