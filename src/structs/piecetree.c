#include "piecetree.h"
#include "core/core.h"

#include <limits.h>
#include <string.h>

// static void pt_node_at_offset(GemPieceTree*, size_t, NodeIdx*, size_t*);
static size_t pt_alloc_node(GemPieceTree*);
static void   pt_free_node(GemPieceTree*, size_t);

// TODO: Change this to take ownership of the pointer so copying isnt necessary.
void gem_piece_tree_init(GemPieceTree* pt, const char* original_src, bool copy)
{
    GEM_ASSERT(pt != NULL);
    GEM_ASSERT(original_src != NULL);
    // Temporary
    GEM_ASSERT(original_src[0] != '\0');

    pt->original_size = strlen(original_src);
    pt->size = pt->original_size;
    if(copy)
    {
        char* original = malloc(pt->size);
        GEM_ENSURE(original != NULL);
        memcpy(original, original_src, pt->size * sizeof(char));
        pt->original = original;
    }
    else
        pt->original = original_src;

    pt->node_buffer.capacity = 10; 
    pt->node_buffer.data = malloc(sizeof(GemPTNode) * pt->node_buffer.capacity);

    // TODO: Replace the 0 initial capacities with something more reasonable
    // to start with.
    da_init(&pt->added, 0);
    da_init(&pt->free_list, pt->node_buffer.capacity);

    da_resize(&pt->free_list, pt->node_buffer.capacity);
    for(size_t i = 0, j = 9; i < pt->free_list.size; ++i, --j)
    {
        pt->node_buffer.data[i].__free = true;
        pt->free_list.data[i] = j;
    }

    size_t idx = pt_alloc_node(pt);
    size_t idx2 = pt_alloc_node(pt);
    pt_free_node(pt, idx);
    pt_free_node(pt, idx2);
    printf("Allocated node at %lu\n", idx);

    // da_resize(&pt->nodes, 1);
    // pt->nodes.data[1] = (GemPTNode){
    //     .start = 0,
    //     .length = pt->original.size,
    //     .newln_cnt = 0, // FIXME!!!!!!!!
    //     
    //     .left_sub_size = 0,
    //     .left_newln_cnt = 0,
    //
    //     .is_original = true,
    //     .is_black = true
    // };

    // pt->size += 4;
    // da_append_arr(&pt->added, "Bruh", 4);
    // pt->nodes.data[2] = PT_NULL_NODE();
    // pt->nodes.data[0] = (GemPTNode){
    //     .start = 0,
    //     .length = 4,
    //     .newln_cnt = 0,
    //
    //     .left_sub_size = pt->original.size,
    //     .left_newln_cnt = 0,
    //
    //     .is_original = false,
    //     .is_black = false
    // }; // Temp for testing 
}

void gem_piece_tree_free(GemPieceTree* pt)
{
    free((void*)pt->original);
    free(pt->node_buffer.data);

    da_free_data(&pt->added);
    da_free_data(&pt->free_list);
}


void gem_piece_tree_insert(GemPieceTree* pt, const char* str, size_t offset)
{
    GEM_ASSERT(pt != NULL);
    GEM_ASSERT(str != NULL);
    GEM_ASSERT(offset <= pt->size);
    if(str[0] == '\0')
        return;

    // Original string was of size 0, so we need to add
    // the root node.
    // if(pt->nodes.size == 0) 
    // {
    // }

    // NodeIdx node_idx;
    // GemPTNode* node;
    // size_t node_start_offset;
    // size_t node_end;
    // size_t len = strlen(str);
    //
    // pt_node_at_offset(pt, offset, &node_idx, &node_start_offset);
    // node = pt_get_ref(pt, node_idx);
    // node_end = node->start + node->length;
    //
    // printf("NodeIdx: %lu, start: %lu\n", node_idx, node_start_offset);
    //
    // if(!node->is_original && node_end == pt->added.size && node_start_offset + node->length == offset)
    // {
    //     printf("Appending\n");
    //     node->length += len;
    //     da_append_arr(&pt->added, str, len);
    // }
    // else if(node_start_offset == offset)
    // {
    //     printf("Adding to the left of a node\n");
    //
    // }
}

// static void pt_print_node(GemPieceTree* pt, NodeIdx node_idx)
// {
//     GemPTNode* node = pt_get_ref(pt, node_idx);
//     if(!node_valid(node))
//         return;
//
//     pt_print_node(pt, ptn_left(pt, node_idx, NULL));
//     size_t end = node->start + node->length;
//     for(size_t j = node->start; j < end; ++j)
//         putc(node->is_original ? pt->original.data[j] : pt->added.data[j], stdout);
//     pt_print_node(pt, ptn_right(pt, node_idx, NULL));
// }
//
// void gem_piece_tree_print(GemPieceTree* pt)
// {
//     GEM_ASSERT(pt != NULL);
//     
//     printf("Original Contents (size %lu):\n", pt->original.size);
//     for(size_t i = 0; i < pt->original.size; ++i)
//         putc(pt->original.data[i], stdout);
//
//     printf("\n\nAdded Contents (size %lu):\n", pt->added.size);
//     for(size_t i = 0; i < pt->added.size; ++i)
//         putc(pt->added.data[i], stdout);
//
//     printf("\n\nTrue Contents (size %lu):\n", pt->size);
//     pt_print_node(pt, PT_ROOT_IDX);
//     printf("\n");
// }

void gem_piece_tree_freelist(GemPieceTree* pt)
{
    GEM_ASSERT(pt != NULL);
    printf("Free List Size: %lu\nFree List Contents:\n\t", pt->free_list.size);
    for(size_t i = 0; i < pt->free_list.size; ++i)
    {
        printf("%lu ", pt->free_list.data[pt->free_list.size - i - 1]);
        if(i % 8 == 7)
            printf("\n\t");
    }
    printf("\n");
}

static size_t UNUSED pt_alloc_node(GemPieceTree* pt)
{
    size_t node_idx;
    if(pt->free_list.size > 0)
    {
        node_idx = pt->free_list.data[pt->free_list.size - 1];
        GEM_ASSERT(pt->node_buffer.data[node_idx].__free);
        pt->free_list.size--;
    }
    else
    {
        node_idx = pt->node_buffer.capacity;
        size_t new_cap = pt->node_buffer.capacity;
        new_cap += new_cap >> 1;
        pt->node_buffer.data = realloc(pt->node_buffer.data, sizeof(GemPTNode) * new_cap);
        GEM_ENSURE(pt->node_buffer.data != NULL);
        pt->node_buffer.capacity = new_cap;

        da_resize(&pt->free_list, new_cap - node_idx - 1);
        for(size_t i = 0, j = new_cap - 1; i < pt->free_list.size; ++i, --j)
            pt->free_list.data[i] = j;
    }

    GemPTNode* node = pt->node_buffer.data + node_idx;
    node->__free = false;
    return node_idx;
}

static void UNUSED pt_free_node(GemPieceTree* pt, size_t node_idx)
{
    GEM_ASSERT(node_idx < pt->node_buffer.capacity);

    GemPTNode* node = pt->node_buffer.data + node_idx;
    GEM_ASSERT(!node->__free);

    node->__free = true;
    da_append(&pt->free_list, node_idx);
}

// static void pt_node_at_offset(GemPieceTree* pt, size_t offset, NodeIdx* node_idx, size_t* node_start_offset)
// {
//     GEM_ASSERT(offset <= pt->size);
//     NodeIdx idx = PT_ROOT_IDX;
//     GemPTNode* node = pt_root(pt);
//     size_t startoff = 0;
//
//     while(node_valid(node))
//     {
//         if(node->left_sub_size > offset) // Offset is in left subtree
//             idx = ptn_left(pt, idx, &node);
//         else if(node->left_sub_size + node->length >= offset) // Offset is inside this node
//         {
//             *node_idx = idx;
//             *node_start_offset = startoff + node->left_sub_size;
//             return;
//         }
//         else
//         {
//             offset -= node->left_sub_size + node->length;
//             startoff += node->left_sub_size + node->length;
//             idx = ptn_right(pt, idx, &node);
//         }
//     }
//
//     GEM_ERROR("This should not have been reached.");
// }
