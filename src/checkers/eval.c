// =============================================
// File: src/checkers/eval.c
// Project: dama
// Purpose: Static evaluation
// License: MIT (c) 2025
// =============================================
#include "checkers/eval.h"
#include "core/bitops.h"

#define VAL_MAN  100
#define VAL_KING 300

// ----------------------------------------------------------------
// Piece-square tables for men
// White advances toward rank 7 (higher index = closer to promotion).
// Values indexed by rank (0-7).
// White promotes at rank 7, black at rank 0.
// ----------------------------------------------------------------
static const int MAN_ADV[8] = { 0, 4, 7, 10, 13, 16, 20, 0 };

// Center-control bonus: files 2-5, ranks 2-5 (0-indexed)
static int center_bonus(int sq) {
    int r = sq >> 3, f = sq & 7;
    if (r >= 3 && r <= 4 && f >= 2 && f <= 5) return 6;
    if (r >= 2 && r <= 5 && f >= 3 && f <= 4) return 3;
    return 0;
}

int dama_eval_material(const dama_board_t *b) {
    int stm = b->side_to_move, opp = stm ^ 1;
    int score = 0;
    score += VAL_MAN  * bo_popcount64(b->bb[stm][DAMA_MAN]);
    score += VAL_KING * bo_popcount64(b->bb[stm][DAMA_KING]);
    score -= VAL_MAN  * bo_popcount64(b->bb[opp][DAMA_MAN]);
    score -= VAL_KING * bo_popcount64(b->bb[opp][DAMA_KING]);
    return score;
}

int dama_eval(const dama_board_t *b) {
    int stm = b->side_to_move, opp = stm ^ 1;
    int score = dama_eval_material(b);

    // Advancement PST for men
    uint64_t wm = b->bb[DAMA_WHITE][DAMA_MAN];
    uint64_t bm = b->bb[DAMA_BLACK][DAMA_MAN];
    while (wm) {
        int sq = bo_extract_lsb_index(&wm);
        int adv = MAN_ADV[sq >> 3] + center_bonus(sq);
        if (stm == DAMA_WHITE) score += adv; else score -= adv;
    }
    while (bm) {
        int sq = bo_extract_lsb_index(&bm);
        // Black advances toward rank 0, so advancement = MAN_ADV[7 - rank]
        int adv = MAN_ADV[7 - (sq >> 3)] + center_bonus(sq);
        if (stm == DAMA_BLACK) score += adv; else score -= adv;
    }

    // Back-rank protection: bonus for pieces on own back rank
    uint64_t wk_back = b->bb[DAMA_WHITE][DAMA_MAN] & 0xFFULL;       // rank 0
    uint64_t bk_back = b->bb[DAMA_BLACK][DAMA_MAN] & (0xFFULL << 56); // rank 7
    int back_bonus = 8 * bo_popcount64(stm == DAMA_WHITE ? wk_back : bk_back)
                   - 8 * bo_popcount64(stm == DAMA_WHITE ? bk_back : wk_back);
    score += back_bonus;

    return score;
}
