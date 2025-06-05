#pragma once
#include "buffer.h"
#include "structs/da.h"
#include "structs/piecetree.h"
#include "structs/quad.h"

#include <limits.h>
#include <sys/stat.h>
#ifndef NAME_MAX
    #define NAME_MAX 255
#endif

typedef struct Cursor    Cursor;
typedef struct View      View;
typedef struct WinFrame  WinFrame;
typedef struct BufferWin BufferWin;
typedef struct DirEntry  DirEntry;
typedef struct EntryDA   EntryDA;

struct Cursor
{
    BufferPos pos;
    BufferPos vis;
    int64_t   horiz;
    size_t    offset;
};

struct View
{
    BufferPos start;
    BufferPos count;
};

enum
{
    FRAME_TYPE_LEAF = 0,
    FRAME_TYPE_VSPLIT,
    FRAME_TYPE_HSPLIT
};

enum
{
    WIN_MODE_NORMAL = 0,
    WIN_MODE_FILEMAN,
};

struct WinFrame
{
    WinFrame*  parent;
    WinFrame*  left;
    WinFrame*  right;
    GemQuad    bounding_box;
    uint8_t    type;
    bool       visible;
};

struct DirEntry
{
    struct stat stats;
    char        name[NAME_MAX + 1]; // +1 for null termination
};

struct EntryDA
{
    DirEntry* data;
    size_t    size;
    size_t    capacity;
    int       largest_name;
};


struct BufferWin
{
    Cursor      cursor; // May extend to multiple cursors later
    View        view;

    WinFrame    frame;
    GemQuad     line_num_bb;
    GemQuad     contents_bb;
    GemPadding  text_padding;

    char*       local_dir;
    EntryDA     dir_entries;
    size_t      sel_entry;

    int         bufnr; 
    uint8_t     mode;
};

#define frame_win(f) ((BufferWin*)((uintptr_t)(f) - offsetof(BufferWin, frame)))

extern BufferWin* g_cur_win;

void bufwin_init_root_frame(void);
void bufwin_open(char* filepath);
void bufwin_close(void);

BufferWin* bufwin_copy(BufferWin* bufwin);
BufferWin* bufwin_split(bool vsplit);

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
void bufwin_mouse_press(uint32_t button, uint32_t mods, int sequence, int x, int y);

void bufwin_print_cursor_loc(const BufferWin* bufwin);
void bufwin_print_view(const BufferWin* bufwin);

static inline void bufwin_set_cursor_bp(BufferWin* bufwin, BufferPos pos)
{ 
    bufwin_set_cursor(bufwin, pos.line, pos.column);
}
