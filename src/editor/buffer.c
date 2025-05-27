#include "buffer.h"
#include "core/app.h"
#include "core/core.h"
#include "fileman/fileio.h"
#include "fileman/path.h"
#include "structs/da.h"

#define MAX_BUFFER_CNT 128
#define INITIAL_CAP    4

#define FF_READONLY    1

typedef struct BufferStorage BufferStorage;
struct BufferStorage
{
    Buffer* buffers;
    size_t  capacity;
    size_t  free_count;
    BufNr   free_head;
};

static void  mark_free(size_t start);
static void  ensure_free(size_t count);
static bool  is_valid_buf(BufNr nr);
static BufNr alloc_buf(void);
static void  free_buf(BufNr nr);

static BufferStorage s_Buffers;

void buffer_list_init(void)
{
    s_Buffers.capacity = INITIAL_CAP;
    s_Buffers.buffers = malloc(s_Buffers.capacity * sizeof(Buffer));
    GEM_ENSURE(s_Buffers.buffers != NULL);
    s_Buffers.free_count = 0;
    s_Buffers.free_head = -1;
    mark_free(0);
}

BufNr buffer_open_empty(void)
{
    ensure_free(1);
    BufNr res = alloc_buf();
    Buffer* buf = buffer_get(res);
    buf->filepath = NULL;
    buf->modified = false;
    buf->next = -1;
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
        Buffer* buf = s_Buffers.buffers + i;
        if(buf->open && buf->filepath && strcmp(buf->filepath, full_path) == 0)
        {
            free(full_path);
            return i;
        }
    }

    ensure_free(1);
    BufNr res = alloc_buf();
    Buffer* buf = buffer_get(res);
    buf->modified = false;
    buf->next = -1;
    buf->open = true;

    char* contents;
    size_t size;
    bool readonly;
    if(!read_entire_file(filepath, &contents, &size, &readonly))
    {
        fprintf(stderr, "Failed to find file: %s\n", filepath);
        piece_tree_init(&buf->contents, NULL, 0, false);
    }
    else
    {
        if(size > 0 && contents[size - 1] == '\n')
            size--;
        piece_tree_init(&buf->contents, contents, size, false);
        buf->file_flags = readonly ? FF_READONLY : 0;
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

void buffer_close(BufNr bufnr, bool force)
{
    Buffer* buf = buffer_get(bufnr);
    if(!force && buf->modified)
        printf("Attempted to close a buffer that has been modified.\n");
    else
        free_buf(bufnr);
}

void close_all_buffers(bool force)
{
    for(size_t i = 0; i < s_Buffers.capacity - 1; ++i)
    {
        Buffer* buf = s_Buffers.buffers + i;
        if(buf->open)
        {
            if(!force && buf->modified)
                printf("Attempted to close a buffer that has been modified.\n");
            else
                free_buf(i);
        }
    }
}

int open_buffer_count(void)
{
    return s_Buffers.capacity - s_Buffers.free_count;
}

void buffer_insert(BufNr bufnr, const char* str, size_t len, size_t offset)
{
    GEM_ASSERT(str != NULL);
    Buffer* buf = buffer_get(bufnr);
    if(buf->file_flags & FF_READONLY)
    {
        printf("Tried to modify a readonly buffer.\n");
        return;
    }
    piece_tree_insert(&buf->contents, str, len, offset);
    buf->modified = true;
}

void buffer_insert_repeat(BufNr bufnr, const char* str, size_t len, size_t count, size_t offset)
{
    GEM_ASSERT(str != NULL);
    Buffer* buf = buffer_get(bufnr);
    if(buf->file_flags & FF_READONLY)
    {
        printf("Tried to modify a readonly buffer.\n");
        return;
    }
    piece_tree_insert_repeat(&buf->contents, str, len, count, offset);
    buf->modified = true;
}

void buffer_delete(BufNr bufnr, size_t offset, size_t count)
{
    Buffer* buf = buffer_get(bufnr);
    if(buf->file_flags & FF_READONLY)
    {
        printf("Tried to modify a readonly buffer.\n");
        return;
    }
    piece_tree_delete(&buf->contents, offset, count);
    buf->modified = true;

}

Buffer* buffer_get(BufNr bufnr)
{
    GEM_ASSERT(is_valid_buf(bufnr));
    return s_Buffers.buffers + bufnr;
}

static void mark_free(size_t start)
{
    GEM_ASSERT(s_Buffers.capacity > 0);
    GEM_ASSERT(start < s_Buffers.capacity);
    Buffer* last;
    if(s_Buffers.free_head == -1)
        s_Buffers.free_head = start;
    else
    {
        last = s_Buffers.buffers + s_Buffers.free_head;
        while(last->next != -1)
            last = s_Buffers.buffers + last->next;
        last->next = start;
    }
    last = s_Buffers.buffers + start;
    for(size_t i = start + 1; i < s_Buffers.capacity; ++i)
    {
        last->next = i;
        last->open = false;
        last++;
    }
    last->next = -1;
    last->open = false;
    s_Buffers.free_count += s_Buffers.capacity - start;
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
    
    s_Buffers.capacity ^= new_cap;
    new_cap ^= s_Buffers.capacity;
    s_Buffers.capacity ^= new_cap;
    mark_free(new_cap);
}

static UNUSED bool is_valid_buf(BufNr nr)
{
    return (size_t)nr < s_Buffers.capacity && 
           s_Buffers.buffers[nr].open;
}


static BufNr alloc_buf(void)
{
    GEM_ASSERT(s_Buffers.free_count > 0);
    BufNr res = s_Buffers.free_head;
    Buffer* prev = s_Buffers.buffers + s_Buffers.free_head;
    s_Buffers.free_head = prev->next;
    s_Buffers.free_count--;
    prev->next = -1;
    prev->open = true;
    return res;
}

static void free_buf(BufNr nr)
{
    Buffer* buf = buffer_get(nr);
    buf->next = s_Buffers.free_head;
    s_Buffers.free_head = nr;
    free(buf->filepath);
    buf->open = false;
    piece_tree_free(&buf->contents);
    s_Buffers.free_count++;
}
