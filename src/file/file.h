#pragma once

#include <stddef.h>
#include <stdbool.h>

bool gem_read_entire_file(const char* path, char** src, size_t* size);
