#include "games.h"
#include "1_push_fast/1_push_fast.h"
#include "2_hit20/2_hit20.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static const game_descriptor_t g_games[] = {
    {"FAST", run_game_1_push_fast},
    {"HIT20", run_game_2_hit20},
};

const game_descriptor_t *games_get_all(size_t *count)
{
    if (count != NULL) {
        *count = ARRAY_SIZE(g_games);
    }
    return g_games;
}
