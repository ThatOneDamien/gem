#pragma once
#include "font.h"
#include "structs/quad.h"
#include "editor/bufferwin.h"

typedef struct GemRenderStats GemRenderStats;
struct GemRenderStats
{
    uint32_t draw_calls;
    uint32_t quad_count;
};

void renderer_init(void);
void renderer_cleanup(void);
void renderer_start_batch(void);
void renderer_render_batch(void);
void renderer_draw_bufwin(const BufferWin* bufwin, bool active);

const GemRenderStats* renderer_get_stats(void);
const GemFont* gem_get_font(void);

