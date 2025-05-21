#include "textbuffer.h"
#include "core/app.h"
#include "core/core.h"
#include "fileman/fileio.h"
#include "render/font.h"
#include "render/renderer.h"

#include <string.h>

#define MIN(x, y) ((x) > (y) ? (y) : (x))

static BufferPos actual_to_vis(const TextBuffer* buf, BufferPos actual);
static BufferPos vis_to_actual(const TextBuffer* buf, BufferPos vis);

void text_buffer_open_empty(TextBuffer* buf)
{
    GEM_ASSERT(buf != NULL);

    memset(buf, 0, sizeof(TextBuffer));
    piece_tree_init(&buf->contents, NULL, 0, false);
}

void text_buffer_open_file(TextBuffer* buf, const char* filepath)
{
    GEM_ASSERT(buf != NULL);
    GEM_ASSERT(filepath != NULL);

    memset(buf, 0, sizeof(TextBuffer));

    char* contents;
    size_t size;
    if(!gem_read_entire_file(filepath, &contents, &size))
    {
        fprintf(stderr, "Failed to find file: %s\n", filepath);
        piece_tree_init(&buf->contents, NULL, 0, false);
    }
    else
    {
        if(contents[size - 1] == '\n')
            size--;
        piece_tree_init(&buf->contents, contents, size, false);
        buf->filepath = filepath;
    }
}

static void clamp_val(int64_t* val, int64_t min, int64_t max)
{
    GEM_ASSERT(min <= max);
    if(min == max || *val < min)
        *val = min;
    else if(*val > max)
        *val = max;
}

void text_buffer_close(TextBuffer* buf)
{
    GEM_ASSERT(buf != NULL);
    piece_tree_free(&buf->contents);
}

void text_buffer_update_view(TextBuffer* buf)
{
    GEM_ASSERT(buf != NULL);
    uint32_t adv = gem_get_font()->advance;
    int vert_sz = buf->bounding_box.bl.y - buf->bounding_box.tr.y -
                  buf->text_padding.bottom - buf->text_padding.top;
    int hori_sz = buf->bounding_box.tr.x - buf->bounding_box.bl.x -
                  buf->text_padding.left - buf->text_padding.right - 
                  adv * 5 + (adv >> 2);
    buf->view.count.line = (int64_t)(vert_sz / gem_get_vert_advance()) - 1;
    buf->view.count.column = (int64_t)(hori_sz / adv) - 1;
}

void text_buffer_cursor_refresh(TextBuffer* buf)
{
    GEM_ASSERT(buf != NULL);
    Cursor* c = &buf->cursor;
    c->pos = vis_to_actual(buf, c->vis);
    c->offset = piece_tree_get_offset_bp(&buf->contents, c->pos);
}

void text_buffer_set_view(TextBuffer* buf, int64_t start_line, int64_t start_col)
{
    (void)start_col;
    GEM_ASSERT(buf != NULL);
    clamp_val(&start_line, 0, buf->contents.line_cnt - 1);
    if(start_line != buf->view.start.line)
    {
        buf->view.start.line = start_line;
        if(buf->visible)
            gem_app_request_redraw();
    }
}

void text_buffer_set_cursor(TextBuffer* buf, int64_t line, int64_t column)
{
    GEM_ASSERT(buf != NULL);
    
    Cursor* c = &buf->cursor;
    PieceTree* pt = &buf->contents;
    size_t line_len;

    clamp_val(&line, 0, buf->contents.line_cnt - 1);
    line_len = piece_tree_get_line_length(pt, line);
    clamp_val(&column, 0, line_len);
    if(line == c->pos.line && column == c->pos.column)
        return;

    c->pos.line = line;
    c->pos.column = column;
    c->vis = actual_to_vis(buf, c->pos);
    c->offset = piece_tree_get_offset_bp(&buf->contents, c->pos);
}

void text_buffer_move_cursor_line(TextBuffer* buf, int64_t line_delta)
{
    GEM_ASSERT(buf != NULL);
    if(line_delta == 0)
        return;

    PieceTree* pt = &buf->contents;
    Cursor* c = &buf->cursor;
    clamp_val(&line_delta, -c->pos.line, pt->line_cnt - 1 - c->pos.line);
    if(line_delta != 0)
    {
        c->vis.line += line_delta;
        c->pos = vis_to_actual(buf, c->vis);
        c->offset = piece_tree_get_offset_bp(pt, c->pos);
        text_buffer_put_cursor_in_view(buf);
        if(buf->visible)
            gem_app_request_redraw();
    }
}

void text_buffer_move_cursor_horiz(TextBuffer* buf, int64_t horiz_delta)
{
    GEM_ASSERT(buf != NULL);
    if(horiz_delta == 0)
        return;

    PieceTree* pt = &buf->contents;
    Cursor* c = &buf->cursor;
    clamp_val(&horiz_delta, -c->offset, pt->size - c->offset);
    c->offset += horiz_delta;
    if(horiz_delta != 0)
    {
        c->pos = piece_tree_get_buffer_pos(pt, c->offset);
        c->vis = actual_to_vis(buf, c->pos);
        text_buffer_put_cursor_in_view(buf);
        if(buf->visible)
            gem_app_request_redraw();
    }
}

void text_buffer_move_view(TextBuffer* buf, int64_t line_delta)
{
    GEM_ASSERT(buf != NULL);
    if(line_delta == 0)
        return;

    text_buffer_set_view(buf, buf->view.start.line + line_delta, 0);
}

void text_buffer_print_cursor_loc(const TextBuffer* buf)
{
    GEM_ASSERT(buf != NULL);
    const Cursor* c = &buf->cursor;
    printf("Cursor Location:\n");
    printf("  Actual: %lu,%lu\n", c->pos.line, c->pos.column);
    printf("  Visual: %lu,%lu\n", c->vis.line, c->vis.column);
    printf("  Offset: %lu\n", c->offset);
}

void text_buffer_put_cursor_in_view(TextBuffer* buf)
{
    GEM_ASSERT(buf != NULL);
    BufferPos* vis = &buf->cursor.vis;
    if(vis->line < buf->view.start.line + 4) 
    {
        text_buffer_set_view(buf, vis->line - 4, 0); // TODO: Change hardcoded 4?
        return;
    }

    if(vis->line > buf->view.count.line - 4 + buf->view.start.line)
    {
        text_buffer_set_view(buf, vis->line + 4 - buf->view.count.line, 0);
    }

}

static BufferPos actual_to_vis(const TextBuffer* buf, BufferPos actual)
{
    const PieceTree* pt = &buf->contents;
    BufferPos res;
    res.line = actual.line; // If I add line wrapping this can change
    res.column = 0;
    size_t start;
    const PTNode* node = piece_tree_node_at_line(pt, actual.line, &start);
    int64_t cur_off = 0;
    while(node != NULL && cur_off < actual.column)
    {
        const char* buf = piece_tree_get_node_start(pt, node);
        for(size_t i = start; cur_off < actual.column && i < node->length; ++i)
        {
            if(buf[i] == '\t')
                res.column += 4 - res.column % 4;
            else if(buf[i] == '\n')
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

static BufferPos vis_to_actual(const TextBuffer* buf, BufferPos vis)
{
    const PieceTree* pt = &buf->contents;
    BufferPos res;
    res.line = vis.line; // If we add line wrapping this can change
    res.column = 0;
    size_t start;
    const PTNode* node = piece_tree_node_at_line(pt, vis.line, &start);
    int64_t cur_vis = 0;
    while(node != NULL && cur_vis < vis.column)
    {
        const char* buf = piece_tree_get_node_start(pt, node);
        for(size_t i = start; cur_vis < vis.column && i < node->length; ++i)
        {
            if(buf[i] == '\t')
                cur_vis += 4 - cur_vis % 4;
            else if(buf[i] == '\n')
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
