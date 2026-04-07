// =============================================
// File: include/checkers/eval.h
// Project: dama
// Purpose: Static position evaluation
// License: MIT (c) 2025
//
// Returns score in centipawns from side-to-move's
// point of view (positive = better for side-to-move).
//
// Evaluation components:
//   - Material: man=100, king=300
//   - Piece-square tables (advancement, center control)
//   - King safety / back-rank bonus
//   - Mobility (number of legal moves)
// =============================================
#pragma once
#include "checkers/board.h"

#ifdef __cplusplus
extern "C" {
#endif

// Full static evaluation (side-to-move relative).
int dama_eval(const dama_board_t *b);

// Material-only (fast, used for MVV ordering).
int dama_eval_material(const dama_board_t *b);

#ifdef __cplusplus
}
#endif
