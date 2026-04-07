// =============================================
// File: include/game/checkers_adapter.h
// Project: dama
// Purpose: GameAPI adapter for checkers
// License: MIT (c) 2025
// =============================================
#pragma once
#include "game/api.h"
#include "checkers/board.h"

#ifdef __cplusplus
extern "C" {
#endif

// Returns the singleton GameAPI for checkers.
const GameAPI *dama_api(void);

// Helpers for external tools (CLI, GUI).
int  dama_init_state_str(game_state_t *st, const char *pos_or_startpos);
void dama_move_to_str(game_move_t m, char *out, size_t out_sz);
const dama_board_t *dama_state_as_board(const game_state_t *st);

#ifdef __cplusplus
}
#endif
