#include "piecetree.h"
#include "core/core.h"

#include <limits.h>
#include <string.h>


struct alignas(8) PTInternNode
{
    PTNode node;
    bool   free;
};


static PTNode* node_at_offset(PieceTree*, size_t, size_t*);
static PTNode* alloc_node(PieceTree*);
static void    free_node(PieceTree*, PTNode*);
static void    expand_node_storage(PieceTree*, size_t);
static size_t  get_newln_count(const char*, size_t);
static void    bubble_meta_changes(PieceTree*, PTNode*, int64_t, int64_t);

static PTNode* left_test(const PieceTree*, const PTNode*);
static PTNode* right_test(const PieceTree*, const PTNode*);
static void    fix_insert(PieceTree*, PTNode*);
static void    left_rotate(PieceTree*, PTNode*);
static void    right_rotate(PieceTree*, PTNode*);

static inline void mark_free(PieceTree*, size_t);
static inline bool in_valid_range(const PieceTree*, const PTNode*);

static PTNode s_Sentinel = (PTNode){
    .start          = 0,
    .length         = 0, 
    .newln_cnt      = 0,

    .left_sub_size  = 0,
    .left_newln_cnt = 0,

    .left           = NULL,
    .right          = NULL,
    .parent         = NULL,

    .is_original    = false,
    .is_black       = true,
};
static PTNode* const SENTINEL = &s_Sentinel;

// TODO: Change this to take ownership of the pointer so copying isnt necessary.
void piece_tree_init(PieceTree* pt, const char* original_src, bool copy)
{
    GEM_ASSERT(pt != NULL);
    memset(pt, 0, sizeof(PieceTree));

    pt->storage.capacity = 10; 
    pt->storage.data = malloc(pt->storage.capacity * sizeof(PTInternNode));

    // TODO: Replace the 0 initial capacities with something more reasonable
    // to start with.
    da_init(&pt->added, 0);
    da_init(&pt->added_newlines, 0);
    da_init(&pt->free_list, pt->storage.capacity);
    pt->size = original_src == NULL ? 0 : strlen(original_src);
    pt->original.size = pt->size;

    if(pt->size == 0)
    {
        mark_free(pt, 0);
        return;
    }

    if(copy)
    {
        pt->original.data = malloc(pt->size);
        GEM_ENSURE(pt->original.data != NULL);
        memcpy((void*)pt->original.data, original_src, pt->size * sizeof(char));
    }
    else
        pt->original.data = original_src;


    mark_free(pt, 1);
    pt->original.newlines_size = get_newln_count(pt->original.data, pt->size);
    pt->original.newlines = malloc(sizeof(size_t) * pt->original.newlines_size);
    pt->newline_count = pt->original.newlines_size;
    GEM_ENSURE(pt->original.newlines != NULL);
    size_t* nls = (size_t*)pt->original.newlines;
    for(size_t i = 0; i < pt->size; ++i)
        if(pt->original.data[i] == '\n')
        {
            *nls = i;
            nls++;
        }

    pt->root = &pt->storage.data[0].node;
    pt->storage.data[0] = (PTInternNode){
        .node = {
            .start          = 0,
            .length         = pt->size,
            .newln_cnt      = pt->newline_count,

            .left_sub_size  = 0,
            .left_newln_cnt = 0,

            .left           = SENTINEL,
            .right          = SENTINEL,
            .parent         = SENTINEL,

            .is_original    = true,
            .is_black       = true,
        },
        .free         = false
    };
}

void piece_tree_free(PieceTree* pt)
{
    free(pt->storage.data);
    free((void*)pt->original.data);
    free((void*)pt->original.newlines);

    da_free_data(&pt->added);
    da_free_data(&pt->added_newlines);
    da_free_data(&pt->free_list);
}


void piece_tree_insert(PieceTree* pt, const char* str, size_t offset)
{
    GEM_ASSERT(pt != NULL);
    GEM_ASSERT(str != NULL);
    GEM_ASSERT(offset <= pt->size);
    if(str[0] == '\0')
        return;

    if(pt->free_list.size < 2)
        expand_node_storage(pt, pt->storage.capacity + (pt->storage.capacity >> 1) + 2);

    PTNode* node;
    size_t node_start_offset;
    size_t len = strlen(str);
    size_t newline_count = 0;
    for(size_t i = 0; i < len; ++i)
        if(str[i] == '\n')
        {
            da_append(&pt->added_newlines, i);
            newline_count++;
        }

    PTNode new_node = (PTNode){
            .start          = pt->added.size,
            .length         = len,
            .newln_cnt      = newline_count,

            .left_sub_size  = 0,
            .left_newln_cnt = 0,

            .left           = SENTINEL,
            .right          = SENTINEL,
            .parent         = SENTINEL,

            .is_original    = false,
            .is_black       = false
    };

    da_append_arr(&pt->added, str, len);
    pt->size += len;
    pt->newline_count += newline_count;

    if(pt->root == NULL)
    {
        pt->root = alloc_node(pt);
        new_node.is_black = true;
        *pt->root = new_node;
        return;
    }

    node = node_at_offset(pt, offset, &node_start_offset);

    if(node_start_offset == offset) // Insert to the left of node
    {
        PTNode* new = alloc_node(pt);
        *new = new_node;
        if(node->left == SENTINEL)
        {
            new->parent = node;
            node->left = new;
        }
        else
        {
            PTNode* rightmost = right_test(pt, node->left);
            new->parent = rightmost;
            rightmost->right = new;
        }
        bubble_meta_changes(pt, node->left, new->length, new->newln_cnt);
        fix_insert(pt, new);
        // TODO: Fix metadata
    }
    else if(node_start_offset + node->length > offset) // Split node
    {
        size_t node_new_len = offset - node_start_offset;
        PTNode* new = alloc_node(pt);
        PTNode* split = alloc_node(pt);
        *new = new_node;
        *split = (PTNode){
            .start          = node->start + node_new_len,
            .length         = node->length - node_new_len,

            .left           = SENTINEL,
            .right          = SENTINEL,
            .parent         = SENTINEL,

            .is_original    = node->is_original
        };
        split->newln_cnt = get_newln_count(pt->added.data + split->start, split->length);
        node->length = node_new_len; 
        node->newln_cnt -= split->newln_cnt;

        if(node->right == SENTINEL) 
        {
            node->right = split;
            split->parent = node;
            bubble_meta_changes(pt, node, split->length, split->newln_cnt);
        }
        else
        {
            PTNode* leftmost = left_test(pt, node->right);
            leftmost->left = split;
            split->parent = leftmost;
            bubble_meta_changes(pt, split, split->length, split->newln_cnt);
        }
        fix_insert(pt, split);

        PTNode* leftmost = left_test(pt, node->right);
        leftmost->left = new;
        new->parent = leftmost;
        bubble_meta_changes(pt, new, new->length, new->newln_cnt);

        fix_insert(pt, new);
    }
    else if(!node->is_original && node->start + node->length == pt->added.size - len) // Append to node
    {
        node->length += len;
        node->newln_cnt += new_node.newln_cnt;
        bubble_meta_changes(pt, node, len, new_node.newln_cnt);
    }
    else // Insert to the right of node
    {
        PTNode* new = alloc_node(pt);
        *new = new_node;
        if(node->right == SENTINEL) 
        {
            node->right = new;
            new->parent = node;
            bubble_meta_changes(pt, node, new->length, new->newln_cnt);
        }
        else
        {
            PTNode* leftmost = left_test(pt, node->right);
            leftmost->left = new;
            new->parent = leftmost;
            bubble_meta_changes(pt, new, new->length, new->newln_cnt);
        }
        fix_insert(pt, new);
    }

}

const PTNode* piece_tree_node_at_line(const PieceTree* pt, size_t line, size_t* node_offset)
{
    GEM_ASSERT(pt != NULL);
    GEM_ASSERT(node_offset != NULL);
    GEM_ASSERT(line <= pt->newline_count);
    if(line == 0)
    {
        *node_offset = 0;
        return left_test(pt, pt->root);
    }

    PTNode* node = pt->root;
    while(node != SENTINEL)
    {
        if(line < node->left_newln_cnt)
            node = node->left;
        else if(line <= node->left_newln_cnt + node->newln_cnt)
        {
            const char* buf = piece_tree_get_node_start(pt, node);
            if(line == node->left_newln_cnt + node->newln_cnt &&
               buf[node->length - 1] == '\n')
            {
                *node_offset = 0;
                return piece_tree_next_inorder(pt, node);
            }
            line -= node->left_newln_cnt;
            size_t offset;
            for(offset = 0; line > 0 && offset < node->length; ++offset)
                if(buf[offset] == '\n')
                    --line;
            *node_offset = offset;
            return node;
        }
        else
        {
            line -= node->left_newln_cnt + node->newln_cnt;
            node = node->right;
        }
    }

    GEM_ERROR("Shouldn't be here.");
    return NULL;
}

const PTNode* piece_tree_next_inorder(const PieceTree* pt, const PTNode* node)
{
    GEM_ASSERT(pt != NULL);
    GEM_ASSERT(node != SENTINEL);
    GEM_ASSERT(in_valid_range(pt, node));

    if(node->right != SENTINEL)
        return left_test(pt, node->right);
    
    while(node != pt->root)
    {
        if(node == node->parent->left)
            return node->parent;
        node = node->parent;
    }

    return NULL;
}

const char* piece_tree_get_node_start(const PieceTree* pt, const PTNode* node)
{
    GEM_ASSERT(pt != NULL);
    GEM_ASSERT(node != SENTINEL);
    GEM_ASSERT(in_valid_range(pt, node));
    return (node->is_original ? pt->original.data : pt->added.data) + node->start;
}

static void print_node_contents(PieceTree* pt, PTNode* node)
{
    if(!in_valid_range(pt, node) || ((PTInternNode*)node)->free)
        return;

    print_node_contents(pt, node->left);
    const char* buf = piece_tree_get_node_start(pt, node);
    for(size_t i = 0; i < node->length; ++i)
        putc(buf[i], stdout);
    print_node_contents(pt, node->right);
}

void piece_tree_print_contents(PieceTree* pt)
{
    GEM_ASSERT(pt != NULL);
    
    // printf("Original Contents (size %lu):\n", pt->original_size);
    // for(size_t i = 0; i < pt->original_size; ++i)
    //     putc(pt->original[i], stdout);
    //
    // printf("\n\nAdded Contents (size %lu):\n", pt->added.size);
    // for(size_t i = 0; i < pt->added.size; ++i)
    //     putc(pt->added.data[i], stdout);

    // printf("\n\nTrue Contents (size %lu):\n", pt->size);
    print_node_contents(pt, pt->root);
    printf("\n");
}

static void print_node_metadata(PieceTree* pt, PTNode* node, size_t depth)
{
    if(!in_valid_range(pt, node) || ((PTInternNode*)node)->free)
        return;

    print_node_metadata(pt, node->right, depth + 1);
    for(size_t i = 0; i < depth; ++i)
        printf("  ");
    printf("(%s %s Start:%lu Length:%lu NewLnCnt:%lu LeftSubSize:%lu LeftNewLn:%lu)\n", 
           node->is_black ? "Black" : "Red",
           node->is_original ? "Original" : "Added",
           node->start,
           node->length,
           node->newln_cnt,
           node->left_sub_size,
           node->left_newln_cnt);
    print_node_metadata(pt, node->left, depth + 1);
}

void piece_tree_print_tree(PieceTree* pt)
{
    GEM_ASSERT(pt != NULL);
    print_node_metadata(pt, pt->root, 0);
}

static PTNode* node_at_offset(PieceTree* pt, size_t offset, size_t* node_start_offset)
{
    GEM_ASSERT(offset <= pt->size);
    PTNode* node = pt->root;
    size_t startoff = 0;

    while(node != SENTINEL)
    {
        if(node->left_sub_size > offset) // Offset is in left subtree
            node = node->left;
        else if(node->left_sub_size + node->length >= offset) // Offset is inside this node
        {
            *node_start_offset = startoff + node->left_sub_size;
            return node;
        }
        else // Offset is in right subtree
        {
            offset -= node->left_sub_size + node->length;
            startoff += node->left_sub_size + node->length;
            node = node->right;
        }
    }

    GEM_ERROR("This should not have been reached.");
    return NULL;
}

static PTNode* alloc_node(PieceTree* pt)
{
    GEM_ASSERT(pt->free_list.size > 0);
    size_t node_idx = pt->free_list.data[pt->free_list.size - 1];
    GEM_ASSERT(pt->storage.data[node_idx].free);       
    
    pt->free_list.size--;
    PTInternNode* node = pt->storage.data + node_idx;
    node->free = false;
    return &node->node;
}

static UNUSED void free_node(PieceTree* pt, PTNode* node)
{
    if(node == SENTINEL || node == NULL)
        return;
    GEM_ASSERT(in_valid_range(pt, node));

    PTInternNode* internal = (PTInternNode*)node;
    GEM_ASSERT(!internal->free);
    size_t index = ((uintptr_t)internal - (uintptr_t)pt->storage.data) / sizeof(PTInternNode);

    internal->free = true;
    da_append(&pt->free_list, index);
}

static void fix_pointer(PTNode** invalid, uintptr_t delta)
{
    if(*invalid != SENTINEL)
        *invalid = (PTNode*)((uintptr_t)(*invalid) + delta);
}

static void expand_node_storage(PieceTree* pt, size_t capacity)
{
    if(pt->storage.capacity >= capacity)
        return;

    size_t prev_cap = pt->storage.capacity;
    PTInternNode* prev_pointer = pt->storage.data;
    pt->storage.data = realloc(pt->storage.data, sizeof(PTInternNode) * capacity);
    GEM_ENSURE(pt->storage.data != NULL);
    if(prev_pointer != pt->storage.data)
    {
        uintptr_t delta = (uintptr_t)pt->storage.data - (uintptr_t)prev_pointer;
        fix_pointer(&pt->root, delta);
        for(size_t i = 0; i < prev_cap; ++i)
        {
            PTInternNode* intern = pt->storage.data + prev_cap;
            if(!intern->free)
            {
                fix_pointer(&intern->node.left, delta);
                fix_pointer(&intern->node.right, delta);
                fix_pointer(&intern->node.parent, delta);
            }
        }
    }
    pt->storage.capacity = capacity;
    mark_free(pt, prev_cap);
}

static UNUSED size_t get_newln_count(const char* text, size_t size)
{
    size_t cnt = 0;
    for(size_t i = 0; i < size; ++i)
        if(text[i] == '\n')
            ++cnt;
    return cnt;
}

static void bubble_meta_changes(PieceTree* pt, PTNode* node, int64_t size_delta, int64_t newln_delta)
{
    GEM_ASSERT(node != SENTINEL);
    while(node != pt->root)
    {
        if(node == node->parent->left)
        {
            node->parent->left_sub_size += size_delta;
            node->parent->left_newln_cnt += newln_delta;
        }
        node = node->parent;
    }
}

static PTNode* left_test(const PieceTree* pt, const PTNode* node)
{
    GEM_ASSERT(in_valid_range(pt, node));
    while(node->left != SENTINEL)
        node = node->left;
    return (PTNode*)node;
}

static PTNode* right_test(const PieceTree* pt, const PTNode* node)
{
    GEM_ASSERT(in_valid_range(pt, node));
    while(node->right != SENTINEL)
        node = node->right;
    return (PTNode*)node;
}

static void fix_insert(PieceTree* pt, PTNode* node)
{
    GEM_ASSERT(in_valid_range(pt, node));
    while(node->parent != SENTINEL && !node->parent->is_black)
    {
        PTNode* aunt;
        PTNode* zigzag_check;
        void (*first_func)(PieceTree*, PTNode*);
        void (*second_func)(PieceTree*, PTNode*);
        if(node->parent == node->parent->parent->left)
        {
            aunt = node->parent->parent->right;
            zigzag_check = node->parent->right;
            first_func = left_rotate;
            second_func = right_rotate;
        }
        else
        {
            aunt = node->parent->parent->left;
            zigzag_check = node->parent->left;
            first_func = right_rotate;
            second_func = left_rotate;
        }

        if(aunt->is_black)
        {
            if(node == zigzag_check)
            {
                node = node->parent;
                first_func(pt, node);
            }
            node->parent->is_black = true;
            node->parent->parent->is_black = false;
            second_func(pt, node->parent->parent);
        }
        else
        {
            node->parent->is_black = true;
            aunt->is_black = true;
            node->parent->parent->is_black = false;
            node = node->parent->parent;
        }
    }
    pt->root->is_black = true;
}

static void left_rotate(PieceTree* pt, PTNode* node)
{
    PTNode* right = node->right;
    right->left_sub_size += node->left_sub_size + node->length;
    right->left_newln_cnt += node->left_newln_cnt + node->newln_cnt;
    node->right = right->left;

    if(node->right != SENTINEL)
        node->right->parent = node;
    right->parent = node->parent;
    if(pt->root == node)
        pt->root = right;
    else if(node->parent->left == node)
        node->parent->left = right;
    else
        node->parent->right = right;

    right->left = node;
    node->parent = right;
}

static void right_rotate(PieceTree* pt, PTNode* node)
{
    PTNode* left = node->left;
    node->left = left->right;
    if(left->right != SENTINEL)
        left->right->parent = node;
    left->parent = node->parent;

    node->left_sub_size -= left->left_sub_size + left->length;
    node->left_newln_cnt -= left->left_newln_cnt + left->newln_cnt;

    if(pt->root == node)
        pt->root = left;
    else if(node->parent->right == node)
        node->parent->right = left;
    else
        node->parent->left = left;

    left->right = node;
    node->parent = left;
}

static inline void mark_free(PieceTree* pt, size_t start)
{
    size_t prev_size = pt->free_list.size;
    da_resize(&pt->free_list, prev_size + pt->storage.capacity - start);
    for(size_t i = prev_size, j = start; i < pt->free_list.size; ++i, ++j)
    {
        pt->free_list.data[i] = pt->storage.capacity - j;
        pt->storage.data[j].free = true;
    }
}

static inline bool in_valid_range(const PieceTree* pt, const PTNode* node)
{
    const PTInternNode* n = (const PTInternNode*)node;
    return n >= pt->storage.data && n < (pt->storage.data + pt->storage.capacity);
}
