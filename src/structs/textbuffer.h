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
    BufferPos start;
    BufferPos count;
} View;

typedef struct
{
    PieceTree   contents;
    Cursor      cursor; // May extend to multiple cursors later
    View        view;
    GemQuad     bounding_box;
    GemPadding  text_padding;
    const char* filepath;
    bool        visible;
    bool        readonly;
} TextBuffer;

void text_buffer_open_empty(TextBuffer* buf);
void text_buffer_open_file(TextBuffer* buf, const char* filepath);
void text_buffer_close(TextBuffer* buf);
void text_buffer_update_view(TextBuffer* buf);
void text_buffer_cursor_refresh(TextBuffer* buf);

void text_buffer_set_cursor(TextBuffer* buf, int64_t line, int64_t column);
void text_buffer_move_cursor_line(TextBuffer* buf, int64_t line_delta);
void text_buffer_move_cursor_horiz(TextBuffer* buf, int64_t horiz_delta);

void text_buffer_set_view(TextBuffer* buf, int64_t start_line, int64_t start_col);
void text_buffer_move_view(TextBuffer* buf, int64_t line_delta);
void text_buffer_put_cursor_in_view(TextBuffer* buf);

void text_buffer_print_cursor_loc(const TextBuffer* buf);

// inline void text_buffer_set_cursor_bp(TextBuffer* buf, BufferPos pos)
// { 
//     text_buffer_set_cursor(buf, pos.line, pos.column);
// }
