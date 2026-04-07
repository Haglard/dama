// =============================================
// File: src/checkers/zobrist.c
// Project: dama
// Purpose: Zobrist hashing — TO BE IMPLEMENTED
// License: MIT (c) 2025
// =============================================
#include "checkers/zobrist.h"
#include "core/bitops.h"

void dama_zobrist_init(dama_zobrist_t *z, rng_t *rng) {
    for (int c = 0; c < 2; c++)
        for (int p = 0; p < DAMA_PIECE_N; p++)
            for (int sq = 0; sq < 64; sq++)
                z->piece[c][p][sq] = rng_next_u64(rng);
    z->side_to_move = rng_next_u64(rng);
}

uint64_t dama_zobrist_hash(const dama_zobrist_t *z, const dama_board_t *b) {
    uint64_t h = 0;
    for (int c = 0; c < 2; c++) {
        for (int p = 0; p < DAMA_PIECE_N; p++) {
            uint64_t bb = b->bb[c][p];
            while (bb) {
                int sq = bo_lsb_index(bb);
                bb &= bb - 1;
                h ^= z->piece[c][p][sq];
            }
        }
    }
    if (b->side_to_move == DAMA_BLACK) h ^= z->side_to_move;
    return h;
}
