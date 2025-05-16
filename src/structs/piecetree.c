#include "piecetree.h"
#include "core/core.h"

#include <limits.h>
#include <string.h>

#ifdef GEM_PT_VALIDATE
static void validate_tree(const PieceTree* pt);
    #define PT_VALIDATE(pt) { validate_tree(pt); }
#else
    #define PT_VALIDATE(pt)
#endif


struct alignas(8) PTInternNode
{
    PTNode node;
    bool   free;
};


static PTNode* node_at_offset(PieceTree* pt, size_t offset, size_t* node_start_offset, bool tail);
static PTNode* alloc_node(PieceTree* pt);
static void    free_node(PieceTree* pt, PTNode* node);
static void    expand_node_storage(PieceTree* pt, size_t capacity);
static size_t  get_newln_count(const char* str, size_t len);
static void    bubble_meta_changes(PieceTree* pt, PTNode* node, int64_t size_delta, int64_t newln_delta);
static PTNode  create_node_and_append(PieceTree* pt, const char* str, size_t len);

static PTNode*   split_node(PieceTree* pt, PTNode* node, size_t left_size, size_t right_size);
static BufferPos position_in_buffer(const PieceTree* pt, PTNode* node, size_t offset);
static PTNode*   left_test(const PTNode* node);
static PTNode*   right_test(const PTNode* node);
static void      fix_insert(PieceTree* pt, PTNode* node);
static void      left_rotate(PieceTree* pt, PTNode* node);
static void      right_rotate(PieceTree* pt, PTNode* node);
static void      delete_node(PieceTree* pt, PTNode* node);

static inline void   mark_free(PieceTree* pt, size_t start);
static inline bool   in_valid_range(const PieceTree* pt, const PTNode* node);
static inline size_t node_id(const PieceTree* pt, const PTNode* node);
static inline PTNode node_default(void);

static PTNode s_Sentinel = (PTNode){
    .left        = &s_Sentinel,
    .right       = &s_Sentinel,
    .parent      = &s_Sentinel,
    .is_black    = true,
};
static PTNode* const SENTINEL = &s_Sentinel;

// TODO: Change this to take ownership of the pointer so copying isnt necessary.
void piece_tree_init(PieceTree* pt, const char* original_src, size_t size, bool copy)
{
    GEM_ASSERT(pt != NULL);
    memset(pt, 0, sizeof(PieceTree));

    pt->storage.capacity = 10; 
    pt->storage.data = malloc(pt->storage.capacity * sizeof(PTInternNode));

    // TODO: Replace the 0 initial capacities with something more reasonable
    // to start with.
    da_init(&pt->free_list, pt->storage.capacity);
    da_init(&pt->added, 0);
    da_init(&pt->added.line_starts, 0);
    da_append(&pt->added.line_starts, 0);
    pt->size = size;
    pt->original.size = size;
    pt->line_cnt = 1;

    if(pt->size == 0)
    {
        pt->root = SENTINEL;
        mark_free(pt, 0);
        return;
    }

    if(copy)
    {
        pt->original.data = malloc(size);
        GEM_ENSURE(pt->original.data != NULL);
        memcpy((void*)pt->original.data, original_src, size * sizeof(char));
    }
    else
        pt->original.data = original_src;


    mark_free(pt, 1);
    pt->line_cnt = get_newln_count(pt->original.data, size) + 1;
    pt->original.line_cnt = pt->line_cnt;
    pt->original.line_starts = malloc(sizeof(size_t) * pt->original.line_cnt);
    GEM_ENSURE(pt->original.line_starts != NULL);
    size_t* ls = (size_t*)pt->original.line_starts;
    *ls = 0;
    ls++;
    for(size_t i = 0; i < size; ++i)
        if(pt->original.data[i] == '\n')
        {
            *ls = i + 1;
            ls++;
        }

    pt->root = &pt->storage.data[0].node;
    *(pt->root) = node_default();

    pt->root->end.line     = pt->line_cnt - 1;
    pt->root->end.column   = size - pt->original.line_starts[pt->line_cnt - 1];
    pt->root->length       = size;
    pt->root->is_original  = true;
    pt->root->is_black     = true;
    pt->root->nl_cnt       = pt->line_cnt - 1;

    pt->storage.data[0].free = false;
    PT_VALIDATE(pt);
}

void piece_tree_free(PieceTree* pt)
{
    free(pt->storage.data);
    free((void*)pt->original.data);
    free((void*)pt->original.line_starts);

    da_free_data(&pt->added);
    da_free_data(&pt->added.line_starts);
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
    PTNode new_node = create_node_and_append(pt, str, len);

    if(pt->root == SENTINEL)
    {
        pt->root = alloc_node(pt);
        new_node.is_black = true;
        *pt->root = new_node;
        PT_VALIDATE(pt);
        return;
    }

    // The only case where we insert to the left of a node is
    // when we are inserting at the very beginning of the document.
    // This ensures we append as much as possible.
    if(offset == 0) 
    {
        node = left_test(pt->root);
        PTNode* new = alloc_node(pt);
        *new = new_node;
        new->parent = node;
        node->left = new;
        bubble_meta_changes(pt, new, new->length, new->nl_cnt);
        fix_insert(pt, new);
        PT_VALIDATE(pt);
        return;
    }

    node = node_at_offset(pt, offset, &node_start_offset, true);

    GEM_ASSERT(offset > node_start_offset);
    if(node_start_offset + node->length > offset) // Split node
    {
        size_t node_new_len = offset - node_start_offset;
        PTNode* new = alloc_node(pt);
        PTNode* split = split_node(pt, node, node_new_len, node->length - node_new_len);
        *new = new_node;

        if(node->right == SENTINEL) 
        {
            node->right = split;
            split->parent = node;
        }
        else
        {
            PTNode* leftmost = node->right;
            while(leftmost->left != SENTINEL)
            {
                leftmost->left_size += split->length;
                leftmost->left_nl_cnt += split->nl_cnt;
                leftmost = leftmost->left;
            }
            leftmost->left = split;
            leftmost->left_size = split->length;
            leftmost->left_nl_cnt = split->nl_cnt;
            split->parent = leftmost;
        }
        fix_insert(pt, split);

        if(node->right == SENTINEL) 
        {
            node->right = new;
            new->parent = node;
            bubble_meta_changes(pt, node, new->length, new->nl_cnt);
        }
        else
        {
            PTNode* leftmost = left_test(node->right);
            leftmost->left = new;
            new->parent = leftmost;
            bubble_meta_changes(pt, new, new->length, new->nl_cnt);
        }
        fix_insert(pt, new);
    }
    else if(!node->is_original && node->end.line == new_node.start.line && 
            node->end.column == new_node.start.column) // Append to node
    {
        node->end = new_node.end;
        node->length += len;
        node->nl_cnt += new_node.nl_cnt;
        bubble_meta_changes(pt, node, len, new_node.nl_cnt);
    }
    else // Insert to the right of node
    {
        PTNode* new = alloc_node(pt);
        *new = new_node;
        if(node->right == SENTINEL) 
        {
            node->right = new;
            new->parent = node;
            bubble_meta_changes(pt, node, new->length, new->nl_cnt);
        }
        else
        {
            PTNode* leftmost = left_test(node->right);
            leftmost->left = new;
            new->parent = leftmost;
            bubble_meta_changes(pt, new, new->length, new->nl_cnt);
        }
        fix_insert(pt, new);
    }
    PT_VALIDATE(pt);
}

void piece_tree_delete(PieceTree* pt, size_t offset, size_t count)
{
    GEM_ASSERT(pt != NULL);

    if(count == 0 || pt->root == SENTINEL)
        return;

    GEM_ASSERT(offset < pt->size);
    GEM_ASSERT(count <= pt->size - offset);

    if(pt->free_list.size < 2)
        expand_node_storage(pt, pt->storage.capacity + (pt->storage.capacity >> 1) + 2);

    size_t start_offset;
    size_t end_offset;
    PTNode* start = node_at_offset(pt, offset, &start_offset, false);
    PTNode* end = node_at_offset(pt, offset + count, &end_offset, true);

    if(start == end)
    {
        if(start_offset == offset)
        {
            if(count == start->length)
            {
                pt->size -= count;
                pt->line_cnt -= start->nl_cnt;
                delete_node(pt, start);
                PT_VALIDATE(pt);
                return;
            }
            start->start = position_in_buffer(pt, start, offset + count - start_offset);
            size_t prev_nl_cnt = start->nl_cnt;
            start->nl_cnt = start->end.line - start->start.line;
            start->length -= count;
            bubble_meta_changes(pt, start, -count, start->nl_cnt - prev_nl_cnt);
            pt->size -= count;
            pt->line_cnt -= prev_nl_cnt - start->nl_cnt;
            PT_VALIDATE(pt);
            return;
        }

        if(start_offset + start->length == offset + count)
        {
            start->end = position_in_buffer(pt, start, offset - start_offset);
            size_t prev_nl_cnt = start->nl_cnt;
            start->nl_cnt = start->end.line - start->start.line;
            start->length -= count;
            bubble_meta_changes(pt, start, -count, start->nl_cnt - prev_nl_cnt);
            pt->size -= count;
            pt->line_cnt -= prev_nl_cnt - start->nl_cnt;
            PT_VALIDATE(pt);
            return;
        }

        size_t prev_nl_cnt = start->nl_cnt;
        PTNode* split = split_node(pt, start, offset - start_offset, 
                                   start_offset + start->length - offset - count);
        if(start->right == SENTINEL) 
        {
            start->right = split;
            split->parent = start;
        }
        else
        {
            PTNode* leftmost = start->right;
            while(leftmost->left != SENTINEL)
            {
                leftmost->left_size += split->length;
                leftmost->left_nl_cnt += split->nl_cnt;
                leftmost = leftmost->left;
            }
            leftmost->left = split;
            leftmost->left_size = split->length;
            leftmost->left_nl_cnt = split->nl_cnt;
            split->parent = leftmost;
        }
        int64_t nl_delta = start->nl_cnt + split->nl_cnt - prev_nl_cnt;
        bubble_meta_changes(pt, start, -(split->length + count), nl_delta);
        fix_insert(pt, split);
        pt->size -= count;
        pt->line_cnt += nl_delta;
    }
    PT_VALIDATE(pt);
}

const PTNode* piece_tree_node_at_line(const PieceTree* pt, size_t line, size_t* node_offset)
{
    GEM_ASSERT(pt != NULL);
    GEM_ASSERT(node_offset != NULL);
    GEM_ASSERT(line < pt->line_cnt);
    if(line == 0)
    {
        *node_offset = 0;
        return pt->root == SENTINEL ? NULL : left_test(pt->root);
    }

    PTNode* node = pt->root;
    while(node != SENTINEL)
    {
        if(line < node->left_nl_cnt)
            node = node->left;
        else if(line <= node->left_nl_cnt + node->nl_cnt)
        {
            const char* buf = piece_tree_get_node_start(pt, node);
            if(line == node->left_nl_cnt + node->nl_cnt &&
               buf[node->length - 1] == '\n')
            {
                *node_offset = 0;
                return piece_tree_next_inorder(pt, node);
            }
            line -= node->left_nl_cnt;
            size_t offset;
            for(offset = 0; line > 0 && offset < node->length; ++offset)
                if(buf[offset] == '\n')
                    --line;
            *node_offset = offset;
            return node;
        }
        else
        {
            line -= node->left_nl_cnt + node->nl_cnt;
            node = node->right;
        }
    }

    return NULL;
}

const PTNode* piece_tree_next_inorder(const PieceTree* pt, const PTNode* node)
{
    GEM_ASSERT(pt != NULL);
    GEM_ASSERT(node != SENTINEL);
    GEM_ASSERT(in_valid_range(pt, node));

    if(node->right != SENTINEL)
        return left_test(node->right);
    
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
    if(node->is_original)
        return pt->original.data + pt->original.line_starts[node->start.line] + node->start.column;
    return pt->added.data + pt->added.line_starts.data[node->start.line] + node->start.column;
}

size_t piece_tree_get_line_length(const PieceTree* pt, size_t line_num)
{
    GEM_ASSERT(pt != NULL);
    GEM_ASSERT(line_num < pt->line_cnt);

    if(line_num == pt->line_cnt - 1)
        return pt->size - piece_tree_get_offset(pt, line_num, 0);
    return piece_tree_get_offset(pt, line_num + 1, 0) - piece_tree_get_offset(pt, line_num, 0) - 1;
}

size_t piece_tree_get_offset(const PieceTree* pt, size_t line, size_t column)
{
    if(line == 0)
        return column;
    size_t left_len = 0;
    PTNode* node = pt->root;

    while(node != SENTINEL)
    {
        if(node->left != SENTINEL && node->left_nl_cnt >= line)
            node = node->left;
        else if(node->left_nl_cnt + node->nl_cnt >= line)
        {
            left_len += node->left_size;
            const size_t* ls = node->is_original ?
                                pt->original.line_starts :
                                pt->added.line_starts.data;
            
            return left_len + column + ls[node->start.line + line - node->left_nl_cnt] - ls[node->start.line] - node->start.column;
        }
        else
        {
            line -= node->left_nl_cnt + node->nl_cnt;
            left_len += node->left_size + node->length;
            node = node->right;
        }
    }

    return left_len;
}

BufferPos piece_tree_get_buffer_pos(const PieceTree* pt, size_t offset)
{
    GEM_ASSERT(pt != NULL);
    if(offset == pt->size)
    {
        BufferPos pos;
        pos.line = pt->line_cnt - 1;
        pos.column = offset - piece_tree_get_offset(pt, pos.line, 0);
        return pos;
    }
    GEM_ASSERT(offset < pt->size);
    PTNode* node = pt->root;
    BufferPos res = { 0, 0 };
    size_t original_offset = offset;

    while(node != SENTINEL)
    {
        if(node->left != SENTINEL && node->left_size >= offset)
            node = node->left;
        else if(node->left_size + node->length >= offset)
        {
            BufferPos in_buf = position_in_buffer(pt, node, offset-node->left_size);
            res.line += node->left_nl_cnt + in_buf.line - node->start.line;
            if(in_buf.line == node->start.line)
                res.column = original_offset - piece_tree_get_offset(pt, res.line, 0);
            else 
                res.column = in_buf.column;
            return res;
        }
        else
        {
            offset -= node->left_size + node->length;
            res.line += node->left_nl_cnt + node->nl_cnt;
            node = node->right;
        }
    }

    GEM_ERROR("This should not have been reached.");
    return res;
}

static void print_node_contents(const PieceTree* pt, const PTNode* node)
{
    if(!in_valid_range(pt, node) || ((PTInternNode*)node)->free)
        return;

    print_node_contents(pt, node->left);
    const char* buf = piece_tree_get_node_start(pt, node);
    for(size_t i = 0; i < node->length; ++i)
        putc(buf[i], stdout);
    print_node_contents(pt, node->right);
}

void piece_tree_print_contents(const PieceTree* pt)
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

static void print_node_metadata(const PieceTree* pt, const PTNode* node, size_t depth)
{
    if(!in_valid_range(pt, node) || ((PTInternNode*)node)->free)
        return;

    print_node_metadata(pt, node->right, depth + 1);
    for(size_t i = 0; i < depth; ++i)
        printf("  ");
    printf("(%c %s Start:%lu,%lu End:%lu,%lu Len:%lu NLCnt:%lu LeftSz:%lu LeftNL:%lu Id:%lu Parent:%lu Left:%lu Right:%lu)\n", 
           node->is_black ? 'B' : 'R',
           node->is_original ? "Or" : "Ad",
           node->start.line,
           node->start.column,
           node->end.line,
           node->end.column,
           node->length,
           node->nl_cnt,
           node->left_size,
           node->left_nl_cnt,
           node_id(pt, node),
           node_id(pt, node->parent),
           node_id(pt, node->left),
           node_id(pt, node->right));
    print_node_metadata(pt, node->left, depth + 1);
}

void piece_tree_print_tree(const PieceTree* pt)
{
    GEM_ASSERT(pt != NULL);
    printf("\nTree Metadata: (Size:%lu Lines:%lu)\n", pt->size, pt->line_cnt);
    if(pt->root != SENTINEL)
        print_node_metadata(pt, pt->root, 0);
    else
        printf("(Empty Tree)\n");
}

static PTNode* node_at_offset(PieceTree* pt, size_t offset, size_t* node_start_offset, bool exclusive)
{
    GEM_ASSERT(!exclusive || offset > 0);
    offset -= exclusive;
    GEM_ASSERT(offset < pt->size);
    PTNode* node = pt->root;
    size_t startoff = 0;

    while(node != SENTINEL)
    {
        if(node->left_size > offset) // Offset is in left subtree
            node = node->left;
        else if(node->left_size + node->length > offset) // Offset is inside this node
        {
            *node_start_offset = startoff + node->left_size;
            return node;
        }
        else // Offset is in right subtree
        {
            offset -= node->left_size + node->length;
            startoff += node->left_size + node->length;
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

static void free_node(PieceTree* pt, PTNode* node)
{
    if(node == SENTINEL)
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
    if(delta == 0)
        return;
    if(*invalid != SENTINEL && *invalid != NULL)
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
            PTInternNode* intern = pt->storage.data + i;
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

static size_t get_newln_count(const char* str, size_t len)
{
    size_t cnt = 0;
    for(size_t i = 0; i < len; ++i)
        if(str[i] == '\n')
            ++cnt;
    return cnt;
}

static void bubble_meta_changes(PieceTree* pt, PTNode* node, int64_t size_delta, int64_t newln_delta)
{
    GEM_ASSERT(in_valid_range(pt, node));
    while(node->parent != SENTINEL)
    {
        if(node == node->parent->left)
        {
            node->parent->left_size += size_delta;
            node->parent->left_nl_cnt += newln_delta;
        }
        node = node->parent;
    }
}

static PTNode create_node_and_append(PieceTree* pt, const char* str, size_t len)
{
    PTNode result = node_default();
    result.length = len;

    PTPosDA* ls = &pt->added.line_starts;
    result.start.line = ls->size - 1;
    result.start.column = pt->added.size - ls->data[ls->size - 1];

    for(size_t i = 0; i < len; ++i)
        if(str[i] == '\n')
        {
            da_append(&pt->added.line_starts, pt->added.size + i + 1);
            result.nl_cnt++;
        }

    da_append_arr(&pt->added, str, len);
    result.end.line = ls->size - 1;
    result.end.column = pt->added.size - ls->data[ls->size - 1];
    
    pt->size += len;
    pt->line_cnt += result.nl_cnt;

    return result;
}

static PTNode* split_node(PieceTree* pt, PTNode* node, size_t left_size, size_t right_size)
{
    GEM_ASSERT(in_valid_range(pt, node));
    GEM_ASSERT(left_size != 0 && right_size != 0);
    GEM_ASSERT(left_size + right_size <= node->length);
    PTNode* split = alloc_node(pt);
    *split = node_default();
    split->start = position_in_buffer(pt, node, node->length - right_size);
    split->end = node->end;
    split->length = right_size;
    split->nl_cnt = split->end.line - split->start.line;
    split->is_original = node->is_original;


    node->end = position_in_buffer(pt, node, left_size);
    node->length = left_size;
    node->nl_cnt -= split->nl_cnt;
    return split;
}

static BufferPos position_in_buffer(const PieceTree* pt, PTNode* node, size_t offset)
{
    GEM_ASSERT(in_valid_range(pt, node));
    GEM_ASSERT(offset <= node->length);

    const size_t* ls = node->is_original ? 
                        pt->original.line_starts : 
                        pt->added.line_starts.data;

    size_t buf_off = ls[node->start.line] + node->start.column + offset;
    size_t lo = node->start.line;
    size_t hi = node->end.line;

    size_t mid = (lo + hi) / 2;

    while(lo < hi)
    {
        if(buf_off < ls[mid])
            hi = mid - 1;
        else if(buf_off >= ls[mid + 1])
            lo = mid + 1;
        else
            break;

        mid = (lo + hi) / 2;
    }

    return (BufferPos) {
        .line = mid,
        .column = buf_off - ls[mid]
    };
}


static PTNode* left_test(const PTNode* node)
{
    while(node->left != SENTINEL)
        node = node->left;
    return (PTNode*)node;
}

static PTNode* UNUSED right_test(const PTNode* node)
{
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
    right->left_size += node->left_size + node->length;
    right->left_nl_cnt += node->left_nl_cnt + node->nl_cnt;
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

    node->left_size -= left->left_size + left->length;
    node->left_nl_cnt -= left->left_nl_cnt + left->nl_cnt;

    if(pt->root == node)
        pt->root = left;
    else if(node->parent->right == node)
        node->parent->right = left;
    else
        node->parent->left = left;

    left->right = node;
    node->parent = left;
}

static void delete_node(PieceTree* pt, PTNode* node)
{
    printf("Deleting node case ");
    GEM_ASSERT(node != SENTINEL);
    GEM_ASSERT(in_valid_range(pt, node));
    PTNode* x;
    PTNode* y;

    if(node->left == SENTINEL)
    {
        printf("1\n");
        y = node;
        x = node->right;
    }
    else if(node->right == SENTINEL)
    {
        printf("2\n");
        y = node;
        x = node->left;
    }
    else
    {
        printf("3\n");
        y = left_test(node->right);
        x = y->right;
    }

    if(y == pt->root)
    {
        printf("Here1\n");
        pt->root = x;
        x->parent = SENTINEL;
        x->is_black = true;
        free_node(pt, node);
        return;
    }

    bool y_red = !y->is_black;

    if(y == y->parent->left)
        y->parent->left = x;
    else
        y->parent->right = x;

    if(y == node)
    {
        printf("Here2\n");
        x->parent = y->parent;
        bubble_meta_changes(pt, y, -y->length, -y->nl_cnt);
    }
    else
    {
        printf("Here3\n");
        x->parent = y->parent == node ? y : y->parent;
        // recompute metadata
        
        y->left = node->left;
        y->right = node->right;
        y->parent = node->parent;
        y->is_black = node->is_black;

        if(node == pt->root)
            pt->root = y;
        else if(node == node->parent->left)
            node->parent->left = y;
        else
            node->parent->right = y;

        if(y->left != SENTINEL)
            y->left->parent = y;
        if(y->right != SENTINEL)
            y->right->parent = y;

        y->left_size = node->left_size;
        y->left_nl_cnt = node->left_nl_cnt;
        // recompute metadata

        if(x == x->parent->left)
        {
            //update newlines and left size
        }

    }

    free_node(pt, node);

    // recompute metadata
    if(y_red)
    {
        SENTINEL->parent = SENTINEL;
        return;
    }

    PTNode* z;
    while(x != pt->root && x->is_black)
    {
        if(x == x->parent->left)
        {
            z = x->parent->right;

            if(!z->is_black)
            {
                z->is_black = true;
                x->parent->is_black = false;
                left_rotate(pt, x->parent);
                z = x->parent->right;
            }

            if(z->left->is_black && z->right->is_black)
            {
                z->is_black = false;
                x = x->parent;
            }
            else
            {
                if(z->right->is_black)
                {
                    z->left->is_black = true;
                    z->is_black = false;
                    right_rotate(pt, z);
                    z = x->parent->right;
                }

                z->is_black = x->parent->is_black;
                x->parent->is_black = true;
                z->right->is_black = true;
                left_rotate(pt, x->parent);
                x = pt->root;
            }
        }
        else
        {
            z = x->parent->left;

            if(!z->is_black)
            {
                z->is_black = true;
                x->parent->is_black = false;
                right_rotate(pt, x->parent);
                z = x->parent->left;
            }

            if(z->left->is_black && z->right->is_black)
            {
                z->is_black = false;
                x = x->parent;
            }
            else
            {
                if(z->left->is_black)
                {
                    z->right->is_black = true;
                    z->is_black = false;
                    left_rotate(pt, z);
                    z = x->parent->left;
                }

                z->is_black = x->parent->is_black;
                x->parent->is_black = true;
                z->left->is_black = true;
                right_rotate(pt, x->parent);
                x = pt->root;
            }
        }
    }

    x->is_black = true;
    SENTINEL->parent = SENTINEL;
}

static inline void mark_free(PieceTree* pt, size_t start)
{
    size_t prev_size = pt->free_list.size;
    da_resize(&pt->free_list, prev_size + pt->storage.capacity - start);
    for(size_t i = prev_size, j = pt->storage.capacity - 1; i < pt->free_list.size; ++i, --j)
    {
        pt->free_list.data[i] = j;
        pt->storage.data[j].free = true;
    }
}

static inline bool in_valid_range(const PieceTree* pt, const PTNode* node)
{
    const PTInternNode* n = (const PTInternNode*)node;
    return n >= pt->storage.data && n < (pt->storage.data + pt->storage.capacity);
}

static inline size_t node_id(const PieceTree* pt, const PTNode* node)
{
    GEM_ASSERT(node == SENTINEL || in_valid_range(pt, node));
    uintptr_t i = ((uintptr_t)node - (uintptr_t)pt->storage.data) / sizeof(PTInternNode);
    return node == SENTINEL ? 0 : 1 + (size_t)i;
}

static inline PTNode node_default(void)
{
    return (PTNode){
        .start       = { 0, 0 },
        .end         = { 0, 0 },
        .length      = 0,
        .nl_cnt      = 0,
        .left_size   = 0,
        .left_nl_cnt = 0,
        .left        = SENTINEL,
        .right       = SENTINEL,
        .parent      = SENTINEL,
        .is_original = false,
        .is_black    = false
    };
}

#ifdef GEM_PT_VALIDATE
typedef struct
{
    size_t size;
    size_t nl_cnt;
    size_t black_height;
    size_t node_cnt;
} PTValidData;

static inline void pt_check_impl(const PieceTree* pt, const PTNode* node,
                                 const char* msg, size_t op, 
                                 int64_t actual, int64_t expected)
{
    static const char* op_str[] = {
        "",
        "",
        "< ",
        "<= ",
        "> ",
        ">= "
    };
    piece_tree_print_tree(pt);
    if(op == 0)
    {
        GEM_ERROR_ARGS("TREE INVALID (Node ID %lu): %s",
                       node == NULL ? 0 : node_id(pt, node),
                       msg);
    }
    else
    {
        GEM_ERROR_ARGS("TREE INVALID (Node ID %lu) (Actual: %ld Expected: %s%ld): %s",
                       node == NULL ? 0 : node_id(pt, node), 
                       actual, 
                       op_str[op],
                       expected,
                       msg);
    }
}

#define PT_CHECK(cond, msg) { if(!(cond)) pt_check_impl(pt, node, msg, 0, 0, 0); }
#define PT_CHECK_EQ(actual, expected, msg) { if((actual) != (expected)) pt_check_impl(pt, node, msg, 1, actual, expected); }
#define PT_CHECK_LT(actual, expected, msg) { if((actual) >= (expected)) pt_check_impl(pt, node, msg, 2, actual, expected); }
#define PT_CHECK_LE(actual, expected, msg) { if((actual) > (expected)) pt_check_impl(pt, node, msg, 3, actual, expected); }
#define PT_CHECK_GT(actual, expected, msg) { if((actual) <= (expected)) pt_check_impl(pt, node, msg, 4, actual, expected); }
#define PT_CHECK_GE(actual, expected, msg) { if((actual) < (expected)) pt_check_impl(pt, node, msg, 5, actual, expected); }

static PTValidData validate_node(const PieceTree* pt, const PTNode* node)
{
    PTValidData res;
    res.size = 0;
    res.black_height = 0;
    res.nl_cnt = 0;
    res.node_cnt = 0;
    if(node == SENTINEL)
        return res;
    
    // Check that the node ref itself is valid
    PT_CHECK(in_valid_range(pt, node), "Node not in valid range.");
    PT_CHECK(!((PTInternNode*)node)->free, "Node is not marked as allocated.");
    // Check that there are no loops 
    PT_CHECK(!node->used, "Node has already been encountered, there is a loop.");
    // Check red-black tree requirement
    PT_CHECK(node->is_black || node->parent->is_black, "Red node followed by red node.");

    // Mark node as seen for loop checking
    ((PTNode*)node)->used = true;

    // Check start and end bounds for validity.
    const size_t* ls;
    size_t line_count;
    size_t buf_size;
    if(node->is_original)
    {
        ls = pt->original.line_starts;
        line_count = pt->original.line_cnt;
        buf_size = pt->original.size;
    }
    else
    {
        ls = pt->added.line_starts.data;
        line_count = pt->added.line_starts.size;
        buf_size = pt->added.size;
    }
    PT_CHECK_LT((size_t)node->start.line, line_count, "Start line out of bounds.");
    PT_CHECK_LT((size_t)node->end.line, line_count, "End line out of bounds.");
    size_t start_check = (size_t)node->start.line == line_count - 1 ?
                            buf_size - ls[node->start.line] :
                            ls[node->start.line + 1] - ls[node->start.line];
    size_t end_check = (size_t)node->end.line == line_count - 1 ?
                            buf_size - ls[node->end.line] :
                            ls[node->end.line + 1] - ls[node->end.line];

    PT_CHECK_LT((size_t)node->start.column, start_check, "Start column out of bounds.");
    PT_CHECK_LE((size_t)node->end.column, end_check, "End column out of bounds.");
    PT_CHECK(node->start.line < node->end.line || 
             node->start.column < node->end.column, "End is before or in the same place as start.");
    PT_CHECK_EQ(node->length, (ls[node->end.line] - ls[node->start.line] - node->start.column + node->end.column), 
                   "Length is invalid.");
    PT_CHECK_EQ((int64_t)node->nl_cnt, node->end.line - node->start.line, "Newline count is invalid.");

    PTValidData left = validate_node(pt, node->left);
    PTValidData right = validate_node(pt, node->right);
    PT_CHECK_EQ(node->left_nl_cnt, left.nl_cnt, "Left newline count is invalid.");
    PT_CHECK_EQ(node->left_size, left.size, "Left size is invalid.");
    PT_CHECK(left.black_height == right.black_height, "Black height is mismatched.");

    res.black_height = left.black_height + node->is_black;
    res.nl_cnt = left.nl_cnt + node->nl_cnt + right.nl_cnt;
    res.size = left.size + node->length + right.size;
    res.node_cnt = left.node_cnt + 1 + right.node_cnt;
    return res;
}

static void validate_tree(const PieceTree* pt)
{
    GEM_ASSERT(pt != NULL);
    // To check:
    // Sentinel validity
    PTNode* node = SENTINEL;
    PT_CHECK(SENTINEL->left == SENTINEL && 
                SENTINEL->right == SENTINEL &&
                SENTINEL->parent == SENTINEL &&
                SENTINEL->is_black, "Sentinel is invalid.");
    
    PT_CHECK(pt->root->parent == SENTINEL && 
                   pt->root->is_black, "Root is invalid.");

    size_t cnt = 0;
    for(size_t i = 0; i < pt->storage.capacity; ++i)
        if(!pt->storage.data[i].free)
        {
            cnt++;
            pt->storage.data[i].node.used = false;
        }

    PTValidData data = validate_node(pt, pt->root);
    PT_CHECK_EQ(pt->size, data.size, "Tree size is invalid.");
    PT_CHECK_EQ(pt->line_cnt, data.nl_cnt + 1, "Tree line count is invalid.");
    PT_CHECK_EQ(cnt, data.node_cnt, "Memory leak in node buffer. Ensure nodes are free when detached.");
}
#endif
