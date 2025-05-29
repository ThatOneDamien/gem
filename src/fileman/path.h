#pragma once

#include <limits.h>
#include <stdbool.h>

#if defined(PATH_MAX) && (PATH_MAX > 1000)
    #define GEM_PATH_MAX PATH_MAX
#else
    #define GEM_PATH_MAX 1024
#endif

char* resolve_path(const char* path);
