#pragma once
#include "da.h"
#include "core/core.h"
#include "render/renderer.h"

#include <stdbool.h>

typedef struct PTInternNode PTInternNode;
typedef struct PTNode PTNode;

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

void piece_tree_print_contents(PieceTree* pt);
void piece_tree_print_tree    (PieceTree* pt);
void piece_tree_render        (PieceTree* pt, const GemQuad* bounding_box);
