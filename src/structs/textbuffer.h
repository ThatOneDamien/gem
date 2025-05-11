#pragma once
#include "piecetree.h"
#include "quad.h"

typedef struct
{
    PieceTree contents;
    BufferPos cursor; // May extend to multiple cursors later
    size_t    camera_start_line;
    GemQuad   bounding_box;
} TextBuffer;

