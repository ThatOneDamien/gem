#include "app.h"
#include "core.h"
#include "keycode.h"
#include "timing.h"
#include "window.h"
#include "file/file.h"
#include "render/font.h"
#include "render/renderer.h"
#include "render/uniforms.h"
#include "structs/textbuffer.h"

#include <glad/glad.h>

#define GEM_INITIAL_WIDTH  1080
#define GEM_INITIAL_HEIGHT 720

static bool s_Running;
static bool s_Redraw;
static bool s_PrintStats;
static TextBuffer s_Buffer;

void gem_app_init(const char* file_to_open)
{
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

    if(file_to_open == NULL)
        text_buffer_open_empty(&s_Buffer);
    else
        text_buffer_open_file(&s_Buffer, file_to_open);

    s_Buffer.visible = true;
    s_Running = false;
    s_Redraw = false;
}

void gem_app_run(void)
{
    s_Running = true;
    s_Redraw = true;
    s_PrintStats = false;
    glClearColor(0.02f, 0.03f, 0.05f, 1.0f);
    while(s_Running)
    {
        if(s_Redraw)
        {
            int width, height;
            gem_window_get_dims(&width, &height);
            glClear(GL_COLOR_BUFFER_BIT);
            s_Buffer.bounding_box.tr.x = (float)width;
            s_Buffer.bounding_box.bl.y = (float)height;
            gem_renderer_start_batch();
            gem_renderer_draw_buffer(&s_Buffer);
            gem_renderer_render_batch();
            if(s_PrintStats)
            {
                const GemRenderStats* stats = gem_renderer_get_stats();
                printf("Draw Calls: %2u\tQuad Count: %u\n", stats->draw_calls, stats->quad_count);
            }
            gem_window_swap();
            s_Redraw = false;
        }
        gem_window_dispatch_events();
    }

    text_buffer_close(&s_Buffer);
    gem_renderer_cleanup();
    gem_freetype_cleanup();
    gem_window_destroy();
}

void gem_app_close(void) { s_Running = false; }

void gem_app_key_press(uint16_t keycode, uint32_t mods)
{
    static const char SHIFT_CONVERSION[GEM_KEY_Z - GEM_KEY_SPACE + 1] = 
        " \0\0\0\0\0\0\"\0\0\0\0<_>?)!@#$%^&*(\0:\0+"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0{|}\0\0~ABCDEFGHIJKLMNOPQR"
        "STUVWXYZ";
    if(mods & GEM_MOD_CONTROL)
    {
        if(keycode == GEM_KEY_P)
        {
            s_PrintStats = !s_PrintStats;
            printf("Printing stats: %s\n", s_PrintStats ? "on" : "off");
        }
        else if(keycode == GEM_KEY_T)
        {
            piece_tree_print_tree(&s_Buffer.contents);
        }
        return;
    }

    if(keycode >= GEM_KEY_SPACE && keycode <= GEM_KEY_Z)
    {
        char c[2];
        c[0] = mods & GEM_MOD_SHIFT ? 
                    SHIFT_CONVERSION[keycode - GEM_KEY_SPACE] : 
                    (char)keycode;
        c[1] = '\0';
        piece_tree_insert(&s_Buffer.contents, c, s_Buffer.cursor.offset);
        s_Buffer.cursor.pos.column++;
        s_Buffer.cursor.offset++;
        s_Redraw = true;
    }
    else if(keycode == GEM_KEY_ENTER)
    {
        char c[2] = "\n\0";
        piece_tree_insert(&s_Buffer.contents, c, s_Buffer.cursor.offset);
        s_Buffer.cursor.pos.line++;
        s_Buffer.cursor.pos.column = 0;
        s_Buffer.cursor.offset++;
        s_Redraw = true;
    }
    else if(keycode == GEM_KEY_BACKSPACE && s_Buffer.cursor.offset > 0)
    {
        piece_tree_delete(&s_Buffer.contents, s_Buffer.cursor.offset - 1, 1);
        text_buffer_move_cursor_horiz(&s_Buffer, -1);
    }
    else if(keycode == GEM_KEY_RIGHT)
        text_buffer_move_cursor_horiz(&s_Buffer, 1);
    else if(keycode == GEM_KEY_LEFT)
        text_buffer_move_cursor_horiz(&s_Buffer, -1);
    else if(keycode == GEM_KEY_DOWN)
        text_buffer_move_cursor_line(&s_Buffer, 1);
    else if(keycode == GEM_KEY_UP)
        text_buffer_move_cursor_line(&s_Buffer, -1);
}

void gem_app_mouse_press(uint32_t button, uint32_t mods, int x, int y)
{
    (void)mods;
    (void)x;
    (void)y;
    if(button == GEM_MOUSE_SCROLL_DOWN)
        text_buffer_move_camera(&s_Buffer, 3);
    else if(button == GEM_MOUSE_SCROLL_UP)
        text_buffer_move_camera(&s_Buffer, -3);
}

void gem_app_request_redraw(void)
{
    s_Redraw = true;
}

bool gem_app_needs_redraw(void)
{
    return s_Redraw;
}
