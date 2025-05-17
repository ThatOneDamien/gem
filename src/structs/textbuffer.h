#pragma once
#include "piecetree.h"
#include "quad.h"

typedef struct
{
    BufferPos pos;
    BufferPos vis;
    size_t    offset;
} Cursor;


typedef struct
{
    PieceTree   contents;
    Cursor      cursor; // May extend to multiple cursors later
    int64_t     camera_start_line;
    GemQuad     bounding_box;
    const char* filepath;
    bool        visible;
    bool        readonly;
} TextBuffer;

void text_buffer_open_empty(TextBuffer* buf);
void text_buffer_open_file(TextBuffer* buf, const char* filepath);
void text_buffer_close(TextBuffer* buf);

// void text_buffer_set_cursor(TextBuffer* buf, int64_t line, int64_t column);
void text_buffer_move_cursor_line(TextBuffer* buf, int64_t line_delta);
void text_buffer_move_cursor_horiz(TextBuffer* buf, int64_t horiz_delta);

// void text_buffer_set_camera(TextBuffer* buf, int64_t camera_start_line);
void text_buffer_move_camera(TextBuffer* buf, int64_t line_delta);
// void text_buffer_center_camera(TextBuffer* buf);

// inline void text_buffer_set_cursor_bp(TextBuffer* buf, BufferPos pos)
// { 
//     text_buffer_set_cursor(buf, pos.line, pos.column);
// }
