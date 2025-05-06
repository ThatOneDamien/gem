#pragma once

#include <cglm/cglm.h>

typedef struct
{
    vec2 bottom_left;
    vec2 top_right;
} GemQuad;

typedef struct
{
    uint32_t draw_calls;
    uint32_t quad_count;
} GemRenderStats;

void gem_renderer_init(void);
void gem_renderer_cleanup(void);
void gem_renderer_start_batch(void);
void gem_renderer_render_batch(void);
void gem_renderer_draw_str(const char* str, const GemQuad* bounding_box);
const GemRenderStats* gem_renderer_get_stats(void);

