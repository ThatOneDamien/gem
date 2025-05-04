#include "app.h"
#include "core.h"
#include "keycode.h"
#include "window.h"
#include "render/font.h"
#include "render/renderer.h"
#include "render/uniforms.h"
#include "timing.h"

#include <glad/glad.h>


#define GEM_INITIAL_WIDTH  1080
#define GEM_INITIAL_HEIGHT 720

static bool s_Running = false;
static GapBuffer s_Buffer;

void gem_app_init(UNUSED int argc, UNUSED char* argv[])
{
    gem_window_create(GEM_INITIAL_WIDTH, GEM_INITIAL_HEIGHT);
    gem_freetype_init();
    gem_renderer_init();
    int width, height;
    gem_window_get_dims(&width, &height);
    gem_set_projection(width, height);

    s_Buffer = gap_create(0);
    gap_append_str(s_Buffer, "Hello this is a test. I love my babe.");
}

void gem_app_run(void)
{
    s_Running = true;
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    while(s_Running)
    {
        int width, height;
        gem_window_get_dims(&width, &height);
        glClear(GL_COLOR_BUFFER_BIT);
        GemQuad bounding_box = {
            .bottom_left = { 0.0f, 0.0f },
            .top_right = { (float)width, (float)height }
        };

        gem_renderer_start_batch();
        gem_renderer_draw_buffer(s_Buffer, &bounding_box);
        gem_renderer_render_batch();
        // const GemRenderStats* stats = gem_renderer_get_stats();
        // printf("Draw Calls: %u\t\tQuad Count: %u\n", stats->draw_calls, stats->quad_count);
        gem_window_swap();
        gem_window_dispatch_events();
    }

    gem_renderer_cleanup();
    gem_freetype_cleanup();
    gem_window_destroy();
}

void gem_app_close(void) { s_Running = false; }

void gem_app_key_press(uint16_t keycode)
{
    if(keycode >= GEM_KEY_SPACE && keycode <= GEM_KEY_Z)
    {
        char c = ' ' + keycode - GEM_KEY_SPACE;
        gap_append(s_Buffer, &c, 1);
    }
    else if(keycode == GEM_KEY_ENTER)
    {
        char c = '\n';
        gap_append(s_Buffer, &c, 1);
    }
    else if(keycode == GEM_KEY_BACKSPACE)
    {
        gap_del_back(s_Buffer, 1);
    }
    else if(keycode == GEM_KEY_RIGHT)
    {
        gap_move_gap(s_Buffer, 1);
    }
    else if(keycode == GEM_KEY_LEFT)
    {
        gap_move_gap(s_Buffer, -1);
    }
}
