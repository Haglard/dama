// =============================================
// File: tools/search_cli.c
// Project: dama
// Purpose: CLI — search a single position and print best move
// Usage: search_cli [position] [time_ms]
//   position: "startpos" or "W:Wa1,...:Bb8,..."
//   time_ms:  search time in milliseconds (default 2000)
// License: MIT (c) 2025
// =============================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/api.h"
#include "game/checkers_adapter.h"
#include "checkers/board.h"
#include "checkers/movegen.h"
#include "engine/search.h"
#include "core/log.h"

int main(int argc, char *argv[]) {
    const char *pos_str = (argc >= 2) ? argv[1] : "startpos";
    int time_ms         = (argc >= 3) ? atoi(argv[2]) : 2000;

    const GameAPI *api = dama_api();

    game_state_t *st = calloc(1, api->state_size);
    if (!st) { fprintf(stderr, "OOM\n"); return 1; }

    if (dama_init_state_str(st, pos_str) != 0) {
        fprintf(stderr, "Invalid position: %s\n", pos_str);
        free(st);
        return 1;
    }

    // Print board
    const dama_board_t *b = dama_state_as_board(st);
    char buf[256];
    dama_board_to_str(b, buf, sizeof(buf));
    printf("Position : %s\n", buf);
    printf("Side     : %s\n", b->side_to_move == DAMA_WHITE ? "White" : "Black");
    printf("Time     : %d ms\n\n", time_ms);

    // Generate legal moves
    dama_move_list_t ml;
    int n = dama_generate_legal(b, &ml);
    printf("Legal moves (%d):\n", n);
    for (int i = 0; i < n; i++) {
        char ms[16];
        dama_move_to_str(ml.data[i], ms, sizeof(ms));
        printf("  %s\n", ms);
    }
    printf("\n");

    if (n == 0) {
        printf("No legal moves — game over.\n");
        free(st);
        return 0;
    }

    // Search
    volatile int stop = 0;
    search_params_t sp = {0};
    sp.use_time    = 1;
    sp.time_ms     = time_ms;
    sp.max_depth   = 99;
    sp.use_qsearch = 1;
    sp.tt_size_mb  = 64;
    sp.stop        = &stop;

    search_result_t res = {0};
    search_root(api, st, &sp, &res);

    char best[16];
    dama_move_to_str(res.best_move, best, sizeof(best));

    printf("Best move : %s\n", best);
    printf("Score     : %+.2f\n", res.score / 100.0);
    printf("Depth     : %d\n", res.depth_searched);
    printf("Nodes     : %llu\n", (unsigned long long)res.nodes);
    printf("NPS       : %.0f\n", res.nps);
    printf("Time      : %.3f s\n", res.time_ns / 1e9);

    if (res.pv_len > 0) {
        printf("PV        :");
        for (int i = 0; i < res.pv_len; i++) {
            char ms[16];
            dama_move_to_str(res.pv[i], ms, sizeof(ms));
            printf(" %s", ms);
        }
        printf("\n");
    }

    free(st);
    return 0;
}
