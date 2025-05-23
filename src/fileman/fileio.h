#pragma once
#include "editor/buffer.h"

#include <stdbool.h>
#include <stddef.h>

bool read_entire_file(const char* path, char** src, size_t* size);
bool write_buffer(BufNr bufnr, const char* path);
