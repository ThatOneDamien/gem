#include "gap.h"
#include "core.h"

typedef struct _GapBuffer
{
    char* data;
    size_t size;
    size_t capacity;
    size_t gap_pos;
} _GapBuffer;

GapBuffer gap_create(size_t initial_capacity)
{
    GapBuffer buf = malloc(sizeof(_GapBuffer));
    GEM_ENSURE(buf != NULL);

    buf->size = 0;
    buf->capacity = initial_capacity > 10 ? initial_capacity : 10;
    buf->gap_pos = 0;
    buf->data = malloc(buf->capacity);
    GEM_ENSURE(buf->data != NULL);
    return buf;
}

void gap_free(GapBuffer buf)
{
    GEM_ASSERT(buf != NULL);
    free(buf->data);
    free(buf);
}

size_t gap_ensure_cap(GapBuffer buf, size_t new_capacity)
{
    GEM_ASSERT(buf != NULL);
    if(buf->capacity >= new_capacity)
        return buf->capacity;

    buf->data = realloc(buf->data, new_capacity);
    GEM_ENSURE(buf->data != NULL);
    buf->capacity = new_capacity;
    return new_capacity;
}

char gap_get_pos(GapBuffer buf, size_t pos)
{
    GEM_ASSERT(buf != NULL);
    return pos < buf->gap_pos ? buf->data[pos] : buf->data[buf->capacity - buf->size + pos];
}

void gap_append(GapBuffer buf, const char* data, size_t size)
{
    GEM_ASSERT(buf != NULL);
    GEM_ASSERT(data != NULL);
    if(size == 0)
        return;

    buf->size += size;
    if(buf->size > buf->capacity)
        gap_ensure_cap(buf, buf->size + (buf->size >> 1));

    memcpy(buf->data + buf->gap_pos, data, size);
    buf->gap_pos += size;
}

void gap_set_gap(GapBuffer buf, size_t new_gap_pos)
{
    GEM_ASSERT(buf != NULL);
    GEM_ASSERT(new_gap_pos <= buf->size);
    if(new_gap_pos == buf->gap_pos)
        return;
    if(new_gap_pos < buf->gap_pos)
    {
        size_t count = buf->gap_pos - new_gap_pos;
        memmove(buf->data + (buf->capacity - buf->size + new_gap_pos), buf->data + new_gap_pos,
                count);
    }
    else
    {
        size_t count = new_gap_pos - buf->gap_pos;
        memmove(buf->data + buf->gap_pos, buf->data + (buf->capacity - buf->size + buf->gap_pos),
                count);
    }
    buf->gap_pos = new_gap_pos;
}

void gap_move_gap(GapBuffer buf, int64_t relative_offset)
{
    GEM_ASSERT(buf != NULL);
    if(relative_offset == 0)
        return;
    size_t new_pos = (size_t)relative_offset + buf->gap_pos;
    GEM_ASSERT(new_pos <= buf->size);
    gap_set_gap(buf, new_pos);
}

void gap_del_back(GapBuffer buf, size_t count)
{
    GEM_ASSERT(buf != NULL);
    if(count == 0)
        return;
    buf->gap_pos = count > buf->gap_pos ? 0 : buf->gap_pos - count;
    buf->size = count > buf->size ? 0 : buf->size - count;
}

void gap_del_forw(GapBuffer buf, size_t count)
{
    GEM_ASSERT(buf != NULL);
    if(count == 0)
        return;
    buf->size = count > buf->size ? 0 : buf->size - count;
}

void gap_print_buffer(GapBuffer buf)
{
    GEM_ASSERT(buf != NULL);
    puts("Before Gap\n--------------------");
    for(size_t i = 0; i < buf->gap_pos; ++i)
        putc(buf->data[i], stdout);

    puts("\n\nAfter Gap\n--------------------");
    for(size_t i = buf->capacity - buf->size + buf->gap_pos; i < buf->capacity; ++i)
        putc(buf->data[i], stdout);
    putc('\n', stdout);
}
