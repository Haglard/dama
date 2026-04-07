// =============================================
// File: src/game/checkers_adapter.c
// Project: dama
// Purpose: GameAPI adapter for Italian checkers
// License: MIT (c) 2025
// =============================================
#include <string.h>
#include "game/checkers_adapter.h"
#include "checkers/board.h"
#include "checkers/movegen.h"
#include "checkers/eval.h"
#include "checkers/zobrist.h"
#include "core/rng.h"
#include "core/log.h"

#define DAMA_HIST_MAX 512

typedef struct {
    dama_board_t b;                       // MUST be first member (for direct cast)
    uint64_t     hash_history[DAMA_HIST_MAX];
    int          hist_len;
} dama_state_t;

// ---- Lazy-init global Zobrist ----
static dama_zobrist_t gZ;
static int gZ_inited = 0;

static void ensure_zobrist(void) {
    if (!gZ_inited) {
        rng_t r;
        const uint64_t S[4] = {
            0xDADA000011112222ULL, 0x3333444455556666ULL,
            0x7777888899990000ULL, 0xAAAABBBBCCCCDDDDULL
        };
        rng_seed(&r, S);
        dama_zobrist_init(&gZ, &r);
        gZ_inited = 1;
        LOGI("dama_adapter: Zobrist initialized");
    }
}

// ---- GameAPI callbacks ----

static int dama_side_to_move_cb(const game_state_t *st) {
    return ((const dama_state_t *)st)->b.side_to_move;
}

static int dama_gen_legal_cb(const game_state_t *st, game_move_t *out, int cap) {
    dama_move_list_t ml;
    int n = dama_generate_legal(&((const dama_state_t *)st)->b, &ml);
    int cnt = (n < cap) ? n : cap;
    memcpy(out, ml.data, cnt * sizeof(game_move_t));
    return cnt;
}

static int dama_gen_captures_cb(const game_state_t *st, game_move_t *out, int cap) {
    dama_move_list_t ml;
    int n = dama_generate_captures(&((const dama_state_t *)st)->b, &ml);
    int cnt = (n < cap) ? n : cap;
    memcpy(out, ml.data, cnt * sizeof(game_move_t));
    return cnt;
}

static int dama_is_capture_cb(const game_state_t *st, game_move_t mv) {
    (void)st;
    return dama_move_is_cap(mv);
}

static uint64_t dama_make_move_cb(game_state_t *st, game_move_t mv, void *undo_buf) {
    ensure_zobrist();
    dama_state_t  *S = (dama_state_t *)st;
    dama_undo_t   *u = (dama_undo_t *)undo_buf;
    uint64_t key = dama_zobrist_hash(&gZ, &S->b);
    if (S->hist_len < DAMA_HIST_MAX)
        S->hash_history[S->hist_len++] = key;
    dama_make_move(&S->b, mv, u);
    return dama_zobrist_hash(&gZ, &S->b);
}

static uint64_t dama_unmake_move_cb(game_state_t *st, game_move_t mv, const void *undo_buf) {
    ensure_zobrist();
    dama_state_t      *S = (dama_state_t *)st;
    const dama_undo_t *u = (const dama_undo_t *)undo_buf;
    if (S->hist_len > 0) S->hist_len--;
    dama_unmake_move(&S->b, mv, u);
    return dama_zobrist_hash(&gZ, &S->b);
}

static uint64_t dama_hash_cb(const game_state_t *st) {
    ensure_zobrist();
    return dama_zobrist_hash(&gZ, &((const dama_state_t *)st)->b);
}

static int dama_is_terminal_cb(const game_state_t *st, game_result_t *out) {
    ensure_zobrist();
    const dama_state_t *S = (const dama_state_t *)st;

    // 40-move rule (no captures)
    if (S->b.halfmove_clock >= 40) {
        if (out) *out = GAME_RESULT_DRAW;
        return 1;
    }

    // Threefold repetition: current position in history >= 2 times
    uint64_t cur = dama_zobrist_hash(&gZ, &S->b);
    int reps = 0;
    for (int i = 0; i < S->hist_len; i++) {
        if (S->hash_history[i] == cur) {
            reps++;
            if (reps >= 2) {
                if (out) *out = GAME_RESULT_DRAW;
                return 1;
            }
        }
    }

    // No legal moves: current side loses
    dama_move_list_t ml;
    if (dama_generate_legal(&S->b, &ml) == 0) {
        if (out) *out = GAME_RESULT_LOSS;
        return 1;
    }

    if (out) *out = GAME_RESULT_NONE;
    return 0;
}

static unsigned dama_is_terminal_ext_cb(const game_state_t *st) {
    game_result_t r = GAME_RESULT_NONE;
    if (dama_is_terminal_cb(st, &r)) {
        if (r == GAME_RESULT_LOSS) return GAME_TERMFLAG_LOSS;
        if (r == GAME_RESULT_DRAW) return GAME_TERMFLAG_DRAW;
        if (r == GAME_RESULT_WIN)  return GAME_TERMFLAG_WIN;
    }
    return GAME_TERMFLAG_NONE;
}

static game_score_t dama_evaluate_cb(const game_state_t *st) {
    return (game_score_t)dama_eval(&((const dama_state_t *)st)->b);
}

static void dama_copy_cb(const game_state_t *src, game_state_t *dst) {
    memcpy(dst, src, sizeof(dama_state_t));
}

static int dama_capture_score_cb(const game_state_t *st, game_move_t mv) {
    if (!dama_move_is_cap(mv)) return 0;
    // MVV: reward capturing more valuable pieces
    // Use num_kings_cap as proxy for victim value
    int nkings = dama_move_num_kings(mv);
    int ncaps  = dama_move_num_caps(mv);
    return 1000 + nkings * 200 + ncaps * 50;
}

// ---- GameAPI table ----
static GameAPI DAMA_API = {
    .state_size        = sizeof(dama_state_t),
    .undo_size         = sizeof(dama_undo_t),
    .side_to_move      = dama_side_to_move_cb,
    .generate_legal    = dama_gen_legal_cb,
    .generate_captures = dama_gen_captures_cb,
    .is_capture        = dama_is_capture_cb,
    .make_move         = dama_make_move_cb,
    .unmake_move       = dama_unmake_move_cb,
    .hash              = dama_hash_cb,
    .is_terminal       = dama_is_terminal_cb,
    .is_terminal_ext   = dama_is_terminal_ext_cb,
    .evaluate          = dama_evaluate_cb,
    .copy              = dama_copy_cb,
    .capture_score     = dama_capture_score_cb,
};

const GameAPI *dama_api(void) { return &DAMA_API; }

// ---- External helpers ----

int dama_init_state_str(game_state_t *st, const char *pos) {
    dama_state_t *S = (dama_state_t *)st;
    S->hist_len = 0;
    if (!pos || strncmp(pos, "startpos", 8) == 0) {
        dama_board_set_startpos(&S->b);
        return 0;
    }
    return dama_board_from_str(&S->b, pos);
}

void dama_move_to_str(game_move_t m, char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    int fr = dama_move_from(m), to = dama_move_to(m);
    int nc = dama_move_num_caps(m);
    if (nc == 0) {
        snprintf(out, out_sz, "%c%d-%c%d",
            'a' + (fr & 7), (fr >> 3) + 1,
            'a' + (to & 7), (to >> 3) + 1);
    } else {
        snprintf(out, out_sz, "%c%dx%c%d",
            'a' + (fr & 7), (fr >> 3) + 1,
            'a' + (to & 7), (to >> 3) + 1);
    }
}

const dama_board_t *dama_state_as_board(const game_state_t *st) {
    return &((const dama_state_t *)st)->b;
}
