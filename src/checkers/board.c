// =============================================
// File: src/checkers/board.c
// Project: dama
// Purpose: Board representation — TO BE IMPLEMENTED
// License: MIT (c) 2025
// =============================================
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "checkers/board.h"
#include "core/log.h"

// TODO: implement all functions declared in board.h

void dama_board_clear(dama_board_t *b) {
    memset(b, 0, sizeof(*b));
    b->side_to_move   = DAMA_WHITE;
    b->fullmove_number = 1;
}

void dama_board_recompute_occupancy(dama_board_t *b) {
    b->occ[DAMA_WHITE] = b->bb[DAMA_WHITE][DAMA_MAN] | b->bb[DAMA_WHITE][DAMA_KING];
    b->occ[DAMA_BLACK] = b->bb[DAMA_BLACK][DAMA_MAN] | b->bb[DAMA_BLACK][DAMA_KING];
    b->occ_all         = b->occ[DAMA_WHITE] | b->occ[DAMA_BLACK];
}

void dama_board_place(dama_board_t *b, int color, int piece, int sq) {
    uint64_t bit = 1ULL << sq;
    b->bb[color][piece] |= bit;
    b->occ[color]       |= bit;
    b->occ_all          |= bit;
}

void dama_board_remove(dama_board_t *b, int color, int piece, int sq) {
    uint64_t bit = 1ULL << sq;
    b->bb[color][piece] &= ~bit;
    b->occ[color]       &= ~bit;
    b->occ_all          &= ~bit;
}

void dama_board_set_startpos(dama_board_t *b) {
    // TODO: place pieces in standard Italian checkers starting position
    dama_board_clear(b);
    LOGW("dama_board_set_startpos: NOT YET IMPLEMENTED");
}

int dama_board_from_str(dama_board_t *b, const char *s) {
    // TODO: parse position string (e.g. "W:Wa1,c1:Bb6,d7")
    (void)s;
    dama_board_clear(b);
    LOGW("dama_board_from_str: NOT YET IMPLEMENTED");
    return -1;
}

int dama_board_to_str(const dama_board_t *b, char *out, size_t out_sz) {
    // TODO: serialize position to string
    (void)b;
    if (out && out_sz > 0) out[0] = '\0';
    LOGW("dama_board_to_str: NOT YET IMPLEMENTED");
    return -1;
}
