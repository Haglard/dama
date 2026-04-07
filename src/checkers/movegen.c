// =============================================
// File: src/checkers/movegen.c
// Project: dama
// Purpose: Legal move generation — Italian rules
// License: MIT (c) 2025
//
// Italian rules implemented:
//   1. Mandatory capture
//   2. Maximum number of captures
//   3. King attacker preferred (among max-capture sequences)
//   4. Most kings captured preferred (among still-equal sequences)
//   5. Men capture forward-only; kings are flying (any distance)
//   6. Promotion stops the jump sequence
//   7. Captured pieces removed immediately (not at end of sequence)
// =============================================
#include "checkers/movegen.h"
#include "core/bitops.h"
#include "core/log.h"
#include <string.h>

// ================================================================
// DFS capture context
// ================================================================

typedef struct {
    const dama_board_t *b;
    int  stm;           // side to move
    int  opp;
    int  from_sq;       // ORIGINAL starting square of the moving piece
    int  is_king;       // is the mover a king?
    // current chain state
    int  caps[DAMA_MAX_CAPS];  // captured squares in sequence
    int  nkings;               // number of kings captured so far
    int  seq_len;              // depth of current chain
    uint64_t captured_mask;    // squares of pieces captured so far in this chain
    // output
    game_move_t *out;
    int  cap;   // capacity of out[]
    int  n;     // count written
} dfs_ctx_t;

// Pack current sequence into a game_move_t and append to output.
static void emit_terminal(dfs_ctx_t *ctx, int to_sq, int is_promo) {
    if (ctx->n >= ctx->cap) return;
    game_move_t mv = (game_move_t)(ctx->from_sq & 63)
                   | ((game_move_t)(to_sq & 63) << 6)
                   | ((game_move_t)(ctx->seq_len & 15) << 12)
                   | ((game_move_t)(is_promo    ? 1 : 0) << 16)
                   | ((game_move_t)(ctx->is_king ? 1 : 0) << 17)
                   | ((game_move_t)(ctx->nkings & 15) << 18);
    for (int i = 0; i < ctx->seq_len && i < DAMA_MAX_CAPS; i++)
        mv |= (game_move_t)(ctx->caps[i] & 63) << (22 + i * 6);
    ctx->out[ctx->n++] = mv;
}

// ================================================================
// Man DFS — forward-only captures
// White men move toward higher ranks (dr = +1)
// Black men move toward lower ranks (dr = -1)
// ================================================================
static void dfs_man(dfs_ctx_t *ctx, int cur_sq) {
    int r = cur_sq >> 3, f = cur_sq & 7;
    int dr = (ctx->stm == DAMA_WHITE) ? 1 : -1;

    // Effective occupancy: original board minus from_sq, minus already-captured pieces.
    // (captured pieces are removed immediately so landing squares are clear)
    uint64_t eff_occ = ctx->b->occ_all & ~ctx->captured_mask & ~(1ULL << ctx->from_sq);
    uint64_t eff_opp = ctx->b->occ[ctx->opp] & ~ctx->captured_mask;

    int found = 0;

    for (int df = -1; df <= 1; df += 2) {
        int mr = r + dr, mf = f + df;          // mid square (piece to capture)
        int lr = r + 2*dr, lf = f + 2*df;      // landing square

        if (mr < 0 || mr > 7 || mf < 0 || mf > 7) continue;
        if (lr < 0 || lr > 7 || lf < 0 || lf > 7) continue;

        int mid_sq  = mr * 8 + mf;
        int land_sq = lr * 8 + lf;

        if (!(eff_opp & (1ULL << mid_sq))) continue;  // no capturable piece
        if (  eff_occ & (1ULL << land_sq)) continue;  // landing occupied

        int was_king = (ctx->b->bb[ctx->opp][DAMA_KING] & (1ULL << mid_sq)) ? 1 : 0;
        ctx->caps[ctx->seq_len] = mid_sq;
        ctx->captured_mask |= (1ULL << mid_sq);
        ctx->nkings  += was_king;
        ctx->seq_len++;
        found = 1;

        int is_promo = (ctx->stm == DAMA_WHITE) ? (lr == 7) : (lr == 0);
        if (is_promo || ctx->seq_len >= DAMA_MAX_CAPS) {
            emit_terminal(ctx, land_sq, is_promo);
        } else {
            dfs_man(ctx, land_sq);
        }

        ctx->seq_len--;
        ctx->nkings  -= was_king;
        ctx->captured_mask &= ~(1ULL << mid_sq);
    }

    // No further captures: emit what we have (if chain is non-empty)
    if (!found && ctx->seq_len > 0)
        emit_terminal(ctx, cur_sq, 0);
}

// ================================================================
// King DFS — flying king, all 4 diagonals
// ================================================================
static const int KING_DR[4] = {+1, +1, -1, -1};
static const int KING_DF[4] = {+1, -1, +1, -1};

static void dfs_king(dfs_ctx_t *ctx, int cur_sq) {
    uint64_t eff_occ = ctx->b->occ_all & ~ctx->captured_mask & ~(1ULL << ctx->from_sq);
    uint64_t eff_opp = ctx->b->occ[ctx->opp] & ~ctx->captured_mask;

    int found = 0;

    for (int d = 0; d < 4; d++) {
        int dr = KING_DR[d], df = KING_DF[d];
        int r  = cur_sq >> 3, fc = cur_sq & 7;

        // Scan diagonal to find first occupied square
        int opp_sq = -1;
        int sr = r + dr, sf = fc + df;
        while (sr >= 0 && sr <= 7 && sf >= 0 && sf <= 7) {
            int sq = sr * 8 + sf;
            if (eff_occ & (1ULL << sq)) {
                if (eff_opp & (1ULL << sq)) opp_sq = sq;
                // else own piece: direction blocked
                break;
            }
            sr += dr; sf += df;
        }
        if (opp_sq < 0) continue;  // no capturable piece in this direction

        // Try every empty landing square beyond opp_sq
        int was_king = (ctx->b->bb[ctx->opp][DAMA_KING] & (1ULL << opp_sq)) ? 1 : 0;
        int lr = (opp_sq >> 3) + dr, lf = (opp_sq & 7) + df;
        while (lr >= 0 && lr <= 7 && lf >= 0 && lf <= 7) {
            int land_sq = lr * 8 + lf;
            if (eff_occ & (1ULL << land_sq)) break;  // blocked

            ctx->caps[ctx->seq_len] = opp_sq;
            ctx->captured_mask |= (1ULL << opp_sq);
            ctx->nkings  += was_king;
            ctx->seq_len++;
            found = 1;

            if (ctx->seq_len >= DAMA_MAX_CAPS) {
                emit_terminal(ctx, land_sq, 0);
            } else {
                dfs_king(ctx, land_sq);
            }

            ctx->seq_len--;
            ctx->nkings  -= was_king;
            ctx->captured_mask &= ~(1ULL << opp_sq);

            lr += dr; lf += df;
        }
    }

    if (!found && ctx->seq_len > 0)
        emit_terminal(ctx, cur_sq, 0);
}

// ================================================================
// Italian rules filter
// Filters buf[0..n) in-place; returns new count.
// Priority: (1) max captures, (2) king attacker, (3) max kings cap.
// ================================================================
static int filter_captures(game_move_t *buf, int n) {
    if (n == 0) return 0;
    int i, out;

    // 1. Maximum number of captures
    int max_caps = 0;
    for (i = 0; i < n; i++) {
        int nc = (int)((buf[i] >> 12) & 15);
        if (nc > max_caps) max_caps = nc;
    }
    out = 0;
    for (i = 0; i < n; i++)
        if ((int)((buf[i] >> 12) & 15) == max_caps) buf[out++] = buf[i];
    n = out;

    // 2. King attacker preference
    int has_king = 0;
    for (i = 0; i < n; i++)
        if ((buf[i] >> 17) & 1) { has_king = 1; break; }
    if (has_king) {
        out = 0;
        for (i = 0; i < n; i++)
            if ((buf[i] >> 17) & 1) buf[out++] = buf[i];
        n = out;
    }

    // 3. Maximum kings captured
    int max_kings = 0;
    for (i = 0; i < n; i++) {
        int nk = (int)((buf[i] >> 18) & 15);
        if (nk > max_kings) max_kings = nk;
    }
    out = 0;
    for (i = 0; i < n; i++)
        if ((int)((buf[i] >> 18) & 15) == max_kings) buf[out++] = buf[i];
    return out;
}

// ================================================================
// Capture generation (mandatory-capture + Italian rules)
// ================================================================
int dama_generate_captures(const dama_board_t *b, dama_move_list_t *ml) {
    ml->n = 0;

    // Temporary buffer (pre-filter: can be larger than DAMA_MAX_MOVES)
    game_move_t tmp[512];
    int total = 0;
    int stm = b->side_to_move, opp = stm ^ 1;

    for (int pt = DAMA_MAN; pt < DAMA_PIECE_N && total < 512; pt++) {
        uint64_t pieces = b->bb[stm][pt];
        while (pieces && total < 512) {
            int sq = bo_extract_lsb_index(&pieces);
            dfs_ctx_t ctx = {
                .b = b, .stm = stm, .opp = opp,
                .from_sq = sq, .is_king = (pt == DAMA_KING),
                .seq_len = 0, .nkings = 0, .captured_mask = 0,
                .out = tmp + total,
                .cap = 512 - total,
                .n   = 0
            };
            if (pt == DAMA_MAN) dfs_man (&ctx, sq);
            else                dfs_king(&ctx, sq);
            total += ctx.n;
        }
    }

    if (total == 0) return 0;

    // Apply Italian rules filter
    total = filter_captures(tmp, total);
    if (total > DAMA_MAX_MOVES) total = DAMA_MAX_MOVES;

    memcpy(ml->data, tmp, total * sizeof(game_move_t));
    ml->n = total;
    return total;
}

// ================================================================
// Simple (non-capture) move generation
// ================================================================
static int gen_simple(const dama_board_t *b, game_move_t *out, int cap) {
    int n = 0;
    int stm = b->side_to_move;
    int dr_man = (stm == DAMA_WHITE) ? 1 : -1;

    // Men: one step diagonally forward
    {
        uint64_t men = b->bb[stm][DAMA_MAN];
        while (men && n < cap) {
            int sq = bo_extract_lsb_index(&men);
            int r = sq >> 3, f = sq & 7;
            for (int df = -1; df <= 1; df += 2) {
                int nr = r + dr_man, nf = f + df;
                if (nr < 0 || nr > 7 || nf < 0 || nf > 7) continue;
                int to = nr * 8 + nf;
                if (b->occ_all & (1ULL << to)) continue;
                int is_promo = (stm == DAMA_WHITE) ? (nr == 7) : (nr == 0);
                if (n < cap)
                    out[n++] = dama_move_quiet(sq, to, is_promo, 0);
            }
        }
    }

    // Kings: any distance diagonally
    {
        uint64_t kings = b->bb[stm][DAMA_KING];
        while (kings && n < cap) {
            int sq = bo_extract_lsb_index(&kings);
            for (int d = 0; d < 4; d++) {
                int dr = KING_DR[d], df = KING_DF[d];
                int cr = sq >> 3, cf = sq & 7;
                cr += dr; cf += df;
                while (cr >= 0 && cr <= 7 && cf >= 0 && cf <= 7) {
                    int to = cr * 8 + cf;
                    if (b->occ_all & (1ULL << to)) break;
                    if (n < cap)
                        out[n++] = dama_move_quiet(sq, to, 0, 1);
                    cr += dr; cf += df;
                }
            }
        }
    }

    return n;
}

// ================================================================
// Legal move generation (mandatory capture first)
// ================================================================
int dama_generate_legal(const dama_board_t *b, dama_move_list_t *ml) {
    int nc = dama_generate_captures(b, ml);
    if (nc > 0) return nc;
    ml->n = gen_simple(b, ml->data, DAMA_MAX_MOVES);
    return ml->n;
}

// ================================================================
// Make / Unmake move
// ================================================================

void dama_make_move(dama_board_t *b, game_move_t mv, dama_undo_t *u) {
    int from   = dama_move_from(mv);
    int to     = dama_move_to(mv);
    int ncaps  = dama_move_num_caps(mv);
    int is_promo = dama_move_is_promo(mv);
    int stm    = b->side_to_move;
    int opp    = stm ^ 1;

    // Determine mover piece type
    int mover_type = (b->bb[stm][DAMA_KING] & (1ULL << from)) ? DAMA_KING : DAMA_MAN;

    // Fill undo record
    u->n_caps      = ncaps;
    u->was_promo   = is_promo;
    u->mover_type  = mover_type;
    u->old_halfmove = b->halfmove_clock;
    u->old_fullmove = b->fullmove_number;
    for (int i = 0; i < ncaps && i < DAMA_MAX_CAPS; i++) {
        int csq = dama_move_cap_sq(mv, i);
        u->cap_sqs[i]   = csq;
        u->cap_types[i] = (b->bb[opp][DAMA_KING] & (1ULL << csq)) ? DAMA_KING : DAMA_MAN;
    }

    // Remove mover from origin
    dama_board_remove(b, stm, mover_type, from);

    // Remove captured pieces (Italian: removed immediately, one by one)
    for (int i = 0; i < ncaps && i < DAMA_MAX_CAPS; i++)
        dama_board_remove(b, opp, u->cap_types[i], u->cap_sqs[i]);

    // Place mover at destination (possibly promoted)
    int final_type = (is_promo || mover_type == DAMA_KING) ? DAMA_KING : DAMA_MAN;
    dama_board_place(b, stm, final_type, to);

    // Update clocks
    b->halfmove_clock = (ncaps > 0) ? 0 : b->halfmove_clock + 1;
    if (stm == DAMA_BLACK) b->fullmove_number++;
    b->side_to_move = opp;
}

void dama_unmake_move(dama_board_t *b, game_move_t mv, const dama_undo_t *u) {
    int from = dama_move_from(mv);
    int to   = dama_move_to(mv);

    // After make_move: side_to_move is opp; the mover was side_to_move^1
    int stm_was = b->side_to_move ^ 1;  // who made the move
    int opp_was = b->side_to_move;       // their opponent

    // Restore clocks and side
    b->halfmove_clock  = u->old_halfmove;
    b->fullmove_number = u->old_fullmove;
    b->side_to_move    = stm_was;

    // Remove mover from destination (may be king due to promotion or original king)
    int at_to = (u->was_promo || u->mover_type == DAMA_KING) ? DAMA_KING : DAMA_MAN;
    dama_board_remove(b, stm_was, at_to, to);

    // Restore mover at origin with original type
    dama_board_place(b, stm_was, u->mover_type, from);

    // Restore captured pieces
    for (int i = 0; i < u->n_caps && i < DAMA_MAX_CAPS; i++)
        dama_board_place(b, opp_was, u->cap_types[i], u->cap_sqs[i]);
}
