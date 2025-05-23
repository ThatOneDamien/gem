#include "bufferwin.h"
#include "core/app.h"
#include "core/core.h"
#include "core/keycode.h"
#include "fileman/fileio.h"
#include "render/font.h"
#include "render/renderer.h"

#include <string.h>

#define MIN(x, y) ((x) > (y) ? (y) : (x))

static BufferPos actual_to_vis(const BufferWin* bufwin, BufferPos actual);
static BufferPos vis_to_actual(const BufferWin* bufwin, BufferPos vis);
static void      clamp_val(int64_t* val, int64_t min, int64_t max);

enum
{
    FRAME_TYPE_LEAF = 0,
    FRAME_TYPE_VSPLIT,
    FRAME_TYPE_HSPLIT
};

typedef struct WinFrame WinFrame;
struct WinFrame
{
    WinFrame*  parent;
    WinFrame*  left;
    WinFrame*  right;
    BufferWin* bufwin;
    int        lnum;
    int        lden;
    int        width;
    int        height;
    uint8_t    type;
};

static BufferWin* s_CurrentWindow;
static WinFrame s_RootFrame;

void bufwin_init(void)
{
    // TODO: Make this system dynamically allocated like
    // the piece tree node storage and the buffer array.
    s_CurrentWindow = malloc(sizeof(BufferWin));
    memset(s_CurrentWindow, 0, sizeof(BufferWin));
    s_CurrentWindow->visible = true;
    s_CurrentWindow->text_padding.left = 10;

    s_RootFrame.parent = NULL;
    s_RootFrame.left = NULL;
    s_RootFrame.right = NULL;
    s_RootFrame.bufwin = s_CurrentWindow;
    s_RootFrame.lnum = 1;
    s_RootFrame.lden = 1;
    s_RootFrame.width = 0;
    s_RootFrame.height = 0;
    s_RootFrame.type = FRAME_TYPE_LEAF;
}

void bufwin_open(const char* filepath)
{
    GEM_ASSERT(s_CurrentWindow != NULL);
    // todo: close previous buffer if one was open.
    s_CurrentWindow->bufnr = filepath == NULL ?
                        buffer_open_empty() :
                        buffer_open_file(filepath);
}

void bufwin_open_bufnr(BufNr bufnr)
{
    GEM_ASSERT(s_CurrentWindow != NULL);
    // todo: close previous buffer if one was open.
    bool success = buffer_reopen(bufnr);
    if(success)
        s_CurrentWindow->bufnr = bufnr;
}

static void render_frame(WinFrame* frame)
{
    GEM_ASSERT(frame != NULL);
    if(frame->type == FRAME_TYPE_LEAF)
        renderer_draw_bufwin(frame->bufwin);
    else
    {
        render_frame(frame->left);
        render_frame(frame->right);
    }
}

void bufwin_render_all(void)
{
    GEM_ASSERT(s_CurrentWindow != NULL);
    render_frame(&s_RootFrame);
}

void bufwin_update_screen(int width, int height)
{
    s_RootFrame.width = width;
    s_RootFrame.height = height;
    if(s_RootFrame.type == FRAME_TYPE_LEAF)
    {
        s_RootFrame.bufwin->bounding_box = make_quad(0, height, width, 0);
        bufwin_update_view(s_CurrentWindow);
    }
}

//
// void bufwin_close(BufferWin* bufwin)
// {
//     GEM_ASSERT(bufwin != NULL);
//     buffer_close(bufwin->bufnr);
// }
//

void bufwin_set_cursor(BufferWin* bufwin, int64_t line, int64_t column)
{
    GEM_ASSERT(bufwin != NULL);
    
    Cursor* c = &bufwin->cursor;
    const PieceTree* pt = buffer_get_pt(bufwin->bufnr);
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

    const PieceTree* pt = buffer_get_pt(bufwin->bufnr);
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
        if(bufwin->visible)
            gem_request_redraw();
    }
}

void bufwin_move_cursor_horiz(BufferWin* bufwin, int64_t horiz_delta)
{
    GEM_ASSERT(bufwin != NULL);
    if(horiz_delta == 0)
        return;

    const PieceTree* pt = buffer_get_pt(bufwin->bufnr);
    Cursor* c = &bufwin->cursor;
    clamp_val(&horiz_delta, -c->offset, pt->size - c->offset);
    c->offset += horiz_delta;
    if(horiz_delta != 0)
    {
        c->pos = piece_tree_get_buffer_pos(pt, c->offset);
        c->vis = actual_to_vis(bufwin, c->pos);
        c->horiz = c->vis.column;
        bufwin_put_cursor_in_view(bufwin);
        if(bufwin->visible)
            gem_request_redraw();
    }
}

void bufwin_cursor_refresh(BufferWin* bufwin)
{
    GEM_ASSERT(bufwin != NULL);
    Cursor* c = &bufwin->cursor;
    const PieceTree* pt = buffer_get_pt(bufwin->bufnr);
    c->pos = vis_to_actual(bufwin, c->vis);
    c->offset = piece_tree_get_offset_bp(pt, c->pos);
}

void bufwin_set_view(BufferWin* bufwin, int64_t start_line, int64_t start_col)
{
    (void)start_col;
    GEM_ASSERT(bufwin != NULL);
    const PieceTree* pt = buffer_get_pt(bufwin->bufnr);
    clamp_val(&start_line, 0, pt->line_cnt - 1);
    if(start_col < 0)
        start_col = 0;
    if(start_line != bufwin->view.start.line || 
       start_col != bufwin->view.start.column)
    {
        bufwin->view.start.line = start_line;
        bufwin->view.start.column = start_col;
        if(bufwin->visible)
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
    uint32_t adv = gem_get_font()->advance;
    int vert_sz = bufwin->bounding_box.bl.y - bufwin->bounding_box.tr.y -
                  bufwin->text_padding.bottom - bufwin->text_padding.top;
    int hori_sz = bufwin->bounding_box.tr.x - bufwin->bounding_box.bl.x -
                  bufwin->text_padding.left - bufwin->text_padding.right - 
                  adv * 5 + (adv >> 2);
    bufwin->view.count.line = vert_sz / get_vert_advance() + 1;
    bufwin->view.count.column = hori_sz / adv + 1;
}

void bufwin_key_press(uint16_t keycode, uint32_t mods)
{
    PieceTree* pt = (PieceTree*)buffer_get_pt(s_CurrentWindow->bufnr);
    static const char SHIFT_CONVERSION[GEM_KEY_Z - GEM_KEY_SPACE + 1] = 
        " \0\0\0\0\0\0\"\0\0\0\0<_>?)!@#$%^&*(\0:\0+"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0{|}\0\0~ABCDEFGHIJKLMNOPQR"
        "STUVWXYZ";
    if(mods & GEM_MOD_CONTROL)
    {
        if(keycode == GEM_KEY_C)
        {
            bufwin_print_cursor_loc(s_CurrentWindow);
            printf("\n");
            bufwin_print_view(s_CurrentWindow);
        }
        else if(keycode == GEM_KEY_T)
        {
            piece_tree_print_tree(pt);
        }
        else if(keycode == GEM_KEY_S)
        {
            if(write_buffer(s_CurrentWindow->bufnr, "temp.txt"))
                printf("Wrote %lu bytes to temp.txt\n", pt->size);
            else
                printf("Failed to write to temp.txt\n");
        }
        else if(keycode == GEM_KEY_D && mods & GEM_MOD_SHIFT && pt->size > 0)
        {
            Cursor* c = &s_CurrentWindow->cursor;
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
                bufwin_move_cursor_line(s_CurrentWindow, -1);
            else
                bufwin_cursor_refresh(s_CurrentWindow);
            gem_request_redraw();
        }
        return;
    }

    if(keycode >= GEM_KEY_SPACE && keycode <= GEM_KEY_Z)
    {
        char c = mods & GEM_MOD_SHIFT ? 
                    SHIFT_CONVERSION[keycode - GEM_KEY_SPACE] : 
                    (char)keycode;
        piece_tree_insert_char(pt, c, s_CurrentWindow->cursor.offset);
        bufwin_move_cursor_horiz(s_CurrentWindow, 1);
        gem_request_redraw();
    }
    else if(keycode == GEM_KEY_ENTER)
    {
        piece_tree_insert_char(pt, '\n', s_CurrentWindow->cursor.offset);
        bufwin_set_cursor(s_CurrentWindow, s_CurrentWindow->cursor.pos.line + 1, 0);
        gem_request_redraw();
    }
    else if(keycode == GEM_KEY_TAB)
    {
        char spaces[4] = "    ";
        int count = 4 - s_CurrentWindow->cursor.vis.column % 4;
        piece_tree_insert(pt, spaces, count, s_CurrentWindow->cursor.offset);
        bufwin_move_cursor_horiz(s_CurrentWindow, count);
    }
    else if(keycode == GEM_KEY_BACKSPACE && s_CurrentWindow->cursor.offset > 0)
    {
        int64_t max = (s_CurrentWindow->cursor.vis.column - 1) % 4 + 1;
        int64_t cnt = 0;
        size_t node_start;
        const PTNode* node = piece_tree_node_at(pt, s_CurrentWindow->cursor.offset - 1, &node_start);
        node_start = s_CurrentWindow->cursor.offset - 1 - node_start;
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

        piece_tree_delete(pt, s_CurrentWindow->cursor.offset - cnt, cnt);
        bufwin_move_cursor_horiz(s_CurrentWindow, -cnt);
    }
    else if(keycode == GEM_KEY_RIGHT)
        bufwin_move_cursor_horiz(s_CurrentWindow, 1);
    else if(keycode == GEM_KEY_LEFT)
        bufwin_move_cursor_horiz(s_CurrentWindow, -1);
    else if(keycode == GEM_KEY_DOWN)
        bufwin_move_cursor_line(s_CurrentWindow, 1);
    else if(keycode == GEM_KEY_UP)
        bufwin_move_cursor_line(s_CurrentWindow, -1);
}

void bufwin_mouse_press(uint32_t button, uint32_t mods, int x, int y)
{
    (void)mods;
    (void)x;
    (void)y;
    if(button == GEM_MOUSE_SCROLL_DOWN)
        bufwin_move_view(s_CurrentWindow, 3);
    else if(button == GEM_MOUSE_SCROLL_UP)
        bufwin_move_view(s_CurrentWindow, -3);
    // else if(button == GEM_MOUSE_BUTTON_LEFT)
    //
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

static BufferPos actual_to_vis(const BufferWin* bufwin, BufferPos actual)
{
    const PieceTree* pt = buffer_get_pt(bufwin->bufnr);
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
    const PieceTree* pt = buffer_get_pt(bufwin->bufnr);
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
