#pragma once
#include "da.h"

#include <stdbool.h>

typedef struct
{
    size_t start;
    size_t length;
    size_t newln_cnt;

    size_t left_sub_size;
    size_t left_newln_cnt;

    size_t left;
    size_t right;
    size_t parent;

    bool   is_original;
    bool   is_black;
    bool   __free; // Used for internal storage
} GemPTNode;

typedef struct
{
    GemPTNode* data;
    size_t     capacity;
} GemPTNodeDA;

typedef struct
{
    size_t* data;
    size_t  size;
    size_t  capacity;
} GemFreeListDA;

typedef struct
{
    const char*      original;
    size_t           original_size;
    GemStringBuilder added;
    size_t           size;

    size_t           root;
    GemPTNodeDA      node_buffer;
    GemFreeListDA    free_list;
} GemPieceTree;

// #define PT_INVALID UINT64_MAX
// #define PT_NULL_NODE() ((GemPTNode) {                       \
//                             .start = PT_INVALID,            \
//                             .length = PT_INVALID,           \
//                             .newln_cnt = PT_INVALID,        \
//                             .left_sub_size = PT_INVALID,    \
//                             .left_newln_cnt = PT_INVALID,   \
//                             .is_original = false,           \
//                             .is_black = false               \
//                         })

void gem_piece_tree_init  (GemPieceTree* pt, const char* original_src, bool copy);
void gem_piece_tree_free  (GemPieceTree* pt);
void gem_piece_tree_insert(GemPieceTree* pt, const char* str, size_t offset);
void gem_piece_tree_print (GemPieceTree* pt);


void gem_piece_tree_freelist(GemPieceTree* pt);
