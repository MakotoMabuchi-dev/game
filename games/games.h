#ifndef GAMES_H
#define GAMES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t *random_state;
    const char *game_name;
    bool is_first_start;
} game_run_context_t;

static inline bool game_consume_first_start(game_run_context_t *context)
{
    bool is_first_start = context->is_first_start;
    context->is_first_start = false;
    return is_first_start;
}

typedef struct {
    const char *name;
    void (*run)(game_run_context_t *context);
} game_descriptor_t;

const game_descriptor_t *games_get_all(size_t *count);

#endif
