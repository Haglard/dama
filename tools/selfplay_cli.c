// =============================================
// File: tools/selfplay_cli.c
// Project: dama
// Purpose: CLI — engine self-play (CvC)
// Usage: selfplay_cli [time_ms] [max_moves] [position]
//   time_ms:   per-move search time in ms (default 1000)
//   max_moves: max half-moves before stopping (default 200)
//   position:  starting position string (default "startpos")
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
    int time_ms   = (argc >= 2) ? atoi(argv[1]) : 1000;
    int max_moves = (argc >= 3) ? atoi(argv[2]) : 200;
    const char *pos_str = (argc >= 4) ? argv[3] : "startpos";

    const GameAPI *api = dama_api();

    game_state_t *st = calloc(1, api->state_size);
    if (!st) { fprintf(stderr, "OOM\n"); return 1; }

    if (dama_init_state_str(st, pos_str) != 0) {
        fprintf(stderr, "Invalid position: %s\n", pos_str);
        free(st);
        return 1;
    }

    printf("=== Vibe Dama — self-play ===\n");
    printf("Time/move: %d ms  |  Max half-moves: %d\n\n", time_ms, max_moves);

    volatile int stop = 0;

    for (int half = 0; half < max_moves; half++) {
        // Check terminal
        game_result_t result = GAME_RESULT_NONE;
        if (api->is_terminal(st, &result)) {
            const dama_board_t *b = dama_state_as_board(st);
            int stm = b->side_to_move;
            printf("\n--- Game over ---\n");
            if (result == GAME_RESULT_LOSS)
                printf("Result: %s wins\n", stm == DAMA_WHITE ? "Black" : "White");
            else if (result == GAME_RESULT_DRAW)
                printf("Result: Draw\n");
            else
                printf("Result: Unknown\n");
            break;
        }

        // Search
        search_params_t sp = {0};
        sp.use_time    = 1;
        sp.time_ms     = time_ms;
        sp.max_depth   = 99;
        sp.use_qsearch = 1;
        sp.tt_size_mb  = 64;
        sp.stop        = &stop;

        search_result_t res = {0};
        search_root(api, st, &sp, &res);

        if (res.best_move == 0) {
            printf("Engine returned null move — stopping.\n");
            break;
        }

        // Print move
        const dama_board_t *b = dama_state_as_board(st);
        int stm = b->side_to_move;
        char ms[16];
        dama_move_to_str(res.best_move, ms, sizeof(ms));
        printf("%3d. [%s] %-10s  score=%+.2f  depth=%d  nodes=%llu\n",
               half / 2 + 1,
               stm == DAMA_WHITE ? "W" : "B",
               ms,
               res.score / 100.0,
               res.depth_searched,
               (unsigned long long)res.nodes);

        // Apply
        void *undo = calloc(1, api->undo_size);
        if (!undo) { fprintf(stderr, "OOM\n"); break; }
        api->make_move(st, res.best_move, undo);
        free(undo);
    }

    free(st);
    return 0;
}
