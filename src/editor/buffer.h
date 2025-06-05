#pragma once
#include "structs/piecetree.h"
#include "structs/quad.h"

typedef int BufNr;
typedef struct Buffer Buffer;

struct Buffer
{
    PieceTree contents;
    char*     filepath;  // Absolute path for when multiple windows have different cwds
    BufNr     next;
    int       file_flags;
    int       soft_tab_width;
    bool      modified;
    bool      open;
};

extern Buffer* g_cur_buf;

void  buffer_list_init(void);

BufNr buffer_open_empty(void);
BufNr buffer_open_file(char* filepath);
bool  buffer_reopen(BufNr bufnr);
void  buffer_close(BufNr bufnr, bool force);
void  close_all_buffers(bool force);
int   open_buffer_count(void);

void  buffer_insert(BufNr bufnr, const char* str, size_t len, size_t offset);
void  buffer_insert_repeat(BufNr bufnr, const char* str, size_t len, size_t count, size_t offset);
void  buffer_delete(BufNr bufnr, size_t offset, size_t count);

Buffer* buffer_get(BufNr bufnr);
