#include "piecetree.h"
#include "da.h"
#include "core/core.h"

#include <string.h>

#define PT_INVALID        UINT64_MAX
#define INITIAL_STORAGE   (1 << 7)
#define INITIAL_ADDED_CAP (1 << 12)
#define INITIAL_LINE_CAP  (1 << 6)

#ifdef GEM_PT_VALIDATE
static void validate_tree(const PieceTree* pt);
    #define PT_VALIDATE(pt) { validate_tree(pt); }
#else
    #define PT_VALIDATE(pt)
#endif

typedef struct __PTNodeIntern NodeIntern;
struct alignas(8) __PTNodeIntern
{
    PTNode node;
    bool   free;
    size_t next;
};

static PTNode* split_node(PieceTree* pt, PTNode* node, size_t left_size, size_t right_size);
static void    fix_insert(PieceTree* pt, PTNode* node);
static void    left_rotate(PieceTree* pt, PTNode* node);
static void    right_rotate(PieceTree* pt, PTNode* node);
static void    delete_node(PieceTree* pt, PTNode* node);
static void    bubble_meta_changes(PieceTree* pt, PTNode* node, PTNode* stop, int64_t size_delta, int64_t newln_delta);
static PTNode* create_node_and_append(PieceTree* pt, const char* str, size_t len);

static PTNode*   node_at_offset(PieceTree* pt, size_t offset, size_t* node_start_offset, bool tail);
static BufferPos position_in_buffer(const PieceTree* pt, PTNode* node, size_t offset);
static PTNode*   next(PieceTree* pt, PTNode* node);
static PTNode*   left_test(const PTNode* node);
static PTNode*   right_test(const PTNode* node);

static PTNode* alloc_node(PieceTree* pt);
static void    free_node(PieceTree* pt, PTNode* node);
static void    expand_node_storage(PieceTree* pt, size_t capacity);

static inline void   mark_free(PieceTree* pt, size_t start);
static inline bool   is_valid_node(const PieceTree* pt, const PTNode* node);
static inline size_t node_id(const PieceTree* pt, const PTNode* node);
static inline PTNode node_default(void);

static PTNode s_Sentinel = {
    .left     = &s_Sentinel,
    .right    = &s_Sentinel,
    .parent   = &s_Sentinel,
    .is_black = true
};
static PTNode* const SENTINEL = &s_Sentinel;

// TODO: Change this to take ownership of the pointer so copying isnt necessary.
void piece_tree_init(PieceTree* pt, const char* original_src, size_t size, bool copy)
{
    GEM_ASSERT(pt != NULL);
    memset(pt, 0, sizeof(PieceTree));

    pt->storage.capacity = INITIAL_STORAGE;
    pt->storage.nodes = malloc(sizeof(NodeIntern) * pt->storage.capacity);
    GEM_ENSURE(pt->storage.nodes != NULL);
    pt->storage.free_head = PT_INVALID;

    // TODO: Replace the 0 initial capacities with something more reasonable
    // to start with.
    da_init(&pt->added, INITIAL_ADDED_CAP);
    da_init(&pt->added.line_starts, INITIAL_LINE_CAP);
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

    for(size_t i = 0; i < size; ++i)
        if(pt->original.data[i] == '\n')
            pt->line_cnt++;

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

    pt->root = (PTNode*)pt->storage.nodes;
    *(pt->root) = node_default();

    pt->root->end.line     = pt->line_cnt - 1;
    pt->root->end.column   = size - pt->original.line_starts[pt->line_cnt - 1];
    pt->root->length       = size;
    pt->root->is_original  = true;
    pt->root->is_black     = true;
    pt->root->nl_cnt       = pt->line_cnt - 1;

    pt->storage.nodes[0].next = PT_INVALID;
    pt->storage.nodes[0].free = false;

    PT_VALIDATE(pt);
}

void piece_tree_free(PieceTree* pt)
{
    free(pt->storage.nodes);
    free((void*)pt->original.data);
    free((void*)pt->original.line_starts);

    da_free_data(&pt->added);
    da_free_data(&pt->added.line_starts);
}

void piece_tree_insert(PieceTree* pt, const char* data, size_t count, size_t offset)
{
    GEM_ASSERT(pt != NULL);
    GEM_ASSERT(data != NULL);
    GEM_ASSERT(offset <= pt->size);
    if(count == 0 || data[0] == '\0')
        return;

    expand_node_storage(pt, 2);

    PTNode* node;
    PTNode* new = create_node_and_append(pt, data, count);
    size_t  node_start_offset;

    if(pt->root == SENTINEL)
    {
        pt->root = new;
        pt->root->is_black = true;
        PT_VALIDATE(pt);
        return;
    }

    // The only case where we insert to the left of a node is
    // when we are inserting at the very beginning of the document.
    // This ensures we append as much as possible.
    if(offset == 0) 
    {
        node = left_test(pt->root);
        new->parent = node;
        node->left = new;
        bubble_meta_changes(pt, new, pt->root, new->length, new->nl_cnt);
        fix_insert(pt, new);
        PT_VALIDATE(pt);
        return;
    }

    node = node_at_offset(pt, offset, &node_start_offset, true);

    GEM_ASSERT(offset > node_start_offset);
    if(node_start_offset + node->length > offset) // Split node
    {
        size_t node_new_len = offset - node_start_offset;
        PTNode* split = split_node(pt, node, node_new_len, node->length - node_new_len);

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
            bubble_meta_changes(pt, node, pt->root, new->length, new->nl_cnt);
        }
        else
        {
            PTNode* leftmost = left_test(node->right);
            leftmost->left = new;
            new->parent = leftmost;
            bubble_meta_changes(pt, new, pt->root, new->length, new->nl_cnt);
        }
        fix_insert(pt, new);
    }
    else if(!node->is_original && node->end.line == new->start.line && 
            node->end.column == new->start.column) // Append to node
    {
        node->end = new->end;
        node->length += count;
        node->nl_cnt += new->nl_cnt;
        bubble_meta_changes(pt, node, pt->root, count, new->nl_cnt);
        free_node(pt, new);
    }
    else // Insert to the right of node
    {
        if(node->right == SENTINEL) 
        {
            node->right = new;
            new->parent = node;
            bubble_meta_changes(pt, node, pt->root, new->length, new->nl_cnt);
        }
        else
        {
            PTNode* leftmost = left_test(node->right);
            leftmost->left = new;
            new->parent = leftmost;
            bubble_meta_changes(pt, new, pt->root, new->length, new->nl_cnt);
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

    expand_node_storage(pt, 2);

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
            bubble_meta_changes(pt, start, pt->root, -count, start->nl_cnt - prev_nl_cnt);
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
            bubble_meta_changes(pt, start, pt->root, -count, start->nl_cnt - prev_nl_cnt);
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
        bubble_meta_changes(pt, start, pt->root, -count, nl_delta);
        fix_insert(pt, split);
        pt->size -= count;
        pt->line_cnt += nl_delta;
        PT_VALIDATE(pt);
        return;
    }

    // TODO: Maybe make this dynamic array permanent to avoid
    //       unnecessary mallocs and reallocs?
    struct {
        PTNode** data;
        size_t capacity;
        size_t size;
    } nodes_to_del;
    da_init(&nodes_to_del, 10);

    size_t prev_length;
    size_t prev_nl_cnt;

    // TODO: Separate these into functions like delete_head and delete_tail
    prev_length = start->length;
    prev_nl_cnt = start->nl_cnt;
    start->end = position_in_buffer(pt, start, offset - start_offset);
    start->nl_cnt = start->end.line - start->start.line;
    start->length = offset - start_offset;
    bubble_meta_changes(pt, start, pt->root, start->length - prev_length, start->nl_cnt - prev_nl_cnt);
    pt->line_cnt -= prev_nl_cnt - start->nl_cnt;
    if(start->length == 0)
        da_append(&nodes_to_del, start);

    prev_length = end->length;
    prev_nl_cnt = end->nl_cnt;
    end->start = position_in_buffer(pt, end, offset + count - end_offset);
    end->nl_cnt = end->end.line - end->start.line;
    end->length -= offset + count - end_offset;
    bubble_meta_changes(pt, end, pt->root, end->length - prev_length, end->nl_cnt - prev_nl_cnt);
    pt->line_cnt -= prev_nl_cnt - end->nl_cnt;
    if(end->length == 0)
        da_append(&nodes_to_del, end);

    PTNode* cur = next(pt, start);
    while(cur != SENTINEL && cur != end)
    {
        da_append(&nodes_to_del, cur);
        cur = next(pt, cur);
    }

    for(size_t i = 0; i < nodes_to_del.size; ++i)
    {
        pt->line_cnt -= nodes_to_del.data[i]->nl_cnt;
        delete_node(pt, nodes_to_del.data[i]);
    }
    da_free_data(&nodes_to_del);

    pt->size -= count;
    PT_VALIDATE(pt);
    
}

const PTNode* piece_tree_node_at(const PieceTree* pt, size_t offset, size_t* node_start_offset)
{
    GEM_ASSERT(pt != NULL);
    return node_at_offset((PieceTree*)pt, offset, node_start_offset, false);
}

const PTNode* piece_tree_node_at_line(const PieceTree* pt, size_t line, size_t* node_offset)
{
    GEM_ASSERT(pt != NULL);
    GEM_ASSERT(node_offset != NULL);
    GEM_ASSERT(line < pt->line_cnt);
    
    *node_offset = 0;
    if(line == 0)
        return pt->root == SENTINEL ? NULL : left_test(pt->root);

    PTNode* node = pt->root;
    while(node != SENTINEL)
    {
        if(node->left != SENTINEL && line <= node->left_nl_cnt)
            node = node->left;
        else if(line < node->left_nl_cnt + node->nl_cnt)
        {
            const size_t* ls = node->is_original ?
                                pt->original.line_starts :
                                pt->added.line_starts.data;
            line += node->start.line - node->left_nl_cnt;
            *node_offset = ls[line] - ls[node->start.line] - node->start.column;
            return node;
        }
        else if(line == node->left_nl_cnt + node->nl_cnt)
        {
            const size_t* ls = node->is_original ?
                                pt->original.line_starts :
                                pt->added.line_starts.data;
            const char* buf = piece_tree_get_node_start(pt, node);
            if(buf[node->length - 1] == '\n')
                return piece_tree_next_inorder(pt, node);
            *node_offset = ls[node->end.line] - ls[node->start.line] - node->start.column;
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
    if(node == NULL)
        return pt->root == SENTINEL ? NULL : left_test(pt->root);
    GEM_ASSERT(node != SENTINEL);
    GEM_ASSERT(is_valid_node(pt, node));

    PTNode* n = next((PieceTree*)pt, (PTNode*)node);
    return n == SENTINEL ? NULL : n;
}

const PTNode* piece_tree_prev_inorder(const PieceTree* pt, const PTNode* node)
{
    GEM_ASSERT(pt != NULL);
    if(node == NULL)
        return pt->root == SENTINEL ? NULL : right_test(pt->root);
    GEM_ASSERT(node != SENTINEL);
    GEM_ASSERT(is_valid_node(pt, node));

    if(node->left != SENTINEL)
        return right_test(node->left);
    
    while(node != pt->root)
    {
        if(node == node->parent->right)
            return node->parent;
        node = node->parent;
    }

    return NULL;
}

const char* piece_tree_get_node_start(const PieceTree* pt, const PTNode* node)
{
    GEM_ASSERT(pt != NULL);
    GEM_ASSERT(node != SENTINEL);
    GEM_ASSERT(is_valid_node(pt, node));
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
    GEM_ASSERT(((NodeIntern*)node)->next != PT_INVALID);
    if(!is_valid_node(pt, node))
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
    if(!is_valid_node(pt, node))
        return;

    print_node_metadata(pt, node->right, depth + 1);
    for(size_t i = 0; i < depth; ++i)
        printf("  ");
    printf("(Id:%lu Parent:%lu Left:%lu Right:%lu %c %s Start:%lu,%lu End:%lu,%lu Len:%lu NLCnt:%lu LeftSz:%lu LeftNL:%lu)\n", 
           node_id(pt, node),
           node_id(pt, node->parent),
           node_id(pt, node->left),
           node_id(pt, node->right),
           node->is_black ? 'B' : 'R',
           node->is_original ? "Or" : "Ad",
           node->start.line,
           node->start.column,
           node->end.line,
           node->end.column,
           node->length,
           node->nl_cnt,
           node->left_size,
           node->left_nl_cnt);
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

size_t piece_tree_node_id(const PieceTree* pt, const PTNode* node)
{
    GEM_ASSERT(pt != NULL);
    return node_id(pt, node);
}

static PTNode* split_node(PieceTree* pt, PTNode* node, size_t left_size, size_t right_size)
{
    GEM_ASSERT(is_valid_node(pt, node));
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
    node->nl_cnt = node->end.line - node->start.line;
    return split;
}

static void fix_insert(PieceTree* pt, PTNode* node)
{
    GEM_ASSERT(is_valid_node(pt, node));
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
    GEM_ASSERT(node != SENTINEL);
    GEM_ASSERT(is_valid_node(pt, node));
    PTNode* x;
    PTNode* y;

    if(node->left == SENTINEL)
    {
        y = node;
        x = node->right;
    }
    else if(node->right == SENTINEL)
    {
        y = node;
        x = node->left;
    }
    else
    {
        y = left_test(node->right);
        x = y->right;
    }

    if(y == pt->root)
    {
        pt->root = x;
        x->parent = SENTINEL;
        x->is_black = true;
        free_node(pt, node);
        return;
    }

    bool y_red = !y->is_black;

    if(y == y->parent->left)
    {
        y->parent->left = x;
        y->parent->left_size -= y->length;
        y->parent->left_nl_cnt -= y->nl_cnt;
    }
    else
        y->parent->right = x;

    if(y == node)
    {
        x->parent = y->parent;
        bubble_meta_changes(pt, x->parent, pt->root, -y->length, -y->nl_cnt);
    }
    else
    {
        if(y->parent == node)
            x->parent = y;
        else
        {
            x->parent = y->parent;
            bubble_meta_changes(pt, x->parent, node->right, -y->length, -y->nl_cnt);
        }

        
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
        bubble_meta_changes(pt, y, pt->root, -node->length, -node->nl_cnt);
    }

    free_node(pt, node);

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

static void bubble_meta_changes(PieceTree* pt, PTNode* node, PTNode* stop,
                                int64_t size_delta, int64_t newln_delta)
{
    GEM_ASSERT(node == SENTINEL || is_valid_node(pt, node));
    while(node != stop && node->parent != SENTINEL)
    {
        if(node == node->parent->left)
        {
            node->parent->left_size += size_delta;
            node->parent->left_nl_cnt += newln_delta;
        }
        node = node->parent;
    }
}

static PTNode* create_node_and_append(PieceTree* pt, const char* str, size_t len)
{
    PTNode* result = alloc_node(pt);
    *result = node_default();
    result->length = len;

    PTPosDA* ls = &pt->added.line_starts;
    result->start.line = ls->size - 1;
    result->start.column = pt->added.size - ls->data[ls->size - 1];

    for(size_t i = 0; i < len; ++i)
        if(str[i] == '\n')
        {
            da_append(&pt->added.line_starts, pt->added.size + i + 1);
            result->nl_cnt++;
        }

    da_append_arr(&pt->added, str, len);
    result->end.line = ls->size - 1;
    result->end.column = pt->added.size - ls->data[ls->size - 1];
    
    pt->size += len;
    pt->line_cnt += result->nl_cnt;

    return result;
}

static PTNode* node_at_offset(PieceTree* pt, size_t offset, size_t* node_start_offset, bool tail)
{
    GEM_ASSERT(!tail || offset > 0);
    offset -= tail;
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

static BufferPos position_in_buffer(const PieceTree* pt, PTNode* node, size_t offset)
{
    GEM_ASSERT(is_valid_node(pt, node));
    GEM_ASSERT(offset <= node->length);
    if(offset == 0)
        return node->start;
    if(offset == node->length)
        return node->end;

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

static PTNode* next(PieceTree* pt, PTNode* node)
{
    if(node->right != SENTINEL)
        return left_test(node->right);
    
    while(node != pt->root)
    {
        if(node == node->parent->left)
            return node->parent;
        node = node->parent;
    }

    return SENTINEL;
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

static PTNode* alloc_node(PieceTree* pt)
{
    PTStorage* s = &pt->storage;
    GEM_ASSERT(s->free_count > 0);
    NodeIntern* intern = s->nodes + s->free_head;
    GEM_ASSERT(intern->free);
    
    s->free_count--;
    s->free_head = intern->next;
    intern->next = PT_INVALID;
    intern->free = false;
    return &intern->node;
}

static void free_node(PieceTree* pt, PTNode* node)
{
    if(node == SENTINEL)
        return;

    PTStorage* s = &pt->storage;
    GEM_ASSERT(is_valid_node(pt, node));
    NodeIntern* internal = (NodeIntern*)node;
    GEM_ASSERT(!internal->free);
    size_t index = node_id(pt, node) - 1;

    internal->free = true;
    internal->next = s->free_head;
    s->free_head = index;
    s->free_count++;
}

static void fix_pointer(PTNode** invalid, uintptr_t delta)
{
    PTNode* val = *invalid;
    if(delta != 0 && val != SENTINEL && val != NULL)
        *invalid = (PTNode*)((uintptr_t)val + delta);
}

static void expand_node_storage(PieceTree* pt, size_t increase)
{
    PTStorage* s = &pt->storage;
    if(increase == 0 || increase <= s->free_count)
        return;

    NodeIntern* prev_pointer = s->nodes;
    size_t prev_cap = s->capacity;
    size_t new_cap = s->capacity + increase;
    new_cap += new_cap >> 1;

    s->nodes = realloc(s->nodes, sizeof(NodeIntern) * new_cap);
    GEM_ENSURE(s->nodes != NULL);
    if(prev_pointer != s->nodes)
    {
        uintptr_t delta = (uintptr_t)s->nodes - (uintptr_t)prev_pointer;
        fix_pointer(&pt->root, delta);
        for(NodeIntern* intern = s->nodes; intern < s->nodes + prev_cap; intern++)
        {
            if(intern->next >= prev_cap)
            {
                fix_pointer(&intern->node.left, delta);
                fix_pointer(&intern->node.right, delta);
                fix_pointer(&intern->node.parent, delta);
            }
        }
    }
    s->capacity = new_cap;
    mark_free(pt, prev_cap);
}

static inline void mark_free(PieceTree* pt, size_t start)
{
    PTStorage* s = &pt->storage;
    GEM_ASSERT(s->capacity > 0);
    GEM_ASSERT(start < s->capacity);
    NodeIntern* last;
    if(s->free_head == PT_INVALID)
        s->free_head = start;
    else
    {
        last = s->nodes + s->free_head;
        while(last->next != PT_INVALID)
            last = s->nodes + last->next;
        last->next = start;
    }
    last = s->nodes + start;
    for(size_t i = start + 1; i < s->capacity; ++i)
    {
        last->next = i;
        last->free = true;
        last++;
    }
    last->next = PT_INVALID;
    last->free = true;
    s->free_count += s->capacity - start;
}

static inline bool is_valid_node(const PieceTree* pt, const PTNode* node)
{
    const NodeIntern* n = (const NodeIntern*)node;
    return n >= pt->storage.nodes && 
           n < (pt->storage.nodes + pt->storage.capacity) &&
           n->next == PT_INVALID;
}

static inline size_t node_id(const PieceTree* pt, const PTNode* node)
{
    GEM_ASSERT(node == SENTINEL || is_valid_node(pt, node));
    uintptr_t i = ((uintptr_t)node - (uintptr_t)pt->storage.nodes) / sizeof(NodeIntern);
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
        "< ",
        "<= ",
        "> ",
        ">= "
    };
    piece_tree_print_tree(pt);
    if(node == NULL)
    {
        GEM_ERROR_ARGS("TREE INVALID (Actual: %ld Expected: %s%ld): %s",
                       actual, 
                       op_str[op],
                       expected,
                       msg);
    }
    else
    {
        GEM_ERROR_ARGS("TREE NODE INVALID (ID %lu) (Actual: %ld Expected: %s%ld): %s",
                       node_id(pt, node), 
                       actual, 
                       op_str[op],
                       expected,
                       msg);
    }
}

static inline void pt_check_simple(const PieceTree* pt, const PTNode* node,
                                   const char* msg)
{
    piece_tree_print_tree(pt);
    if(node == NULL)
    {
        GEM_ERROR_ARGS("TREE INVALID: %s",
                       msg);
    }
    else
    {
        GEM_ERROR_ARGS("TREE NODE INVALID (ID %lu): %s",
                       node_id(pt, node), 
                       msg);
    }

}

#define PT_CHECK(cond, msg) { if(!(cond)) pt_check_simple(pt, node, msg); }
#define PT_CHECK_EQ(actual, expected, msg) { if((actual) != (expected)) pt_check_impl(pt, node, msg, 0, actual, expected); }
#define PT_CHECK_LT(actual, expected, msg) { if((actual) >= (expected)) pt_check_impl(pt, node, msg, 1, actual, expected); }
#define PT_CHECK_LE(actual, expected, msg) { if((actual) > (expected)) pt_check_impl(pt, node, msg, 2, actual, expected); }
#define PT_CHECK_GT(actual, expected, msg) { if((actual) <= (expected)) pt_check_impl(pt, node, msg, 3, actual, expected); }
#define PT_CHECK_GE(actual, expected, msg) { if((actual) < (expected)) pt_check_impl(pt, node, msg, 4, actual, expected); }

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
    PT_CHECK(is_valid_node(pt, node), "Node pointer is invalid.");
    PT_CHECK(((NodeIntern*)node)->next == PT_INVALID, "Node is not marked as allocated.");
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
    PTNode* node = NULL;
    PT_CHECK(SENTINEL->left == SENTINEL && 
                SENTINEL->right == SENTINEL &&
                SENTINEL->parent == SENTINEL &&
                SENTINEL->is_black, "Sentinel is invalid.");
    
    PT_CHECK(pt->root->parent == SENTINEL && 
                   pt->root->is_black, "Root is invalid.");

    size_t cnt = 0;
    for(size_t i = 0; i < pt->storage.capacity; ++i)
        if(!pt->storage.nodes[i].free)
        {
            cnt++;
            pt->storage.nodes[i].node.used = false;
        }

    PTValidData data = validate_node(pt, pt->root);
    PT_CHECK_EQ(pt->size, data.size, "Tree size is invalid.");
    PT_CHECK_EQ(pt->line_cnt, data.nl_cnt + 1, "Tree line count is invalid.");
    PT_CHECK_EQ(cnt, data.node_cnt, "Memory leak in node buffer. Ensure nodes are free when detached.");
    PT_CHECK_EQ(pt->storage.capacity - cnt, pt->storage.free_count, "Free count is incorrect.");
}
#endif
