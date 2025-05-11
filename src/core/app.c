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

    if(file_to_open != NULL)
    {
        char* buf;
        size_t size;
        if(!gem_read_entire_file(file_to_open, &buf, &size))
            printf("Failed to find file: %s\n", file_to_open);
        if(buf[size - 1] == '\n')
            buf[size - 1] = '\0';
        piece_tree_init(&s_Buffer.contents, buf, false);
    }
    else
        piece_tree_init(&s_Buffer.contents, NULL, true);

    s_Buffer.cursor = (BufferPos){ 0, 0 };
    s_Buffer.camera_start_line = 0;
    s_Buffer.bounding_box.bottom_left[0] = 0.0f;
    s_Buffer.bounding_box.bottom_left[1] = 0.0f;

    s_Running = false;
    s_Redraw = false;
}

void gem_app_run(void)
{
    s_Running = true;
    s_Redraw = true;
    glClearColor(0.02f, 0.03f, 0.05f, 1.0f);
    while(s_Running)
    {
        if(s_Redraw)
        {
            int width, height;
            gem_window_get_dims(&width, &height);
            glClear(GL_COLOR_BUFFER_BIT);
            s_Buffer.bounding_box.top_right[0] = (float)width;
            s_Buffer.bounding_box.top_right[1] = (float)height;
            gem_renderer_start_batch();
            gem_renderer_draw_buffer(&s_Buffer);
            gem_renderer_render_batch();
            const GemRenderStats* stats = gem_renderer_get_stats();
            printf("Draw Calls: %2u\tQuad Count: %u\n", stats->draw_calls, stats->quad_count);
            gem_window_swap();
            s_Redraw = false;
        }
        gem_window_dispatch_events();
    }

    piece_tree_free(&s_Buffer.contents);
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

    if(keycode >= GEM_KEY_SPACE && keycode <= GEM_KEY_Z)
    {
        char c = mods & GEM_MOD_SHIFT ? 
                    SHIFT_CONVERSION[keycode - GEM_KEY_SPACE] : 
                    (char)keycode;
        printf("Pressed %c\n", c);
    }
    else if(keycode == GEM_KEY_DOWN && s_Buffer.contents.newline_count > s_Buffer.camera_start_line)
    {
        s_Buffer.camera_start_line++;
        s_Redraw = true;
    }
    else if(keycode == GEM_KEY_UP && s_Buffer.camera_start_line > 0)
    {
        s_Buffer.camera_start_line--;
        s_Redraw = true;
    }
    // else if(keycode == GEM_KEY_ENTER)
    // {
    //     char c = '\n';
    // }
    // else if(keycode == GEM_KEY_BACKSPACE)
    // {
    // }
    // else if(keycode == GEM_KEY_RIGHT)
    // {
    // }
    // else if(keycode == GEM_KEY_LEFT)
    // {
    // }
}

void gem_app_redraw(void)
{
    s_Redraw = true;
}
