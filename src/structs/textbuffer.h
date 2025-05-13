#pragma once
#include "piecetree.h"
#include "quad.h"

typedef struct
{
    PieceTree contents;
    BufferPos cursor; // May extend to multiple cursors later
    size_t    camera_start_line;
    GemQuad   bounding_box;
    bool      visible;
    bool      readonly;
} TextBuffer;

void text_buffer_open_empty(TextBuffer* buf);
void text_buffer_open_file(TextBuffer* buf, const char* filepath);
void text_buffer_close(TextBuffer* buf);

void text_buffer_move_camera(TextBuffer* buf, int64_t line_delta);
