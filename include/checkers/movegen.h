// =============================================
// File: include/checkers/movegen.h
// Project: dama
// Purpose: Legal move generation (Italian rules)
// License: MIT (c) 2025
//
// Italian rules summary
// ----------------------
// - Captures are mandatory (must capture if possible).
// - When multiple captures exist, choose the one that
//   takes the most pieces; if still tied, prefer capturing
//   kings over men (and kings over men as attacker).
// - A man reaching the last rank is immediately promoted to
//   king (and stops — no continuation of the jump as king).
// - Kings move/capture diagonally any distance (flying king).
// - Captured pieces are removed only after the full sequence.
// =============================================
#pragma once
#include "checkers/board.h"
#include "game/api.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---- Move list ----
#define DAMA_MAX_MOVES 64

typedef struct {
    game_move_t data[DAMA_MAX_MOVES];
    int         n;
} dama_move_list_t;

// ---- Move encoding helpers ----
static inline game_move_t dama_move_make(int from, int to,
                                          int is_cap, int cap_sq,
                                          int is_promo) {
    return (game_move_t)(from & 63)
         | ((game_move_t)(to   & 63) << 6)
         | ((game_move_t)(is_cap  ? 1 : 0) << 12)
         | ((game_move_t)(is_promo? 1 : 0) << 13)
         | ((game_move_t)(cap_sq & 63)     << 14);
}
static inline int dama_move_from(game_move_t m)    { return (int)( m        & 63); }
static inline int dama_move_to(game_move_t m)      { return (int)((m >> 6)  & 63); }
static inline int dama_move_is_cap(game_move_t m)  { return (int)((m >> 12) &  1); }
static inline int dama_move_is_promo(game_move_t m){ return (int)((m >> 13) &  1); }
static inline int dama_move_cap_sq(game_move_t m)  { return (int)((m >> 14) & 63); }

// ---- Generation ----
// Returns number of legal moves written into ml.
// Applies mandatory-capture and maximum-capture rules.
int dama_generate_legal(const dama_board_t *b, dama_move_list_t *ml);

// Capture-only subset (for quiescence search).
int dama_generate_captures(const dama_board_t *b, dama_move_list_t *ml);

// ---- Execution ----
typedef struct {
    int      cap_sq;        // captured square (or -1)
    int      cap_piece;     // piece type that was captured
    int      cap_color;
    int      from_piece;    // original mover piece type (before promo)
    int      old_halfmove;
    int      old_fullmove;
} dama_undo_t;

void dama_make_move(dama_board_t *b, game_move_t mv, dama_undo_t *u);
void dama_unmake_move(dama_board_t *b, game_move_t mv, const dama_undo_t *u);

#ifdef __cplusplus
}
#endif
