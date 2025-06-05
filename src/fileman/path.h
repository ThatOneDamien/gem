#pragma once
#include "editor/bufferwin.h"

#include <limits.h>
#include <stdbool.h>

#if defined(PATH_MAX) && (PATH_MAX > 1000)
    #define GEM_PATH_MAX PATH_MAX
#else
    #define GEM_PATH_MAX 1024
#endif

char*  resolve_path(const char* path);
char*  get_cwd_path(void);
void   scan_bufwin_dir(BufferWin* bufwin);
bool   is_dir(DirEntry* ent);
