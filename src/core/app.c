#include "app.h"
#include "core.h"
#include "keycode.h"
#include "timing.h"
#include "window.h"
#include "file/file.h"
#include "render/font.h"
#include "render/renderer.h"
#include "render/uniforms.h"
#include "structs/piecetree.h"

#include <glad/glad.h>

#define GEM_INITIAL_WIDTH  1080
#define GEM_INITIAL_HEIGHT 720

static bool s_Running = false;
static PieceTree s_Buffer;

void gem_app_init(int argc, char* argv[])
{
    GEM_ENSURE(argc > 0);
    // Print welcome message (TEMPORARY)
    printf("\n");
    printf("-------------------------\n");
    printf("|    Welcome to Gem!    |\n");
    printf("-------------------------\n\n");

    // Create window, and initialize OpenGL context, renderer,
    // and freetype.
    gem_window_create(GEM_INITIAL_WIDTH, GEM_INITIAL_HEIGHT);
    gem_freetype_init();
    gem_renderer_init();

    int width, height;
    gem_window_get_dims(&width, &height);
    gem_set_projection(width, height);

    if(argc > 1)
    {
        // TODO: Handle multiple arguments down the line.
        char* buf;
        size_t size;
        if(!gem_read_entire_file(argv[1], &buf, &size))
            printf("Failed to find file: %s\n", argv[1]);
        piece_tree_init(&s_Buffer, buf, false);
    }
    else
        piece_tree_init(&s_Buffer, NULL, true);
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
        piece_tree_render(&s_Buffer, &bounding_box);
        gem_renderer_render_batch();
        // const GemRenderStats* stats = gem_renderer_get_stats();
        // printf("Draw Calls: %u\t\tQuad Count: %u\n", stats->draw_calls, stats->quad_count);
        gem_window_swap();
        gem_window_dispatch_events();
    }

    piece_tree_free(&s_Buffer);
    gem_renderer_cleanup();
    gem_freetype_cleanup();
    gem_window_destroy();
}

void gem_app_close(void) { s_Running = false; }

void gem_app_key_press(uint16_t keycode)
{
    (void)keycode;
    // if(keycode >= GEM_KEY_SPACE && keycode <= GEM_KEY_Z)
    // {
    //     char c = ' ' + keycode - GEM_KEY_SPACE;
    //     gap_append(s_Buffer, &c, 1);
    // }
    // else if(keycode == GEM_KEY_ENTER)
    // {
    //     char c = '\n';
    //     gap_append(s_Buffer, &c, 1);
    // }
    // else if(keycode == GEM_KEY_BACKSPACE)
    // {
    //     gap_del_back(s_Buffer, 1);
    // }
    // else if(keycode == GEM_KEY_RIGHT)
    // {
    //     gap_move_gap(s_Buffer, 1);
    // }
    // else if(keycode == GEM_KEY_LEFT)
    // {
    //     gap_move_gap(s_Buffer, -1);
    // }
}
