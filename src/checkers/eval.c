// =============================================
// File: src/checkers/eval.c
// Project: dama
// Purpose: Static evaluation — TO BE IMPLEMENTED
// License: MIT (c) 2025
// =============================================
#include "checkers/eval.h"
#include "core/bitops.h"

// Piece values (centipawns)
#define VAL_MAN  100
#define VAL_KING 300

int dama_eval_material(const dama_board_t *b) {
    int stm = b->side_to_move;
    int opp = stm ^ 1;
    int score = 0;
    score += VAL_MAN  * bo_popcount64(b->bb[stm][DAMA_MAN]);
    score += VAL_KING * bo_popcount64(b->bb[stm][DAMA_KING]);
    score -= VAL_MAN  * bo_popcount64(b->bb[opp][DAMA_MAN]);
    score -= VAL_KING * bo_popcount64(b->bb[opp][DAMA_KING]);
    return score;
}

int dama_eval(const dama_board_t *b) {
    // TODO: add PST, mobility, back-rank bonus
    return dama_eval_material(b);
}
