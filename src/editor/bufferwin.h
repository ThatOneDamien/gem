#pragma once
#include "buffer.h"
#include "structs/piecetree.h"
#include "structs/quad.h"

typedef struct Cursor Cursor;
struct Cursor
{
    BufferPos pos;
    BufferPos vis;
    int64_t   horiz;
    size_t    offset;
};

typedef struct View View;
struct View
{
    BufferPos start;
    BufferPos count;
};

typedef struct WinFrame WinFrame;
typedef struct BufferWin BufferWin;
struct BufferWin
{
    Cursor      cursor; // May extend to multiple cursors later
    View        view;
    GemPadding  text_padding;
    WinFrame*   frame;
    int         bufnr; 
    bool        visible;
};

void bufwin_init_root_frame(void);
void bufwin_open(char* filepath);
void bufwin_open_bufnr(BufNr bufnr);
void bufwin_close(void);

BufferWin* bufwin_split(bool vsplit);
BufNr      bufwin_get_bufnr(void);
void       bufwin_set_current(BufferWin* bufwin);

void bufwin_set_cursor(BufferWin* bufwin, int64_t line, int64_t column);
void bufwin_move_cursor_line(BufferWin* bufwin, int64_t line_delta);
void bufwin_move_cursor_horiz(BufferWin* bufwin, int64_t horiz_delta);
void bufwin_cursor_refresh(BufferWin* bufwin);

void bufwin_set_view(BufferWin* bufwin, int64_t start_line, int64_t start_col);
void bufwin_move_view(BufferWin* bufwin, int64_t line_delta);
void bufwin_put_cursor_in_view(BufferWin* bufwin);
void bufwin_update_view(BufferWin* bufwin);

void bufwin_render_all(void);
void bufwin_update_screen(int width, int height);

void bufwin_key_press(uint16_t keycode, uint32_t mods);
void bufwin_mouse_press(uint32_t button, uint32_t mods, int x, int y);

void bufwin_print_cursor_loc(const BufferWin* bufwin);
void bufwin_print_view(const BufferWin* bufwin);

const GemQuad* bufwin_get_bb(const BufferWin* bufwin);

static inline void bufwin_set_cursor_bp(BufferWin* bufwin, BufferPos pos)
{ 
    bufwin_set_cursor(bufwin, pos.line, pos.column);
}
