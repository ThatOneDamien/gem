#pragma once
#include "structs/piecetree.h"
#include "structs/quad.h"

typedef int BufNr;

void  buffer_list_init(void);
BufNr buffer_open_empty(void);
BufNr buffer_open_file(char* filepath);
bool  buffer_reopen(BufNr bufnr);
void  buffer_close(BufNr bufnr);
void  close_all_buffers(void);

const PieceTree* buffer_get_pt(BufNr bufnr);
