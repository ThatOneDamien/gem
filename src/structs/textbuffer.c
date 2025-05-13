#include "textbuffer.h"
#include "core/app.h"
#include "core/core.h"
#include "file/file.h"

#include <string.h>

#define MIN(x, y) ((x) > (y) ? (y) : (x))

void text_buffer_open_empty(TextBuffer* buf)
{
    GEM_ASSERT(buf != NULL);

    memset(buf, 0, sizeof(TextBuffer));
    piece_tree_init(&buf->contents, NULL, 0, false);
}

void text_buffer_open_file(TextBuffer* buf, const char* filepath)
{
    GEM_ASSERT(buf != NULL);
    GEM_ASSERT(filepath != NULL);

    memset(buf, 0, sizeof(TextBuffer));

    char* contents;
    size_t size;
    if(!gem_read_entire_file(filepath, &contents, &size))
    {
        printf("Failed to find file: %s\n", filepath);
        piece_tree_init(&buf->contents, NULL, 0, false);
    }
    else
    {
        if(contents[size - 1] == '\n')
            size--;
        piece_tree_init(&buf->contents, contents, size, false);
    }
}

void text_buffer_close(TextBuffer* buf)
{
    GEM_ASSERT(buf != NULL);
    piece_tree_free(&buf->contents);
}

void text_buffer_move_camera(TextBuffer* buf, int64_t line_delta)
{
    GEM_ASSERT(buf != NULL);
    if(line_delta == 0)
        return;

    size_t delta = line_delta < 0 ? 
                        -MIN(-(size_t)line_delta, buf->camera_start_line) :
                        MIN((size_t)line_delta, 
                            buf->contents.line_cnt - 1 - buf->camera_start_line);
    buf->camera_start_line += delta;
    if(delta != 0 && buf->visible)
        gem_app_request_redraw();
}
