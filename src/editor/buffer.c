#include "buffer.h"
#include "core/app.h"
#include "core/core.h"
#include "fileman/fileio.h"
#include "structs/da.h"

#define MAX_BUFFER_CNT 128

typedef struct Buffer Buffer;
struct Buffer
{
    PieceTree   contents;
    const char* filepath;
    int         window_cnt;
    bool        readonly;
    bool        modified;
    bool        closed;
};

typedef struct BufferDA BufferDA;
struct BufferDA
{
    Buffer* data;
    size_t  size;
    size_t  capacity;
};

static BufferDA s_Buffers;

void buffer_list_init(void)
{
    da_init(&s_Buffers, 0);
}

BufNr buffer_open_empty(void)
{
    GEM_ENSURE(s_Buffers.size < MAX_BUFFER_CNT);
    BufNr res = s_Buffers.size;
    da_resize(&s_Buffers, s_Buffers.size + 1);
    Buffer* buf = s_Buffers.data + res;
    memset(buf, 0, sizeof(Buffer));
    buf->window_cnt = 1;
    piece_tree_init(&buf->contents, NULL, 0, false);
    return res;
}

BufNr buffer_open_file(const char* filepath)
{
    GEM_ASSERT(filepath != NULL);

    BufNr res = s_Buffers.size;
    da_resize(&s_Buffers, s_Buffers.size + 1);
    Buffer* buf = s_Buffers.data + res;
    memset(buf, 0, sizeof(Buffer));
    buf->window_cnt = 1;

    char* contents;
    size_t size;
    if(!read_entire_file(filepath, &contents, &size))
    {
        fprintf(stderr, "Failed to find file: %s\n", filepath);
        piece_tree_init(&buf->contents, NULL, 0, false);
    }
    else
    {
        if(contents[size - 1] == '\n')
            size--;
        piece_tree_init(&buf->contents, contents, size, false);
        buf->filepath = filepath;
    }
    return res;
}

bool buffer_reopen(BufNr bufnr)
{
    GEM_ASSERT((size_t)bufnr < s_Buffers.size);
    Buffer* buf = s_Buffers.data + bufnr;
    if(buf->closed)
        return false;
    buf->window_cnt++;
    return true;
}

void buffer_close(BufNr bufnr)
{
    GEM_ASSERT((size_t)bufnr < s_Buffers.size);
    Buffer* buf = s_Buffers.data + bufnr;
    GEM_ASSERT(!buf->closed);
    if(--buf->window_cnt == 0)
    {
        buf->closed = true;
        piece_tree_free(&buf->contents);
    }
}

void close_all_buffers(void)
{
    // TODO: Check if they are modified and if so dont allow the closing
    da_free_data(&s_Buffers);
}

const PieceTree* buffer_get_pt(BufNr bufnr)
{
    GEM_ASSERT((size_t)bufnr < s_Buffers.size);
    Buffer* buf = s_Buffers.data + bufnr;
    GEM_ASSERT(!buf->closed);
    return &buf->contents;
}
