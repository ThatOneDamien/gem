#pragma once
#include "da.h"
#include "core/core.h"

#include <stdbool.h>

typedef struct PTInternNode PTInternNode;
typedef struct PTNode PTNode;

struct alignas(1) PTNode
{
    size_t  start;
    size_t  length;
    size_t  newln_cnt;

    size_t  left_sub_size;
    size_t  left_newln_cnt;

    PTNode* left;
    PTNode* right;
    PTNode* parent;

    bool    is_original;
    bool    is_black;
};

typedef struct
{
    size_t line;
    size_t column;
} BufferPos;

typedef struct
{
    PTInternNode* data;
    size_t        capacity;
} PTNodeStorage;

typedef struct
{
    size_t*  data;
    size_t   capacity;
    size_t   size;
} PTPosDA;

typedef struct
{
    const char*   data;          /* Original buffer */
    size_t        size;          /* Original buffer size */
    const size_t* newlines;      /* Locations of all newlines in original buffer */
    size_t        newlines_size; /* Size of newlines buffer */
} PTOrigBuffer;

typedef struct
{
    StringBuilder added;          /* Added buffer */
    PTPosDA       added_newlines; /* Locations of all newlines in added buffer */
    PTOrigBuffer  original;  /* Original buffer */

    size_t        size;           /* Effective character count of tree */
    size_t        newline_count;
    PTNode*       root;          

    PTNodeStorage storage;
    PTPosDA       free_list;
} PieceTree;

void piece_tree_init  (PieceTree* pt, const char* original_src, bool copy);
void piece_tree_free  (PieceTree* pt);
void piece_tree_insert(PieceTree* pt, const char* str, size_t offset);

const PTNode* piece_tree_node_at_line(const PieceTree* pt, size_t line, size_t* node_offset);
const PTNode* piece_tree_next_inorder(const PieceTree* pt, const PTNode* node);
const char* piece_tree_get_node_start(const PieceTree* pt, const PTNode* node);

void piece_tree_print_contents(PieceTree* pt);
void piece_tree_print_tree    (PieceTree* pt);
