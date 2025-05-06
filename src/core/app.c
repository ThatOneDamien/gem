#include "app.h"
#include "core.h"
#include "keycode.h"
#include "timing.h"
#include "window.h"
#include "render/font.h"
#include "render/renderer.h"
#include "render/uniforms.h"
#include "structs/piecetree.h"

#include <glad/glad.h>


#define GEM_INITIAL_WIDTH  1080
#define GEM_INITIAL_HEIGHT 720

static bool s_Running = false;

void gem_app_init(UNUSED int argc, UNUSED char* argv[])
{
    printf("\n");
    printf("-------------------------\n");
    printf("|    Welcome to Gem!    |\n");
    printf("-------------------------\n\n");
    gem_window_create(GEM_INITIAL_WIDTH, GEM_INITIAL_HEIGHT);
    gem_freetype_init();
    gem_renderer_init();
    int width, height;
    gem_window_get_dims(&width, &height);
    gem_set_projection(width, height);
    GemPieceTree tb;
    gem_piece_tree_init(&tb, "Testing. ", true);
    gem_piece_tree_freelist(&tb);
    // gem_piece_tree_print(&tb);
    // printf("\n-----------------\n");
    // gem_piece_tree_insert(&tb, "Should be appended", 13);
    // gem_piece_tree_print(&tb);
    gem_piece_tree_free(&tb);

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
        // GemQuad bounding_box = {
        //     .bottom_left = { 0.0f, 0.0f },
        //     .top_right = { (float)width, (float)height }
        // };

        gem_renderer_start_batch();
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
