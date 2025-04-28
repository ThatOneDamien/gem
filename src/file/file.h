#pragma once

#include <stdbool.h>
#include <stddef.h>

bool gem_read_entire_file(const char* path, char** src, size_t* size);
