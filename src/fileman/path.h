#pragma once

#include <limits.h>
#include <stdbool.h>

#if defined(PATH_MAX) && (PATH_MAX > 1000)
    #define GEM_PATH_MAX PATH_MAX
#else
    #define GEM_PATH_MAX 1024
#endif

char* resolve_path(const char* path);
char* resolve_path_rel_to(const char* rel_path, const char* starting_dir);
char* get_cwd_path(void);
