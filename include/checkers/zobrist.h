// =============================================
// File: include/checkers/zobrist.h
// Project: dama
// Purpose: Zobrist hashing for checkers positions
// License: MIT (c) 2025
// =============================================
#pragma once
#include <stdint.h>
#include "checkers/board.h"
#include "core/rng.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t piece[2][DAMA_PIECE_N][64]; // [color][type][square]
    uint64_t side_to_move;               // XOR when it's black's turn
} dama_zobrist_t;

void     dama_zobrist_init(dama_zobrist_t *z, rng_t *rng);
uint64_t dama_zobrist_hash(const dama_zobrist_t *z, const dama_board_t *b);

#ifdef __cplusplus
}
#endif
