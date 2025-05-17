#pragma once
#include "structs/textbuffer.h"

#include <stdbool.h>
#include <stddef.h>

bool gem_read_entire_file(const char* path, char** src, size_t* size);
bool gem_write_text_buffer(const TextBuffer* buf, const char* path);
