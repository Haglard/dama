// =============================================
// File: include/checkers/movegen.h
// Project: dama
// Purpose: Legal move generation (Italian rules)
// License: MIT (c) 2025
//
// Italian rules summary
// ----------------------
// - Captures are mandatory (must capture if possible).
// - When multiple captures exist, choose the one with the
//   most pieces; if still tied, prefer using a king as
//   attacker; if still tied, prefer capturing more kings.
// - A man reaching the last rank is immediately promoted to
//   king (and stops — no continuation of the jump as king).
// - Kings move/capture diagonally any distance (flying king).
// - Captured pieces are removed immediately after each jump.
//
// Move encoding (game_move_t = uint64_t)
// ----------------------------------------
//   bits  0- 5  from square        (0..63)
//   bits  6-11  to square          (0..63)
//   bits 12-15  num_caps           (0..15, usually 0..7)
//   bit     16  is_promo           (1 if man promoted)
//   bit     17  is_king_attacker   (1 if mover is/was a king)
//   bits 18-21  num_kings_cap      (kings captured in sequence)
//   bits 22-27  cap[0] square      (first captured square)
//   bits 28-33  cap[1] square
//   bits 34-39  cap[2] square
//   bits 40-45  cap[3] square
//   bits 46-51  cap[4] square
//   bits 52-57  cap[5] square
//   bits 58-63  cap[6] square      (seventh captured square)
// =============================================
#pragma once
#include "checkers/board.h"
#include "game/api.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---- Move list ----
#define DAMA_MAX_MOVES 128
#define DAMA_MAX_CAPS    7   // max captures in one sequence

typedef struct {
    game_move_t data[DAMA_MAX_MOVES];
    int         n;
} dama_move_list_t;

// ---- Move encoding helpers ----
static inline int dama_move_from(game_move_t m)         { return (int)( m        & 63); }
static inline int dama_move_to(game_move_t m)           { return (int)((m >>  6) & 63); }
static inline int dama_move_num_caps(game_move_t m)     { return (int)((m >> 12) & 15); }
static inline int dama_move_is_cap(game_move_t m)       { return ((m >> 12) & 15) > 0;  }
static inline int dama_move_is_promo(game_move_t m)     { return (int)((m >> 16) &  1); }
static inline int dama_move_is_king(game_move_t m)      { return (int)((m >> 17) &  1); }
static inline int dama_move_num_kings(game_move_t m)    { return (int)((m >> 18) & 15); }
// Get the i-th captured square (i = 0..num_caps-1)
static inline int dama_move_cap_sq(game_move_t m, int i){ return (int)((m >> (22 + i*6)) & 63); }

// Build a simple (non-capture) move
static inline game_move_t dama_move_quiet(int from, int to, int is_promo, int is_king) {
    return (game_move_t)(from & 63)
         | ((game_move_t)(to & 63) << 6)
         | ((game_move_t)(is_promo ? 1 : 0) << 16)
         | ((game_move_t)(is_king  ? 1 : 0) << 17);
}

// ---- Generation ----
// Returns number of legal moves written into ml.
// Applies mandatory-capture and Italian priority rules.
int dama_generate_legal(const dama_board_t *b, dama_move_list_t *ml);

// Capture-only subset (for quiescence search).
int dama_generate_captures(const dama_board_t *b, dama_move_list_t *ml);

// ---- Undo record ----
typedef struct {
    int cap_sqs[DAMA_MAX_CAPS];    // captured squares in sequence order
    int cap_types[DAMA_MAX_CAPS];  // DAMA_MAN or DAMA_KING for each
    int n_caps;
    int was_promo;    // 1 if this move promoted the mover
    int mover_type;   // original piece type of mover (before any promotion)
    int old_halfmove;
    int old_fullmove;
} dama_undo_t;

// ---- Execution ----
void dama_make_move(dama_board_t *b, game_move_t mv, dama_undo_t *u);
void dama_unmake_move(dama_board_t *b, game_move_t mv, const dama_undo_t *u);

#ifdef __cplusplus
}
#endif
