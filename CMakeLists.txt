cmake_minimum_required(VERSION 3.16)
project(dama C OBJC)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---- Include paths ----
include_directories(include)

# ================================================================
# core — generic utilities (log, vec, hashmap, pool, ringbuf, rng,
#         time, bitops, GameAPI base)
# ================================================================
add_library(core STATIC
    src/core/log.c
    src/core/vec.c
    src/core/hashmap.c
    src/core/time.c
    src/core/rng.c
    src/core/bitops.c
    src/core/ringbuf.c
    src/core/pool.c
    src/game/api.c
    src/game/nim.c
)
target_include_directories(core PUBLIC include)

# ================================================================
# checkers — dama board, movegen, eval, zobrist
# ================================================================
add_library(checkers STATIC
    src/checkers/board.c
    src/checkers/movegen.c
    src/checkers/eval.c
    src/checkers/zobrist.c
)
target_include_directories(checkers PUBLIC include)
target_link_libraries(checkers PRIVATE core)

# ================================================================
# game_dama — GameAPI adapter for checkers
# ================================================================
add_library(game_dama STATIC
    src/game/checkers_adapter.c
)
target_include_directories(game_dama PUBLIC include)
target_link_libraries(game_dama PRIVATE checkers core)

# ================================================================
# engine — generic negamax / alpha-beta / TT (reused unchanged)
# ================================================================
add_library(engine STATIC
    src/engine/search.c
    src/engine/tt.c
)
target_include_directories(engine PUBLIC include)
target_link_libraries(engine PRIVATE core)

# ================================================================
# Tools
# ================================================================

# --- search_cli: single position search ---
add_executable(search_cli tools/search_cli.c)
target_include_directories(search_cli PRIVATE include)
target_link_libraries(search_cli PRIVATE game_dama checkers engine core)

# --- selfplay_cli: engine self-play ---
add_executable(selfplay_cli tools/selfplay_cli.c)
target_include_directories(selfplay_cli PRIVATE include)
target_link_libraries(selfplay_cli PRIVATE game_dama checkers engine core)

# --- Native macOS GUI client (Cocoa) ---
if(APPLE)
    enable_language(OBJC)

    # Build as a proper .app bundle
    add_executable(dama_client MACOSX_BUNDLE tools/dama_client_cocoa.m)
    target_include_directories(dama_client PRIVATE include)
    target_compile_options(dama_client PRIVATE -fobjc-arc)
    target_link_libraries(dama_client PRIVATE
        game_dama checkers engine core
        "-framework AppKit" "-framework Foundation")

    set_target_properties(dama_client PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME          "Vibe Dama"
        MACOSX_BUNDLE_GUI_IDENTIFIER       "com.haglard.vibedama"
        MACOSX_BUNDLE_BUNDLE_VERSION       "1.0.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_COPYRIGHT            "© 2026 Haglard"
        OUTPUT_NAME                        "Vibe Dama"
    )

    # Custom target: create VibeDama-1.0.dmg from the .app bundle
    add_custom_target(dmg
        DEPENDS dama_client
        COMMAND ${CMAKE_COMMAND} -E remove -f
                "${CMAKE_BINARY_DIR}/VibeDama-1.0.dmg"
        COMMAND hdiutil create
                -volname   "Vibe Dama 1.0"
                -srcfolder "${CMAKE_BINARY_DIR}/Vibe Dama.app"
                -ov
                -format    UDZO
                "${CMAKE_BINARY_DIR}/VibeDama-1.0.dmg"
        COMMENT "Creating VibeDama-1.0.dmg …"
        VERBATIM
    )
endif()

# ================================================================
# Tests
# ================================================================
enable_testing()

# Example: unit test for board
# add_executable(test_board tests/unit/test_board.c)
# target_link_libraries(test_board PRIVATE checkers core)
# add_test(NAME test_board COMMAND test_board)
