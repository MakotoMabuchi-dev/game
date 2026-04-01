#include "games.h"
#include "1_push_fast/1_push_fast.h"
#include "2_hit20/2_hit20.h"
#include "3_TETRIS/3_TETRIS.h"
#include "4_ring8/4_ring8.h"
#include "5_ninja/5_ninja.h"
#include "6_jump/6_jump.h"
#include "7_block/7_block.h"
#include "8_invader/8_invader.h"
#include "9_pinball/9_pinball.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static const game_descriptor_t g_games[] = {
    {"FAST", run_game_1_push_fast, game_1_push_fast_get_best_record},
    {"HIT20", run_game_2_hit20, game_2_hit20_get_best_record},
    {"TETRIS", run_game_3_tetris, game_3_tetris_get_best_record},
    {"RING8", run_game_4_ring8, game_4_ring8_get_best_record},
    {"NINJA", run_game_5_ninja, game_5_ninja_get_best_record},
    {"JUMP", run_game_6_jump, game_6_jump_get_best_record},
    {"BLOCK", run_game_7_block, game_7_block_get_best_record},
    {"INVADER", run_game_8_invader, game_8_invader_get_best_record},
    {"PINBALL", run_game_9_pinball, game_9_pinball_get_best_record},
};

const game_descriptor_t *games_get_all(size_t *count)
{
    if (count != NULL) {
        *count = ARRAY_SIZE(g_games);
    }
    return g_games;
}
