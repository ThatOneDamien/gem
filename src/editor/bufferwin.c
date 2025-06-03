#include "bufferwin.h"
#include "core/app.h"
#include "core/core.h"
#include "core/keycode.h"
#include "fileman/fileio.h"
#include "fileman/path.h"
#include "render/font.h"
#include "render/renderer.h"

#include <math.h>
#include <string.h>

#define MIN(x, y) ((x) > (y) ? (y) : (x))
#define DEFAULT_PADDING ((GemPadding){ .left = 10, .right = 0, .top = 0, .bottom = 0 })
#define MIN_WIN_WIDTH 80
#define MIN_WIN_HEIGHT 120

static void      render_frame(WinFrame* frame);
static void      update_frame(WinFrame* frame, GemQuad* cur); //Temporary
static BufferPos actual_to_vis(const BufferWin* bufwin, BufferPos actual);
static BufferPos vis_to_actual(const BufferWin* bufwin, BufferPos vis);
static void      clamp_val(int64_t* val, int64_t min, int64_t max);

static WinFrame* left_test(WinFrame* start);


static BufferWin* s_CurWin;
static WinFrame*  s_RootFrame;

void bufwin_init_root_frame(void)
{
    s_CurWin = calloc(1, sizeof(BufferWin));
    GEM_ENSURE(s_CurWin != NULL);
    s_CurWin->text_padding = DEFAULT_PADDING;
    s_CurWin->local_dir = get_cwd_path();

    s_RootFrame = &s_CurWin->frame;
    s_RootFrame->type = FRAME_TYPE_LEAF;
}

void bufwin_open(char* filepath)
{
    GEM_ASSERT(s_CurWin != NULL);
    s_CurWin->bufnr = filepath == NULL ?
                        buffer_open_empty() :
                        buffer_open_file(filepath);
    if(s_CurWin->bufnr == -1)
        s_CurWin->bufnr = buffer_open_empty();
    bufwin_update_view(s_CurWin);
}

BufferWin* bufwin_copy(BufferWin* bufwin)
{
    GEM_ASSERT(bufwin != NULL);
    BufferWin* copy = malloc(sizeof(BufferWin));
    GEM_ENSURE(copy != NULL);
    copy->cursor = bufwin->cursor;
    copy->view = bufwin->view;
    copy->text_padding = bufwin->text_padding;
    copy->frame = bufwin->frame;
    if(bufwin->local_dir == NULL)
        copy->local_dir = NULL;
    else
    {
        copy->local_dir = malloc(GEM_PATH_MAX);
        strcpy(copy->local_dir, bufwin->local_dir);
    }
    copy->bufnr = bufwin->bufnr;
    copy->mode = WIN_MODE_NORMAL;
    return copy;
}

BufferWin* bufwin_split(bool vsplit)
{
    GEM_ASSERT(s_CurWin != NULL);

    BufferWin* new_win;
    WinFrame*  new_frame;
    WinFrame*  parent;
    WinFrame*  frame = &s_CurWin->frame;

    if((vsplit  && frame->bounding_box.tr.x - frame->bounding_box.bl.x < MIN_WIN_WIDTH * 2) ||
       (!vsplit && frame->bounding_box.bl.y - frame->bounding_box.tr.y < MIN_WIN_HEIGHT * 2))
        return NULL;

    parent = malloc(sizeof(WinFrame));
    new_win = bufwin_copy(s_CurWin);
    GEM_ENSURE(parent != NULL);
    GEM_ENSURE(new_win != NULL);
    new_frame = &new_win->frame;

    if(frame == s_RootFrame)
        s_RootFrame = parent;
    else if(frame == frame->parent->left)
        frame->parent->left = parent;
    else
        frame->parent->right = parent;

    parent->type = vsplit ? FRAME_TYPE_VSPLIT : FRAME_TYPE_HSPLIT;
    parent->left = frame;
    parent->right = new_frame;
    parent->parent = frame->parent;
    parent->bounding_box = frame->bounding_box;

    frame->parent = parent;

    new_frame->type = FRAME_TYPE_LEAF;
    new_frame->parent = parent;
    GemQuad initial = parent->bounding_box;
    update_frame(parent, &initial);

    return new_win;
}

BufNr bufwin_get_bufnr(void)
{
    return s_CurWin->bufnr;
}

void bufwin_set_current(BufferWin* bufwin)
{
    GEM_ASSERT(bufwin != NULL);
    s_CurWin = bufwin;
}

void bufwin_close(void)
{
    GEM_ASSERT(s_CurWin != NULL);
    if(&s_CurWin->frame == s_RootFrame)
    {
        printf("Tried to delete root frame.\n");
        return;
    }
    WinFrame* frame = &s_CurWin->frame;
    WinFrame* parent = frame->parent;
    WinFrame* other;
    if(frame == parent->left)
        other = parent->right;
    else
        other = parent->left;
    
    if(parent == s_RootFrame)
        s_RootFrame = other;    
    else if(parent == parent->parent->left)
        parent->parent->left = other;
    else
        parent->parent->right = other;

    other->parent = parent->parent;
    update_frame(other, &parent->bounding_box);
    free(s_CurWin);
    free(parent);

    s_CurWin = frame_win(left_test(other));
}


void bufwin_set_cursor(BufferWin* bufwin, int64_t line, int64_t column)
{
    GEM_ASSERT(bufwin != NULL);
    
    Cursor* c = &bufwin->cursor;
    const PieceTree* pt = &buffer_get(bufwin->bufnr)->contents;
    size_t line_len;

    clamp_val(&line, 0, pt->line_cnt - 1);
    line_len = piece_tree_get_line_length(pt, line);
    clamp_val(&column, 0, line_len);
    if(line == c->pos.line && column == c->pos.column)
        return;

    if(column != c->pos.column)
        c->horiz = column;
    c->vis.line = line;
    c->vis.column = column;
    c->pos = vis_to_actual(bufwin, c->vis);
    c->offset = piece_tree_get_offset_bp(pt, c->pos);
}

void bufwin_move_cursor_line(BufferWin* bufwin, int64_t line_delta)
{
    GEM_ASSERT(bufwin != NULL);
    if(line_delta == 0)
        return;

    const PieceTree* pt = &buffer_get(bufwin->bufnr)->contents;
    Cursor* c = &bufwin->cursor;
    clamp_val(&line_delta, -c->pos.line, pt->line_cnt - 1 - c->pos.line);
    if(line_delta != 0)
    {
        c->vis.line += line_delta;
        c->vis.column = piece_tree_get_line_length(pt, c->vis.line);
        c->vis.column = MIN(c->horiz, actual_to_vis(bufwin, c->vis).column);
        c->pos = vis_to_actual(bufwin, c->vis);
        c->offset = piece_tree_get_offset_bp(pt, c->pos);
        bufwin_put_cursor_in_view(bufwin);
        if(bufwin->frame.visible)
            gem_request_redraw();
    }
}

void bufwin_move_cursor_horiz(BufferWin* bufwin, int64_t horiz_delta)
{
    GEM_ASSERT(bufwin != NULL);
    if(horiz_delta == 0)
        return;

    const PieceTree* pt = &buffer_get(bufwin->bufnr)->contents;
    Cursor* c = &bufwin->cursor;
    clamp_val(&horiz_delta, -c->offset, pt->size - c->offset);
    c->offset += horiz_delta;
    if(horiz_delta != 0)
    {
        c->pos = piece_tree_get_buffer_pos(pt, c->offset);
        c->vis = actual_to_vis(bufwin, c->pos);
        c->horiz = c->vis.column;
        bufwin_put_cursor_in_view(bufwin);
        if(bufwin->frame.visible)
            gem_request_redraw();
    }
}

void bufwin_cursor_refresh(BufferWin* bufwin)
{
    GEM_ASSERT(bufwin != NULL);
    Cursor* c = &bufwin->cursor;
    const PieceTree* pt = &buffer_get(bufwin->bufnr)->contents;
    c->pos = vis_to_actual(bufwin, c->vis);
    c->offset = piece_tree_get_offset_bp(pt, c->pos);
}

void bufwin_set_view(BufferWin* bufwin, int64_t start_line, int64_t start_col)
{
    (void)start_col;
    GEM_ASSERT(bufwin != NULL);
    const PieceTree* pt = &buffer_get(bufwin->bufnr)->contents;
    clamp_val(&start_line, 0, pt->line_cnt - 1);
    if(start_col < 0)
        start_col = 0;
    if(start_line != bufwin->view.start.line || 
       start_col != bufwin->view.start.column)
    {
        bufwin->view.start.line = start_line;
        bufwin->view.start.column = start_col;
        if(bufwin->frame.visible)
            gem_request_redraw();
    }
}

void bufwin_move_view(BufferWin* bufwin, int64_t line_delta)
{
    GEM_ASSERT(bufwin != NULL);
    if(line_delta == 0)
        return;

    bufwin_set_view(bufwin, bufwin->view.start.line + line_delta, bufwin->view.start.column);
}

void bufwin_put_cursor_in_view(BufferWin* bufwin)
{
    // TODO: Change hardcoded 4 to be customizable
    GEM_ASSERT(bufwin != NULL);
    BufferPos* vis = &bufwin->cursor.vis;
    int64_t new_line = bufwin->view.start.line;
    int64_t new_col = bufwin->view.start.column;
    if(vis->line < bufwin->view.start.line + 4) 
        new_line = vis->line - 4;
    else if(vis->line > bufwin->view.count.line - 4 + bufwin->view.start.line)
        new_line = vis->line + 4 - bufwin->view.count.line;

    if(vis->column < bufwin->view.start.column + 4)
        new_col = vis->column - 4;
    else if(vis->column > bufwin->view.count.column - 4 + bufwin->view.start.column)
        new_col = vis->column + 4 - bufwin->view.count.column;

    bufwin_set_view(bufwin, new_line, new_col);
}

void bufwin_update_view(BufferWin* bufwin)
{
    GEM_ASSERT(bufwin != NULL);
    uint32_t vert_adv = get_vert_advance();
    uint32_t hori_adv = gem_get_font()->advance;
    GemQuad* bb = &bufwin->frame.bounding_box;
    GemPadding* tp = &bufwin->text_padding;
    PieceTree* pt = &buffer_get(bufwin->bufnr)->contents;

    uint32_t line_num_width = pt->line_cnt < 1000 ? 4 : (log10((double)pt->line_cnt) + 2);
    bufwin->line_num_bb = make_quad(bb->bl.x,
                                    bb->bl.y,
                                    bb->bl.x + hori_adv * line_num_width + hori_adv / 2,
                                    bb->tr.y);

    bufwin->contents_bb = make_quad(bufwin->line_num_bb.tr.x + tp->left,
                                    bb->bl.y - tp->bottom,
                                    bb->tr.x - tp->right,
                                    bb->tr.y + tp->top);

    int vert_sz = bufwin->contents_bb.bl.y - bufwin->contents_bb.tr.y;
    int hori_sz = bufwin->contents_bb.tr.x - bufwin->contents_bb.bl.x;

    bufwin->view.count.line   = vert_sz / vert_adv + 1;
    bufwin->view.count.column = hori_sz / hori_adv + 1;
}


void bufwin_render_all(void)
{
    GEM_ASSERT(s_CurWin != NULL);
    render_frame(s_RootFrame);
}


void bufwin_update_screen(int width, int height)
{
    GemQuad full_screen = make_quad(0, height, width, 0);
    update_frame(s_RootFrame, &full_screen);
}

void bufwin_key_press(uint16_t keycode, uint32_t mods)
{
    BufNr bufnr = s_CurWin->bufnr;
    PieceTree* pt = &buffer_get(bufnr)->contents; 
    static const char SHIFT_CONVERSION[] = 
        " \0\0\0\0\0\0\"\0\0\0\0<_>?)!@#$%^&*(\0:\0+"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0{|}\0\0~ABCDEFGHIJKLMNOPQR"
        "STUVWXYZ";
    if(mods & GEM_MOD_CONTROL)
    {
        if(keycode == GEM_KEY_C)
        {
            bufwin_print_cursor_loc(s_CurWin);
            printf("\n");
            bufwin_print_view(s_CurWin);
        }
        else if(keycode == GEM_KEY_T)
        {
            piece_tree_print_tree(pt);
        }
        else if(keycode == GEM_KEY_S)
        {
            save_buffer(bufnr);
        }
        else if(keycode == GEM_KEY_O)
        {
            s_CurWin->mode = s_CurWin->mode == WIN_MODE_NORMAL ? WIN_MODE_FILEMAN : WIN_MODE_NORMAL;
            gem_request_redraw();
        }
        else if(keycode == GEM_KEY_D && mods & GEM_MOD_SHIFT && pt->size > 0)
        {
            Cursor* c = &s_CurWin->cursor;
            size_t start = c->offset - c->pos.column;
            size_t count = piece_tree_get_line_length(pt, c->pos.line) + 1;
            bool go_down = false;
            if(c->pos.line == (int64_t)pt->line_cnt - 1)
            {
                if(c->pos.line == 0)
                    count--;
                else
                {
                    start--;
                    go_down = true;
                }
            }
            piece_tree_delete(pt, start, count);
            if(go_down)
                bufwin_move_cursor_line(s_CurWin, -1);
            else
                bufwin_cursor_refresh(s_CurWin);
            gem_request_redraw();
        }
        return;
    }

    if(keycode >= GEM_KEY_SPACE && keycode <= GEM_KEY_Z)
    {
        char c = mods & GEM_MOD_SHIFT ? 
                    SHIFT_CONVERSION[keycode - GEM_KEY_SPACE] : 
                    (char)keycode;
        buffer_insert(bufnr, &c, 1, s_CurWin->cursor.offset);
        bufwin_move_cursor_horiz(s_CurWin, 1);
        gem_request_redraw();
    }
    else if(keycode == GEM_KEY_ENTER)
    {
        char c = '\n';
        buffer_insert(bufnr, &c, 1, s_CurWin->cursor.offset);
        bufwin_set_cursor(s_CurWin, s_CurWin->cursor.pos.line + 1, 0);
        gem_request_redraw();
    }
    else if(keycode == GEM_KEY_TAB)
    {
        char c = ' ';
        int count = 4 - s_CurWin->cursor.vis.column % 4;
        buffer_insert_repeat(bufnr, &c, 1, count, s_CurWin->cursor.offset);
        bufwin_move_cursor_horiz(s_CurWin, count);
    }
    else if(keycode == GEM_KEY_BACKSPACE && s_CurWin->cursor.offset > 0)
    {
        int64_t max = (s_CurWin->cursor.vis.column + 3) % 4 + 1;
        int64_t cnt = 0;
        size_t node_start;
        const PTNode* node = piece_tree_node_at(pt, s_CurWin->cursor.offset - 1, &node_start);
        node_start = s_CurWin->cursor.offset - 1 - node_start;
        while(node != NULL && max > 0)
        {
            const char* buf = piece_tree_get_node_start(pt, node);
            for(size_t i = 0; i <= node_start && max > 0; ++i)
            {
                if(buf[node_start - i] == ' ')
                {
                    cnt++;
                    max--;
                }
                else
                    max = 0;
            }
            node = piece_tree_prev_inorder(pt, node);
            if(node != NULL)
                node_start = node->length - 1;
        }
        if(cnt == 0)
            cnt++;

        buffer_delete(bufnr, s_CurWin->cursor.offset - cnt, cnt);
        bufwin_move_cursor_horiz(s_CurWin, -cnt);
    }
    else if(keycode == GEM_KEY_RIGHT)
        bufwin_move_cursor_horiz(s_CurWin, 1);
    else if(keycode == GEM_KEY_LEFT)
        bufwin_move_cursor_horiz(s_CurWin, -1);
    else if(keycode == GEM_KEY_DOWN)
        bufwin_move_cursor_line(s_CurWin, 1);
    else if(keycode == GEM_KEY_UP)
        bufwin_move_cursor_line(s_CurWin, -1);
}

void bufwin_mouse_press(uint32_t button, uint32_t mods, int sequence, int x, int y)
{
    (void)mods;
    (void)sequence;
    if(button == GEM_MOUSE_SCROLL_DOWN)
        bufwin_move_view(s_CurWin, 3);
    else if(button == GEM_MOUSE_SCROLL_UP)
        bufwin_move_view(s_CurWin, -3);
    else if(button == GEM_MOUSE_BUTTON_LEFT)
    {
        if(quad_contains(&s_CurWin->frame.bounding_box, x, y))
        {
            if(sequence == 1) // Single click
            {

            }
        }
        else
        {
            WinFrame* frame = s_RootFrame;
            while(frame->type != FRAME_TYPE_LEAF)
            {
                if(quad_contains(&frame->left->bounding_box, x, y))
                    frame = frame->left;
                else
                    frame = frame->right;
            }
            s_CurWin = frame_win(frame);
            gem_request_redraw();
        }
        
    }
}

void bufwin_print_cursor_loc(const BufferWin* bufwin)
{
    GEM_ASSERT(bufwin != NULL);
    const Cursor* c = &bufwin->cursor;
    printf("Cursor Location:\n");
    printf("  Actual: %lu,%lu\n", c->pos.line, c->pos.column);
    printf("  Visual: %lu,%lu\n", c->vis.line, c->vis.column);
    printf("  Offset: %lu\n", c->offset);
}

void bufwin_print_view(const BufferWin* bufwin)
{
    GEM_ASSERT(bufwin != NULL);
    const View* v = &bufwin->view;
    printf("View Information:\n");
    printf("  Start: %lu,%lu\n", v->start.line, v->start.column);
    printf("  Count: %lu lines, %lu columns\n", v->count.line, v->count.column);
}

static void render_frame(WinFrame* frame)
{
    GEM_ASSERT(frame != NULL);
    if(!frame->visible)
        return;
    if(frame->type == FRAME_TYPE_LEAF)
    {
        BufferWin* win = frame_win(frame);
        renderer_draw_bufwin(win, win == s_CurWin);
    }
    else
    {
        render_frame(frame->left);
        render_frame(frame->right);
    }
}

static void update_frame(WinFrame* frame, GemQuad* cur)
{
    if(cur == NULL)
    {
        frame->visible = false;
        memset(&frame->bounding_box, 0, sizeof(GemQuad));
        if(frame->type != FRAME_TYPE_LEAF)
        {
            update_frame(frame->left, NULL);
            update_frame(frame->right, NULL);
        }
        else if(s_CurWin == frame_win(frame))
            s_CurWin = frame_win(left_test(s_RootFrame));
        return;
    }
    frame->visible = true;
    frame->bounding_box = *cur;
    if(frame->type == FRAME_TYPE_LEAF)
    {
        bufwin_update_view(frame_win(frame));
        return;
    }

    GemQuad next;
    bool    too_small;
    memset(&next, 0, sizeof(GemQuad));
    next.bl.x = cur->bl.x;
    next.tr.y = cur->tr.y;
    if(frame->type == FRAME_TYPE_HSPLIT)
    {
        next.tr.x = cur->tr.x;
        next.bl.y = (cur->bl.y + cur->tr.y) / 2;
        too_small = next.bl.y - next.tr.y < MIN_WIN_HEIGHT;
    }
    else
    {
        next.tr.x = (cur->bl.x + cur->tr.x) / 2;
        next.bl.y = cur->bl.y;
        too_small = next.tr.x - next.bl.x < MIN_WIN_WIDTH;
    }

    if(too_small)
    {
        update_frame(frame->left, cur);
        update_frame(frame->right, NULL);
        return;
    }
    update_frame(frame->left, &next);
    if(frame->type == FRAME_TYPE_HSPLIT)
    {
        next.tr.y = next.bl.y;
        next.bl.y = cur->bl.y;
    }
    else
    {
        next.bl.x = next.tr.x;
        next.tr.x = cur->tr.x;
    }
    update_frame(frame->right, &next);
}

static BufferPos actual_to_vis(const BufferWin* bufwin, BufferPos actual)
{
    const PieceTree* pt = &buffer_get(bufwin->bufnr)->contents;
    BufferPos res;
    res.line = actual.line; // If I add line wrapping this can change
    res.column = 0;
    size_t start;
    const PTNode* node = piece_tree_node_at_line(pt, actual.line, &start);
    int64_t cur_off = 0;
    while(node != NULL && cur_off < actual.column)
    {
        const char* bufwin = piece_tree_get_node_start(pt, node);
        for(size_t i = start; cur_off < actual.column && i < node->length; ++i)
        {
            if(bufwin[i] == '\t')
                res.column += 4 - res.column % 4;
            else if(bufwin[i] == '\n')
                return res;
            else
                res.column++;

            cur_off++;
        }
        start = 0;
        node = piece_tree_next_inorder(pt, node);
    }

    return res;

}

static BufferPos vis_to_actual(const BufferWin* bufwin, BufferPos vis)
{
    const PieceTree* pt = &buffer_get(bufwin->bufnr)->contents;
    BufferPos res;
    res.line = vis.line; // If we add line wrapping this can change
    res.column = 0;
    size_t start;
    const PTNode* node = piece_tree_node_at_line(pt, vis.line, &start);
    int64_t cur_vis = 0;
    while(node != NULL && cur_vis < vis.column)
    {
        const char* bufwin = piece_tree_get_node_start(pt, node);
        for(size_t i = start; cur_vis < vis.column && i < node->length; ++i)
        {
            if(bufwin[i] == '\t')
                cur_vis += 4 - cur_vis % 4;
            else if(bufwin[i] == '\n')
                return res;
            else
                cur_vis++;

            res.column++;
        }
        start = 0;
        node = piece_tree_next_inorder(pt, node);
    }

    return res;
}

static void clamp_val(int64_t* val, int64_t min, int64_t max)
{
    GEM_ASSERT(min <= max);
    if(min == max || *val < min)
        *val = min;
    else if(*val > max)
        *val = max;
}

static WinFrame* left_test(WinFrame* start)
{
    while(start->type != FRAME_TYPE_LEAF)
        start = start->left;
    return start;
}
