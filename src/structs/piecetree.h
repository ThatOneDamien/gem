#pragma once
#include "da.h"
#include "core/core.h"

#include <stdbool.h>

typedef struct PTInternNode PTInternNode;
typedef struct PTNode PTNode;

typedef struct
{
    int64_t line;
    int64_t column;
} BufferPos;

struct alignas(1) PTNode
{
    BufferPos  start;
    BufferPos  end;
    size_t     length;
    size_t     nl_cnt;

    size_t     left_size;
    size_t     left_nl_cnt;

    PTNode*    left;
    PTNode*    right;
    PTNode*    parent;

    bool       is_original;
    bool       is_black;

#ifdef GEM_PT_VALIDATE
    bool used;
#endif
};


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
    const char*   data;        /* Original buffer */
    size_t        size;        /* Original buffer size */
    const size_t* line_starts; /* Locations of all line starts in original buffer */
    size_t        line_cnt;    /* Number of lines in the buffer (will always be > 0) */
} PTOrigBuffer;

typedef struct
{
    char*   data;
    size_t  size;
    size_t  capacity;
    PTPosDA line_starts;
} PTAddedBuffer;

typedef struct
{
    PTAddedBuffer added;         /* Added buffer */
    PTOrigBuffer  original;      /* Original buffer */

    size_t        size;          /* Effective character count of tree */
    size_t        line_cnt;
    PTNode*       root;          

    PTNodeStorage storage;
    PTPosDA       free_list;
} PieceTree;

void piece_tree_init  (PieceTree* pt, const char* original_src, size_t size, bool copy);
void piece_tree_free  (PieceTree* pt);
void piece_tree_insert(PieceTree* pt, const char* str, size_t offset);
void piece_tree_delete(PieceTree* pt, size_t offset, size_t count);

const PTNode* piece_tree_node_at_line(const PieceTree* pt, size_t line, size_t* node_offset);
const PTNode* piece_tree_next_inorder(const PieceTree* pt, const PTNode* node);
const char*   piece_tree_get_node_start(const PieceTree* pt, const PTNode* node);
size_t        piece_tree_get_line_length(const PieceTree* pt, size_t line_num);
size_t        piece_tree_get_offset(const PieceTree* pt, size_t line, size_t column);
BufferPos     piece_tree_get_buffer_pos(const PieceTree* pt, size_t offset);

void piece_tree_print_contents(const PieceTree* pt);
void piece_tree_print_tree    (const PieceTree* pt);

static inline size_t piece_tree_get_offset_bp(const PieceTree* pt, BufferPos pos)
{ 
    return piece_tree_get_offset(pt, pos.line, pos.column);
}
