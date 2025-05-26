#include "buffer.h"
#include "core/app.h"
#include "core/core.h"
#include "fileman/fileio.h"
#include "fileman/path.h"
#include "structs/da.h"

#define MAX_BUFFER_CNT 128
#define INITIAL_CAP    4

typedef struct Buffer Buffer;
struct Buffer
{
    PieceTree contents;
    char*     filepath;  // Absolute path for when multiple windows have different cwds
    BufNr     next;
    int       file_flags;
    bool      modified;
    bool      open;
};

typedef struct BufferStorage BufferStorage;
struct BufferStorage
{
    Buffer* buffers;
    size_t  capacity;
    size_t  free_count;
    BufNr   free_head;
};

static void    ensure_free(size_t count);
static bool    is_valid_buf(BufNr nr);
static Buffer* get_buf(BufNr nr);
static BufNr   alloc_buf(void);

static BufferStorage s_Buffers;

void buffer_list_init(void)
{
    s_Buffers.capacity = INITIAL_CAP;
    s_Buffers.buffers = malloc(s_Buffers.capacity * sizeof(Buffer));
    GEM_ENSURE(s_Buffers.buffers != NULL);
    s_Buffers.free_count = s_Buffers.capacity;
    s_Buffers.free_head = 0;
    for(size_t i = 0; i < s_Buffers.capacity - 1; ++i)
    {
        s_Buffers.buffers[i].open = false;
        s_Buffers.buffers[i].next = i + 1;
    }
    s_Buffers.buffers[s_Buffers.capacity - 1].open = false;
    s_Buffers.buffers[s_Buffers.capacity - 1].next = -1;
}

BufNr buffer_open_empty(void)
{
    ensure_free(1);
    BufNr res = alloc_buf();
    Buffer* buf = get_buf(res);
    buf->filepath = NULL;
    buf->modified = false;
    buf->next = -1;
    buf->open = true;
    piece_tree_init(&buf->contents, NULL, 0, false);
    return res;
}

BufNr buffer_open_file(char* filepath)
{
    GEM_ASSERT(filepath != NULL);
    char* full_path = resolve_path(filepath);
    if(full_path == NULL)
    {
        fprintf(stderr, "Failed to find file (parent directory doesn't exist): %s\n", filepath);
        return -1;
    }

    for(size_t i = 0; i < s_Buffers.capacity; ++i)
    {
        Buffer* buf = get_buf(i);
        if(buf->open && buf->filepath && strcmp(buf->filepath, full_path) == 0)
        {
            free(full_path);
            return i;
        }
    }

    ensure_free(1);
    BufNr res = alloc_buf();
    Buffer* buf = get_buf(res);
    buf->modified = false;
    buf->next = -1;
    buf->open = true;

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
        buf->filepath = full_path;
    }
    return res;
}

bool buffer_reopen(BufNr bufnr)
{
    GEM_ASSERT(is_valid_buf(bufnr));
    (void)bufnr;
    return false;
    // Buffer* buf = s_Buffers.data + bufnr;
    // if(buf->closed)
    //     return false;
    // buf->window_cnt++;
    // return true;
}

void buffer_close(BufNr bufnr)
{
    (void)bufnr;
    // GEM_ASSERT((size_t)bufnr < s_Buffers.size);
    // Buffer* buf = s_Buffers.data + bufnr;
    // GEM_ASSERT(!buf->closed);
    // if(--buf->window_cnt == 0)
    // {
    //     buf->closed = true;
    //     piece_tree_free(&buf->contents);
    // }
}

void close_all_buffers(void)
{
    // TODO: Check if they are modified and if so dont allow the closing
    // da_free_data(&s_Buffers);
}

const PieceTree* buffer_get_pt(BufNr bufnr)
{
    GEM_ASSERT(is_valid_buf(bufnr));
    Buffer* buf = s_Buffers.buffers + bufnr;
    return &buf->contents;
}

static void ensure_free(size_t count)
{
    if(count <= s_Buffers.free_count)
        return;
    GEM_ENSURE(s_Buffers.capacity < MAX_BUFFER_CNT);

    size_t new_cap = s_Buffers.capacity + count;
    new_cap += new_cap >> 1;
    s_Buffers.buffers = realloc(s_Buffers.buffers, sizeof(Buffer) * new_cap);
    GEM_ENSURE(s_Buffers.buffers != NULL);
    s_Buffers.free_count += new_cap - s_Buffers.capacity;
    s_Buffers.capacity = new_cap;
}

static UNUSED bool is_valid_buf(BufNr nr)
{
    return (size_t)nr < s_Buffers.capacity && s_Buffers.buffers[nr].open;
}

static Buffer* get_buf(BufNr nr)
{
    GEM_ASSERT((size_t)nr < s_Buffers.capacity);
    Buffer* buf = s_Buffers.buffers + nr;
    return buf;
}

static BufNr alloc_buf(void)
{
    GEM_ASSERT(s_Buffers.free_count > 0);
    BufNr res = s_Buffers.free_head;
    Buffer* prev = get_buf(s_Buffers.free_head);
    s_Buffers.free_head = prev->next;
    s_Buffers.free_count--;
    prev->next = -1;
    return res;
}
