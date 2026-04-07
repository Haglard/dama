// =============================================
// File: include/checkers/board.h
// Project: dama (Checkers/Draughts engine)
// Purpose: Board representation and FEN-like I/O
// License: MIT (c) 2025
//
// Board layout — Italian rules (8×8)
// -----------------------------------
// Squares are numbered 1..32 (playable dark squares only),
// but internally we use a 64-bit flat index (0..63, files a..h,
// ranks 1..8) and keep two bitboards per side.
//
// Piece types:
//   DAMA_MAN  = 0   (pedina)
//   DAMA_KING = 1   (dama/re)
//
// Colors:
//   DAMA_WHITE = 0
//   DAMA_BLACK = 1
//
// Move encoding (game_move_t = uint64_t):
//   bits  0.. 5  from square (0..63)
//   bits  6..11  to square   (0..63)
//   bit     12   is_capture
//   bit     13   is_promotion (man reaches last rank)
//   bits 14..19  captured square (0..63, valid when is_capture=1)
//   bits 20..63  reserved for multi-jump chains (future)
// =============================================
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- Constants ----
#define DAMA_WHITE  0
#define DAMA_BLACK  1
#define DAMA_MAN    0
#define DAMA_KING   1
#define DAMA_PIECE_N 2

// ---- Board state ----
typedef struct {
    uint64_t bb[2][DAMA_PIECE_N]; // bb[color][piece_type]
    uint64_t occ[2];              // occupancy by color
    uint64_t occ_all;             // all pieces
    int      side_to_move;        // DAMA_WHITE / DAMA_BLACK
    int      halfmove_clock;      // moves since last capture (for draw detection)
    int      fullmove_number;
} dama_board_t;

// ---- Lifecycle ----
void dama_board_clear(dama_board_t *b);
void dama_board_set_startpos(dama_board_t *b);

// ---- Piece placement ----
void dama_board_place(dama_board_t *b, int color, int piece, int sq);
void dama_board_remove(dama_board_t *b, int color, int piece, int sq);
void dama_board_recompute_occupancy(dama_board_t *b);

// ---- Serialization (simple positional string, not standard FEN) ----
// Format: "W:Wf1,f3,...:Bb6,b8,..." — compatible with Scan/KingsRow
int  dama_board_from_str(dama_board_t *b, const char *s);
int  dama_board_to_str(const dama_board_t *b, char *out, size_t out_sz);

#ifdef __cplusplus
}
#endif
