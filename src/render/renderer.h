#pragma once
#include "structs/quad.h"
#include "structs/textbuffer.h"

typedef struct
{
    uint32_t draw_calls;
    uint32_t quad_count;
} GemRenderStats;

void gem_renderer_init(void);
void gem_renderer_cleanup(void);
void gem_renderer_start_batch(void);
void gem_renderer_render_batch(void);
void gem_renderer_draw_str_at(const char* str, size_t count, const GemQuad* bounding_box, float* penX, float* penY);
void gem_renderer_draw_buffer(const TextBuffer* buffer);
const GemRenderStats* gem_renderer_get_stats(void);

