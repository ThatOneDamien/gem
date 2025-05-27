#pragma once
#include "editor/buffer.h"

#include <stdbool.h>
#include <stddef.h>

bool read_entire_file(const char* path, char** src, size_t* size);
bool write_buffer_as(BufNr bufnr, const char* path);

static inline bool write_buffer(BufNr bufnr)
{ 
    return write_buffer_as(bufnr, buffer_get_path(bufnr)); 
}
