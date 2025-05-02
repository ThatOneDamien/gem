#include "app.h"
#include "core.h"
#include "window.h"
#include "render/font.h"
#include "render/renderer.h"
#include "render/uniforms.h"
#include "timing.h"

#include <glad/glad.h>

#include <time.h>

#define GEM_INITIAL_WIDTH  1080
#define GEM_INITIAL_HEIGHT 720

static bool s_Running = false;

void gem_app_init(UNUSED int argc, UNUSED char* argv[])
{
    gem_window_create(GEM_INITIAL_WIDTH, GEM_INITIAL_HEIGHT);
    gem_freetype_init();
    gem_renderer_init();
    int width, height;
    gem_window_get_dims(&width, &height);
    gem_set_projection(width, height);
}

void gem_app_run(void)
{
    s_Running = true;
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
    // int prev_width = 0, prev_height = 0;
    while(s_Running)
    {
        // Will block until window is focused, ensuring no unnecessary CPU
        // utilization, if later on I feel the need to render things even
        // when the window isnt focused, I will add that.
        gem_window_dispatch_events();

        int width, height;
        gem_window_get_dims(&width, &height);
        glClear(GL_COLOR_BUFFER_BIT);
        GemQuad bounding_box = {
            .bottom_left = { 0.0f, 0.0f },
            .top_right = { (float)width, (float)height }
        };
        gem_draw_str("Bruh", 4, &bounding_box);
        gem_window_swap();
    }

    gem_renderer_cleanup();
    gem_freetype_cleanup();
    gem_window_destroy();
}

void gem_app_close(void) { s_Running = false; }
