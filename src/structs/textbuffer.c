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

static void clamp_val(int64_t* val, int64_t min, int64_t max)
{
    GEM_ASSERT(min <= max);
    if(min == max || *val < min)
        *val = min;
    else if(*val > max)
        *val = max;
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

    clamp_val(&line_delta, -buf->camera_start_line, buf->contents.line_cnt - 1 - buf->camera_start_line);
    buf->camera_start_line += line_delta;
    if(buf->visible && line_delta != 0)
        gem_app_request_redraw();
}

void text_buffer_move_cursor_line(TextBuffer* buf, int64_t line_delta)
{
    GEM_ASSERT(buf != NULL);
    if(line_delta == 0)
        return;

    BufferPos* pos = &buf->cursor.pos;
    clamp_val(&line_delta, -pos->line, buf->contents.line_cnt - 1 - pos->line);
    if(line_delta != 0)
    {
        pos->line += line_delta;
        pos->column = MIN((int64_t)piece_tree_get_line_length(&buf->contents, pos->line), buf->cursor.horiz);
        buf->cursor.offset = piece_tree_get_offset_bp(&buf->contents, *pos);
        if(buf->visible)
            gem_app_request_redraw();
    }
}

void text_buffer_move_cursor_horiz(TextBuffer* buf, int64_t horiz_delta)
{
    GEM_ASSERT(buf != NULL);
    if(horiz_delta == 0)
        return;

    clamp_val(&horiz_delta, -buf->cursor.offset, buf->contents.size - buf->cursor.offset);
    buf->cursor.offset += horiz_delta;
    if(horiz_delta != 0)
    {
        buf->cursor.pos = piece_tree_get_buffer_pos(&buf->contents, buf->cursor.offset);
        buf->cursor.horiz = buf->cursor.pos.column;
        if(buf->visible)
            gem_app_request_redraw();
    }
}

