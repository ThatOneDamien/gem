#include "app.h"
#include "core.h"
#include "keycode.h"
#include "timing.h"
#include "window.h"
#include "fileman/fileio.h"
#include "render/font.h"
#include "render/renderer.h"
#include "render/uniforms.h"
#include "editor/bufferwin.h"

#include <glad/glad.h>

#define GEM_INITIAL_WIDTH  1080
#define GEM_INITIAL_HEIGHT 720

static bool s_Redraw;
static bool s_PrintStats;

void gem_init(char* file_to_open)
{
    // Create window, and initialize OpenGL context, renderer,
    // and freetype.
    window_create(GEM_INITIAL_WIDTH, GEM_INITIAL_HEIGHT);
    freetype_init();
    renderer_init();
    bufwin_init_root_frame();
    buffer_list_init();
    bufwin_open(file_to_open);

    int width, height;
    window_get_dims(&width, &height);
    set_projection(width, height);
    bufwin_update_screen(width, height);

    s_Redraw = false;
}

void gem_run(void)
{
    s_Redraw = true;
    s_PrintStats = false;
    // glClearColor(0.02f, 0.03f, 0.05f, 1.0f);
    while(true)
    {
        if(s_Redraw)
        {
            // glClear(GL_COLOR_BUFFER_BIT);
            renderer_start_batch();
            bufwin_render_all();
            renderer_render_batch();
            if(s_PrintStats)
            {
                const GemRenderStats* stats = renderer_get_stats();
                printf("Draw Calls: %2u\tQuad Count: %u\n", stats->draw_calls, stats->quad_count);
            }
            window_swap();
            s_Redraw = false;
        }
        window_dispatch_events();
    }
}

void gem_close(int err)
{ 
    renderer_cleanup();
    freetype_cleanup();
    window_destroy();
    exit(err);
}

void gem_key_press(uint16_t keycode, uint32_t mods)
{
    bool handeled = true;
    if(mods & GEM_MOD_CONTROL)
    {
        if(keycode == GEM_KEY_P)
        {
            s_PrintStats = !s_PrintStats;
            printf("Printing stats turned %s.\n", s_PrintStats ? "on" : "off");
        }
        else if(keycode == GEM_KEY_C)
        {
            bufwin_close();
            s_Redraw = true;
        }
        else if(keycode == GEM_KEY_V)
        {
            bufwin_split(~mods & GEM_MOD_SHIFT);
            s_Redraw = true;
        }
        else
            handeled = false;
    }
    else if(mods & GEM_MOD_ALT && keycode == GEM_KEY_ENTER)
    {
        window_toggle_maximize();
        return;
    }
    else
        handeled = false;
    if(!handeled)
        bufwin_key_press(keycode, mods);
}

void gem_mouse_press(uint32_t button, uint32_t mods, int sequence, int x, int y)
{
    bufwin_mouse_press(button, mods, sequence, x, y);
}

void gem_request_redraw(void)
{
    s_Redraw = true;
}

bool gem_needs_redraw(void)
{
    return s_Redraw;
}
