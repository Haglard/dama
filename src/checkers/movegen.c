// =============================================
// File: src/checkers/movegen.c
// Project: dama
// Purpose: Legal move generation — TO BE IMPLEMENTED
// License: MIT (c) 2025
// =============================================
#include "checkers/movegen.h"
#include "core/log.h"

// TODO: implement Italian-rules move generation with:
//  - mandatory capture
//  - maximum-capture rule
//  - flying king (moves/captures diagonally any distance)
//  - promotion stops jump sequence

int dama_generate_legal(const dama_board_t *b, dama_move_list_t *ml) {
    (void)b;
    ml->n = 0;
    LOGW("dama_generate_legal: NOT YET IMPLEMENTED");
    return 0;
}

int dama_generate_captures(const dama_board_t *b, dama_move_list_t *ml) {
    (void)b;
    ml->n = 0;
    LOGW("dama_generate_captures: NOT YET IMPLEMENTED");
    return 0;
}

void dama_make_move(dama_board_t *b, game_move_t mv, dama_undo_t *u) {
    (void)b; (void)mv; (void)u;
    LOGW("dama_make_move: NOT YET IMPLEMENTED");
}

void dama_unmake_move(dama_board_t *b, game_move_t mv, const dama_undo_t *u) {
    (void)b; (void)mv; (void)u;
    LOGW("dama_unmake_move: NOT YET IMPLEMENTED");
}
