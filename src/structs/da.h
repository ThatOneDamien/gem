#pragma once
#include "core/core.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline void __da_reserve_impl(void** data_ref, size_t* cap_ref, 
                                     size_t desired_cap, size_t data_size)
{
    if(*cap_ref >= desired_cap)
        return;

    desired_cap += desired_cap >> 1;
    *data_ref = realloc(*data_ref, desired_cap * data_size);
    GEM_ENSURE(*data_ref != NULL);

    *cap_ref = desired_cap;
}

#define GEM_DA_MIN_CAPACITY 10

#define da_init(da, initial_cap)                                \
    {                                                           \
        (da)->capacity = (initial_cap) > GEM_DA_MIN_CAPACITY ?  \
                         (initial_cap) : GEM_DA_MIN_CAPACITY;   \
        (da)->size = 0;                                         \
        (da)->data = malloc((da)->capacity *                    \
                            sizeof(*((da)->data)));             \
        GEM_ENSURE((da)->data != NULL);                         \
    }

#define da_reserve(da, desired_cap)                                         \
    {                                                                       \
        if((da)->capacity < (desired_cap))                                  \
        {                                                                   \
            (da)->capacity = (desired_cap) + ((desired_cap) >> 1);          \
            (da)->data = realloc((da)->data,                                \
                                 (da)->capacity * sizeof(*((da)->data)));   \
            GEM_ENSURE((da)->data != NULL);                                 \
        }                                                                   \
    }

#define da_append(da, item)                 \
    {                                       \
        GEM_ASSERT((da) != NULL);           \
        da_reserve((da), (da)->size + 1);   \
        (da)->data[(da)->size++] = (item);  \
    }

#define da_append_arr(da, item_arr, item_count)         \
    {                                                   \
        GEM_ASSERT((da) != NULL);                       \
        GEM_ASSERT((item_arr) != NULL);                 \
        GEM_ASSERT((item_count) > 0);                   \
        da_reserve((da), (da)->size + (item_count));    \
        memcpy((da)->data + (da)->size, (item_arr),     \
               (item_count) * sizeof(*((da)->data)));   \
        (da)->size += (item_count);                     \
    }

#define da_insert(da, item, index)                                          \
    {                                                                       \
        GEM_ASSERT((da) != NULL);                                           \
        GEM_ASSERT((index) <= (da)->size);                                   \
        da_reserve((da), (da)->size + 1);                                   \
        for(size_t __da_idx = (index); __da_idx < (da)->size; ++__da_idx)   \
            (da)->data[__da_idx + 1] = (da)->data[__da_idx];                \
        (da)->data[(index)] = (item);                                       \
        (da)->size++;                                                       \
    }

#define da_insert_arr(da, item_arr, item_count, index)                      \
    {                                                                       \
        GEM_ASSERT((da) != NULL);                                           \
        GEM_ASSERT((item_arr) != NULL);                                     \
        GEM_ASSERT((item_count) > 0);                                       \
        GEM_ASSERT((index) <= (da)->size);                                  \
        da_reserve((da), (da)->size + (item_count));                        \
        for(size_t __da_idx = (index); __da_idx < (da)->size; ++__da_idx)   \
            (da)->data[__da_idx + (item_count)] = (da)->data[__da_idx];     \
        memcpy((da)->data + (index), (item_arr),                            \
               (item_count) * sizeof(*((da)->data)));                       \
        (da)->size += (item_count);                                         \
    }

#define da_resize(da, new_size)         \
    {                                   \
        da_reserve((da), (new_size));   \
        (da)->size = (new_size);        \
    }


#define da_free_data(da) { free((da)->data); }

typedef struct
{
    char* data;
    size_t size;
    size_t capacity;
} StringBuilder;
