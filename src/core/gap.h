#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct _GapBuffer* GapBuffer;

GapBuffer gap_create(size_t initial_capacity);
void gap_free(GapBuffer buf);

size_t gap_ensure_cap(GapBuffer buf, size_t new_capacity);
void gap_append(GapBuffer buf, const char* data, size_t size);
void gap_set_gap(GapBuffer buf, size_t new_gap_pos);
void gap_move_gap(GapBuffer buf, int64_t relative_offset);
void gap_del_back(GapBuffer buf, size_t count);
void gap_del_forw(GapBuffer buf, size_t count);

size_t gap_get_size(GapBuffer buf);
size_t gap_get_gap_pos(GapBuffer buf);
const char* gap_get_data_before_gap(GapBuffer buf);
const char* gap_get_data_after_gap(GapBuffer buf);
char gap_char_at(GapBuffer buf, size_t pos);
char gap_set_at(GapBuffer buf, size_t pos, char c);

void gap_print_buffer(GapBuffer buf);
static inline void gap_append_str(GapBuffer buf, const char* str)
{
    gap_append(buf, str, strlen(str));
}
