// =============================================
// File: src/checkers/board.c
// Project: dama
// Purpose: Board representation — full implementation
// License: MIT (c) 2025
//
// Dark-square convention (Italian checkers)
// -----------------------------------------
// Pieces live on dark squares where (rank + file) % 2 == 0.
//   rank 0, files 0,2,4,6  →  sq  0, 2, 4, 6
//   rank 1, files 1,3,5,7  →  sq  9,11,13,15
//   rank 2, files 0,2,4,6  →  sq 16,18,20,22
//   ...
// White occupies ranks 0-2 (bottom), Black ranks 5-7 (top).
// White promotes at rank 7, Black at rank 0.
// =============================================
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "checkers/board.h"
#include "core/bitops.h"
#include "core/log.h"

// ---- Internal helpers ----

// Convert algebraic notation (e.g. "a1") to square index 0..63.
// Returns -1 on error.
static int alg_to_sq(const char *s) {
    if (!s || s[0] < 'a' || s[0] > 'h') return -1;
    if (s[1] < '1' || s[1] > '8') return -1;
    int f = s[0] - 'a';
    int r = s[1] - '1';
    return r * 8 + f;
}

// ================================================================
// Lifecycle
// ================================================================

void dama_board_clear(dama_board_t *b) {
    memset(b, 0, sizeof(*b));
    b->side_to_move    = DAMA_WHITE;
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

// ================================================================
// Starting position
//
// Italian checkers: 12 men per side on dark squares (r+f)%2==0.
//   White: ranks 0-2
//     rank 0: sq  0, 2, 4, 6   (files a,c,e,g)
//     rank 1: sq  9,11,13,15   (files b,d,f,h)
//     rank 2: sq 16,18,20,22   (files a,c,e,g)
//   Black: ranks 5-7
//     rank 5: sq 41,43,45,47   (files b,d,f,h)
//     rank 6: sq 48,50,52,54   (files a,c,e,g)
//     rank 7: sq 57,59,61,63   (files b,d,f,h)
// ================================================================
void dama_board_set_startpos(dama_board_t *b) {
    dama_board_clear(b);
    static const int wSqs[] = { 0, 2, 4, 6,  9,11,13,15,  16,18,20,22 };
    static const int bSqs[] = {41,43,45,47, 48,50,52,54,  57,59,61,63 };
    for (int i = 0; i < 12; i++) {
        dama_board_place(b, DAMA_WHITE, DAMA_MAN, wSqs[i]);
        dama_board_place(b, DAMA_BLACK, DAMA_MAN, bSqs[i]);
    }
    LOGI("dama_board: starting position set");
}

// ================================================================
// Serialization
//
// Format: "W:Wa1,c1,...,Ke3:Bb6,...,Kd4"
//   First char = side to move (W or B)
//   :W section = white pieces (K prefix = king)
//   :B section = black pieces (K prefix = king)
// ================================================================

// Parse a ':W' or ':B' section and place pieces on the board.
// s points to the first piece character (after "W" or "B" after the colon).
// Returns pointer past the last character consumed (next ':' or '\0').
static const char *parse_pieces(dama_board_t *b, int color, const char *s) {
    while (*s && *s != ':') {
        // Skip commas
        while (*s == ',') s++;
        if (!*s || *s == ':') break;

        // Check for king prefix
        int piece = DAMA_MAN;
        if (*s == 'K' || *s == 'D') {
            piece = DAMA_KING;
            s++;
        }

        // Expect algebraic coordinate (e.g. "a1")
        if (s[0] >= 'a' && s[0] <= 'h' && s[1] >= '1' && s[1] <= '8') {
            int sq = alg_to_sq(s);
            if (sq >= 0)
                dama_board_place(b, color, piece, sq);
            s += 2;
        } else {
            // Skip unexpected characters
            while (*s && *s != ',' && *s != ':') s++;
        }
    }
    return s;
}

int dama_board_from_str(dama_board_t *b, const char *s) {
    if (!s) return -1;
    dama_board_clear(b);

    // Side to move
    if (*s == 'W' || *s == 'w') b->side_to_move = DAMA_WHITE;
    else if (*s == 'B' || *s == 'b') b->side_to_move = DAMA_BLACK;
    else return -1;
    s++;

    // Parse sections
    while (*s) {
        if (*s != ':') { s++; continue; }
        s++;  // skip ':'
        if (*s == 'W' || *s == 'w') {
            s++;
            s = parse_pieces(b, DAMA_WHITE, s);
        } else if (*s == 'B' || *s == 'b') {
            s++;
            s = parse_pieces(b, DAMA_BLACK, s);
        } else {
            s++;
        }
    }

    return 0;
}

int dama_board_to_str(const dama_board_t *b, char *out, size_t out_sz) {
    if (!out || out_sz == 0) return -1;

    // Side to move
    int n = snprintf(out, out_sz, "%c:", b->side_to_move == DAMA_WHITE ? 'W' : 'B');
    if (n < 0 || (size_t)n >= out_sz) return -1;

    // White pieces
    int written = n;
    int first = 1;
    n = snprintf(out + written, out_sz - written, "W");
    if (n < 0) return -1; written += n;

    for (int pt = DAMA_MAN; pt < DAMA_PIECE_N; pt++) {
        uint64_t bb = b->bb[DAMA_WHITE][pt];
        while (bb) {
            int sq = bo_extract_lsb_index(&bb);
            int r = sq >> 3, f = sq & 7;
            if (!first) {
                n = snprintf(out + written, out_sz - written, ",");
                if (n < 0) return -1; written += n;
            }
            first = 0;
            if (pt == DAMA_KING) {
                n = snprintf(out + written, out_sz - written, "K%c%d", 'a'+f, r+1);
            } else {
                n = snprintf(out + written, out_sz - written, "%c%d", 'a'+f, r+1);
            }
            if (n < 0) return -1; written += n;
        }
    }

    // Black pieces
    n = snprintf(out + written, out_sz - written, ":B");
    if (n < 0) return -1; written += n;
    first = 1;

    for (int pt = DAMA_MAN; pt < DAMA_PIECE_N; pt++) {
        uint64_t bb = b->bb[DAMA_BLACK][pt];
        while (bb) {
            int sq = bo_extract_lsb_index(&bb);
            int r = sq >> 3, f = sq & 7;
            if (!first) {
                n = snprintf(out + written, out_sz - written, ",");
                if (n < 0) return -1; written += n;
            }
            first = 0;
            if (pt == DAMA_KING) {
                n = snprintf(out + written, out_sz - written, "K%c%d", 'a'+f, r+1);
            } else {
                n = snprintf(out + written, out_sz - written, "%c%d", 'a'+f, r+1);
            }
            if (n < 0) return -1; written += n;
        }
    }

    return written;
}
