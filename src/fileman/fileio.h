#pragma once
#include "editor/buffer.h"

#include <stdbool.h>
#include <stddef.h>

bool read_entire_file(const char* path, char** src, size_t* size, bool* readonly);
bool save_buffer_as(BufNr bufnr, const char* path);

static inline bool save_buffer(BufNr bufnr)
{ 
    return save_buffer_as(bufnr, NULL);
}
