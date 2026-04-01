#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "app.h"
#include "best_store.h"
#include "2_hit20/2_hit20.h"
#include "8_invader.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define INVADER_FRAME_US 16666
#define INVADER_COLOR_BLUE 0x1F00
#define INVADER_COLOR_GREEN 0xE007
#define INVADER_BASE_ROWS 3
#define INVADER_BASE_COLS 6
#define INVADER_DOUBLE_ROWS 4
#define INVADER_DOUBLE_COLS 9
#define INVADER_MAX_ROWS INVADER_DOUBLE_ROWS
#define INVADER_MAX_COLS INVADER_DOUBLE_COLS
#define INVADER_SPRITE_W 11
#define INVADER_SPRITE_H 8
#define INVADER_SPACING_X 18
#define INVADER_SPACING_Y 16
#define INVADER_FORMATION_START_Y 74
#define INVADER_FORMATION_DESCEND 12
#define INVADER_EDGE_MARGIN 14
#define INVADER_PLAYER_Y 308
#define INVADER_PLAYER_W 20
#define INVADER_PLAYER_H 10
#define INVADER_PLAYER_MIN_X ((INVADER_PLAYER_W / 2) + 10)
#define INVADER_PLAYER_MAX_X (LCD_WIDTH - INVADER_PLAYER_MIN_X)
#define INVADER_PLAYER_SPEED 240
#define INVADER_PLAYER_BULLET_W 2
#define INVADER_PLAYER_BULLET_H 10
#define INVADER_PLAYER_BULLET_SPEED 320
#define INVADER_ENEMY_BULLET_W 4
#define INVADER_ENEMY_BULLET_H 10
#define INVADER_ENEMY_BULLET_SPEED_BASE 180
#define INVADER_MAX_ENEMY_BULLETS 3
#define INVADER_HUD_LABEL_Y 14
#define INVADER_HUD_VALUE_Y 36
#define INVADER_CONTROL_Y 334
#define INVADER_BUNKER_COUNT 3
#define INVADER_BUNKER_CELL_SIZE 2
#define INVADER_BUNKER_GRID_W 14
#define INVADER_BUNKER_GRID_H 8
#define INVADER_BUNKER_Y 246
#define INVADER_UFO_W 27
#define INVADER_UFO_H 10
#define INVADER_UFO_Y 58
#define INVADER_UFO_SPEED 126
#define INVADER_UFO_POINTS 12u
#define INVADER_UFO_MIN_DELAY_MS 6000u
#define INVADER_UFO_DELAY_RANGE_MS 7000u

typedef struct {
    bool active;
    int32_t x;
    int32_t y;
    int speed;
} invader_bullet_t;

typedef struct {
    bool active;
    int32_t x;
    int dir;
} invader_ufo_t;

typedef struct {
    bool invaders[INVADER_MAX_ROWS][INVADER_MAX_COLS];
    bool bunkers[INVADER_BUNKER_COUNT][INVADER_BUNKER_GRID_H][INVADER_BUNKER_GRID_W];
    uint32_t score;
    uint32_t wave;
    uint32_t remaining_invaders;
    bool bunkers_active;
    int formation_rows;
    int formation_cols;
    int32_t formation_x;
    int formation_y;
    int formation_dir;
    int player_x;
    bool player_bullet_active;
    int32_t player_bullet_x;
    int32_t player_bullet_y;
    invader_ufo_t ufo;
    invader_bullet_t enemy_bullets[INVADER_MAX_ENEMY_BULLETS];
    uint32_t next_player_shot_ms;
    uint32_t next_enemy_shot_ms;
    uint32_t next_ufo_spawn_ms;
} invader_state_t;

static uint32_t best_score = 0;
static bool best_loaded = false;

static const char *const invader_enemy_rows[INVADER_SPRITE_H] = {
    "00100100100",
    "00011111000",
    "01111111110",
    "11011011011",
    "11111111111",
    "00110110100",
    "01000000100",
    "10100000101",
};

static void invader_load_best(void)
{
    uint32_t stored_value = 0;
    bool has_value = false;

    if (best_loaded) {
        return;
    }
    if (!best_store_load_u32("INVADER", &stored_value, &has_value)) {
        return;
    }

    if (has_value) {
        best_score = stored_value;
    }
    best_loaded = true;
}

static void invader_save_best(void)
{
    best_loaded = true;
    best_store_save_u32("INVADER", best_score, best_score != 0u);
}

static const char *const invader_player_rows[8] = {
    "00000100000",
    "00001110000",
    "00011111000",
    "00111111100",
    "01111111110",
    "11111111111",
    "01111011110",
    "00110001100",
};

static const char *const invader_ufo_rows[INVADER_UFO_H] = {
    "000000000111111111100000000",
    "000000111111111111111100000",
    "000011111111111111111111000",
    "000111111111111111111111100",
    "001111111111111111111111110",
    "111111001111111111100111111",
    "111111111111111111111111111",
    "001111111111111111111111110",
    "000011100011000110001110000",
    "000110000000000000000011000",
};

static const char *const invader_bunker_rows[INVADER_BUNKER_GRID_H] = {
    "00111111111100",
    "01111111111110",
    "11111111111111",
    "11111111111111",
    "11111100111111",
    "11111000011111",
    "11110000001111",
    "11000000000011",
};

static void invader_format_score(char *buffer, size_t buffer_size, uint32_t score)
{
    snprintf(buffer, buffer_size, "%lu", (unsigned long)score);
}

static int invader_wave_rows(uint32_t wave)
{
    return wave >= 2u ? INVADER_DOUBLE_ROWS : INVADER_BASE_ROWS;
}

static int invader_wave_cols(uint32_t wave)
{
    return wave >= 2u ? INVADER_DOUBLE_COLS : INVADER_BASE_COLS;
}

static int invader_total_invaders(const invader_state_t *state)
{
    return state->formation_rows * state->formation_cols;
}

static int invader_bunker_pixel_width(void)
{
    return INVADER_BUNKER_GRID_W * INVADER_BUNKER_CELL_SIZE;
}

static int invader_bunker_pixel_height(void)
{
    return INVADER_BUNKER_GRID_H * INVADER_BUNKER_CELL_SIZE;
}

static int invader_bunker_left(int index)
{
    int center_x = ((index + 1) * LCD_WIDTH) / (INVADER_BUNKER_COUNT + 1);

    return center_x - (invader_bunker_pixel_width() / 2);
}

static int invader_formation_width(const invader_state_t *state)
{
    return ((state->formation_cols - 1) * INVADER_SPACING_X) + INVADER_SPRITE_W;
}

static int invader_formation_height(const invader_state_t *state)
{
    return ((state->formation_rows - 1) * INVADER_SPACING_Y) + INVADER_SPRITE_H;
}

static int invader_formation_speed(const invader_state_t *state)
{
    int speed = 30 + ((int)state->wave * 7) +
                ((invader_total_invaders(state) - (int)state->remaining_invaders) * 2);

    if (speed > 110) {
        speed = 110;
    }
    return speed;
}

static uint32_t invader_player_shot_interval_ms(const invader_state_t *state)
{
    uint32_t interval = 260u;

    if (state->wave > 1u) {
        interval -= (state->wave - 1u) * 10u;
    }
    if (interval < 150u) {
        interval = 150u;
    }
    return interval;
}

static uint32_t invader_enemy_shot_interval_ms(const invader_state_t *state)
{
    uint32_t interval = 900u;

    if (state->wave > 1u) {
        interval -= (state->wave - 1u) * 70u;
    }
    if (interval < 280u) {
        interval = 280u;
    }
    return interval;
}

static int invader_enemy_bullet_speed(const invader_state_t *state)
{
    return INVADER_ENEMY_BULLET_SPEED_BASE + ((int)state->wave * 18);
}

static void invader_init_bunkers(invader_state_t *state)
{
    memset(state->bunkers, 0, sizeof(state->bunkers));
    state->bunkers_active = state->wave >= 2u;
    if (!state->bunkers_active) {
        return;
    }

    for (int bunker = 0; bunker < INVADER_BUNKER_COUNT; ++bunker) {
        for (int row = 0; row < INVADER_BUNKER_GRID_H; ++row) {
            for (int col = 0; col < INVADER_BUNKER_GRID_W; ++col) {
                state->bunkers[bunker][row][col] = invader_bunker_rows[row][col] == '1';
            }
        }
    }
}

static uint32_t invader_next_ufo_delay_ms(uint32_t *random_state)
{
    return INVADER_UFO_MIN_DELAY_MS +
           (app_random_next(random_state) % INVADER_UFO_DELAY_RANGE_MS);
}

static void invader_schedule_next_ufo(invader_state_t *state, uint32_t elapsed_ms, uint32_t *random_state)
{
    state->ufo.active = false;
    state->next_ufo_spawn_ms = elapsed_ms + invader_next_ufo_delay_ms(random_state);
}

static void invader_spawn_ufo(invader_state_t *state, uint32_t *random_state)
{
    if ((app_random_next(random_state) & 1u) == 0u) {
        state->ufo.dir = 1;
        state->ufo.x = -(INVADER_UFO_W * 1000);
    } else {
        state->ufo.dir = -1;
        state->ufo.x = LCD_WIDTH * 1000;
    }
    state->ufo.active = true;
}

static void invader_start_wave(invader_state_t *state, uint32_t elapsed_ms)
{
    state->wave++;
    state->formation_rows = invader_wave_rows(state->wave);
    state->formation_cols = invader_wave_cols(state->wave);
    state->remaining_invaders = (uint32_t)invader_total_invaders(state);
    state->formation_x = ((LCD_WIDTH - invader_formation_width(state)) / 2) * 1000;
    state->formation_y = INVADER_FORMATION_START_Y;
    state->formation_dir = 1;
    state->player_bullet_active = false;
    state->next_player_shot_ms = elapsed_ms + 120u;
    state->next_enemy_shot_ms = elapsed_ms + 500u;
    memset(state->enemy_bullets, 0, sizeof(state->enemy_bullets));
    memset(state->invaders, 0, sizeof(state->invaders));
    invader_init_bunkers(state);

    for (int row = 0; row < state->formation_rows; ++row) {
        for (int col = 0; col < state->formation_cols; ++col) {
            state->invaders[row][col] = true;
        }
    }
}

static void invader_init_round(invader_state_t *state)
{
    memset(state, 0, sizeof(*state));
    state->player_x = LCD_WIDTH / 2;
    state->next_ufo_spawn_ms = INVADER_UFO_MIN_DELAY_MS;
    invader_start_wave(state, 0u);
}

static bool invader_rect_overlap(int left_a, int top_a, int right_a, int bottom_a,
                                 int left_b, int top_b, int right_b, int bottom_b)
{
    return left_a < right_b && right_a > left_b &&
           top_a < bottom_b && bottom_a > top_b;
}

static int invader_clamp_player_x(int player_x)
{
    if (player_x < INVADER_PLAYER_MIN_X) {
        return INVADER_PLAYER_MIN_X;
    }
    if (player_x > INVADER_PLAYER_MAX_X) {
        return INVADER_PLAYER_MAX_X;
    }
    return player_x;
}

static void invader_move_formation(invader_state_t *state, uint32_t delta_ms)
{
    int32_t left_limit = INVADER_EDGE_MARGIN * 1000;
    int32_t right_limit = (LCD_WIDTH - INVADER_EDGE_MARGIN - invader_formation_width(state)) * 1000;

    state->formation_x += state->formation_dir * invader_formation_speed(state) * (int32_t)delta_ms;

    if (state->formation_x <= left_limit) {
        state->formation_x = left_limit;
        state->formation_dir = 1;
        state->formation_y += INVADER_FORMATION_DESCEND;
    } else if (state->formation_x >= right_limit) {
        state->formation_x = right_limit;
        state->formation_dir = -1;
        state->formation_y += INVADER_FORMATION_DESCEND;
    }
}

static bool invader_invaders_reached_player(const invader_state_t *state)
{
    int invader_bottom = state->formation_y + invader_formation_height(state);
    int player_top = INVADER_PLAYER_Y - (INVADER_PLAYER_H / 2);

    return invader_bottom >= player_top;
}

static bool invader_hit_enemy(invader_state_t *state)
{
    int bullet_x;
    int bullet_y;

    if (!state->player_bullet_active) {
        return false;
    }

    bullet_x = (int)(state->player_bullet_x / 1000);
    bullet_y = (int)(state->player_bullet_y / 1000);

    for (int row = 0; row < state->formation_rows; ++row) {
        for (int col = 0; col < state->formation_cols; ++col) {
            int invader_left;
            int invader_top;

            if (!state->invaders[row][col]) {
                continue;
            }

            invader_left = (int)(state->formation_x / 1000) + (col * INVADER_SPACING_X);
            invader_top = state->formation_y + (row * INVADER_SPACING_Y);
            if (!invader_rect_overlap(bullet_x - (INVADER_PLAYER_BULLET_W / 2),
                                      bullet_y - (INVADER_PLAYER_BULLET_H / 2),
                                      bullet_x + (INVADER_PLAYER_BULLET_W / 2),
                                      bullet_y + (INVADER_PLAYER_BULLET_H / 2),
                                      invader_left,
                                      invader_top,
                                      invader_left + INVADER_SPRITE_W,
                                      invader_top + INVADER_SPRITE_H)) {
                continue;
            }

            state->invaders[row][col] = false;
            state->player_bullet_active = false;
            state->score++;
            state->remaining_invaders--;
            return true;
        }
    }

    return false;
}

static bool invader_hit_ufo(invader_state_t *state, uint32_t elapsed_ms, uint32_t *random_state)
{
    int bullet_x;
    int bullet_y;
    int ufo_x;

    if (!state->player_bullet_active || !state->ufo.active) {
        return false;
    }

    bullet_x = (int)(state->player_bullet_x / 1000);
    bullet_y = (int)(state->player_bullet_y / 1000);
    ufo_x = (int)(state->ufo.x / 1000);

    if (!invader_rect_overlap(bullet_x - (INVADER_PLAYER_BULLET_W / 2),
                              bullet_y - (INVADER_PLAYER_BULLET_H / 2),
                              bullet_x + (INVADER_PLAYER_BULLET_W / 2),
                              bullet_y + (INVADER_PLAYER_BULLET_H / 2),
                              ufo_x,
                              INVADER_UFO_Y - (INVADER_UFO_H / 2),
                              ufo_x + INVADER_UFO_W,
                              INVADER_UFO_Y + (INVADER_UFO_H / 2))) {
        return false;
    }

    state->player_bullet_active = false;
    state->score += INVADER_UFO_POINTS;
    invader_schedule_next_ufo(state, elapsed_ms, random_state);
    return true;
}

static bool invader_damage_bunkers_rect(invader_state_t *state,
                                        int left,
                                        int top,
                                        int right,
                                        int bottom)
{
    bool damaged = false;

    if (!state->bunkers_active) {
        return false;
    }

    for (int bunker = 0; bunker < INVADER_BUNKER_COUNT; ++bunker) {
        int bunker_left = invader_bunker_left(bunker);
        int bunker_top = INVADER_BUNKER_Y;
        int bunker_right = bunker_left + invader_bunker_pixel_width();
        int bunker_bottom = bunker_top + invader_bunker_pixel_height();

        if (!invader_rect_overlap(left, top, right, bottom,
                                  bunker_left, bunker_top, bunker_right, bunker_bottom)) {
            continue;
        }

        for (int row = 0; row < INVADER_BUNKER_GRID_H; ++row) {
            for (int col = 0; col < INVADER_BUNKER_GRID_W; ++col) {
                int cell_left;
                int cell_top;

                if (!state->bunkers[bunker][row][col]) {
                    continue;
                }

                cell_left = bunker_left + (col * INVADER_BUNKER_CELL_SIZE);
                cell_top = bunker_top + (row * INVADER_BUNKER_CELL_SIZE);
                if (!invader_rect_overlap(left, top, right, bottom,
                                          cell_left, cell_top,
                                          cell_left + INVADER_BUNKER_CELL_SIZE,
                                          cell_top + INVADER_BUNKER_CELL_SIZE)) {
                    continue;
                }

                state->bunkers[bunker][row][col] = false;
                damaged = true;
            }
        }
    }

    return damaged;
}

static void invader_damage_bunkers_by_invaders(invader_state_t *state)
{
    if (!state->bunkers_active) {
        return;
    }

    if (state->formation_y + invader_formation_height(state) < INVADER_BUNKER_Y) {
        return;
    }

    for (int row = 0; row < state->formation_rows; ++row) {
        for (int col = 0; col < state->formation_cols; ++col) {
            int invader_left;
            int invader_top;

            if (!state->invaders[row][col]) {
                continue;
            }

            invader_left = (int)(state->formation_x / 1000) + (col * INVADER_SPACING_X);
            invader_top = state->formation_y + (row * INVADER_SPACING_Y);
            (void)invader_damage_bunkers_rect(state,
                                              invader_left,
                                              invader_top,
                                              invader_left + INVADER_SPRITE_W,
                                              invader_top + INVADER_SPRITE_H);
        }
    }
}

static void invader_update_ufo(invader_state_t *state,
                               uint32_t delta_ms,
                               uint32_t elapsed_ms,
                               uint32_t *random_state)
{
    if (!state->ufo.active) {
        if (elapsed_ms >= state->next_ufo_spawn_ms) {
            invader_spawn_ufo(state, random_state);
        }
        return;
    }

    state->ufo.x += state->ufo.dir * INVADER_UFO_SPEED * (int32_t)delta_ms;
    if ((state->ufo.dir > 0 && (int)(state->ufo.x / 1000) > LCD_WIDTH) ||
        (state->ufo.dir < 0 && ((int)(state->ufo.x / 1000) + INVADER_UFO_W) < 0)) {
        invader_schedule_next_ufo(state, elapsed_ms, random_state);
    }
}

static void invader_update_player_bullet(invader_state_t *state,
                                         uint32_t delta_ms,
                                         uint32_t elapsed_ms,
                                         uint32_t *random_state)
{
    if (!state->player_bullet_active && elapsed_ms >= state->next_player_shot_ms) {
        state->player_bullet_active = true;
        state->player_bullet_x = state->player_x * 1000;
        state->player_bullet_y = (INVADER_PLAYER_Y - INVADER_PLAYER_H) * 1000;
        state->next_player_shot_ms = elapsed_ms + invader_player_shot_interval_ms(state);
    }

    if (!state->player_bullet_active) {
        return;
    }

    state->player_bullet_y -= INVADER_PLAYER_BULLET_SPEED * (int32_t)delta_ms;
    if ((int)(state->player_bullet_y / 1000) + INVADER_PLAYER_BULLET_H < 0) {
        state->player_bullet_active = false;
        return;
    }

    if (invader_damage_bunkers_rect(state,
                                    (int)(state->player_bullet_x / 1000) - 4,
                                    (int)(state->player_bullet_y / 1000) - 6,
                                    (int)(state->player_bullet_x / 1000) + 4,
                                    (int)(state->player_bullet_y / 1000) + 2)) {
        state->player_bullet_active = false;
        return;
    }

    if (invader_hit_ufo(state, elapsed_ms, random_state)) {
        return;
    }

    if (invader_hit_enemy(state) && state->remaining_invaders == 0u) {
        invader_start_wave(state, elapsed_ms);
    }
}

static bool invader_spawn_enemy_bullet(invader_state_t *state, uint32_t *random_state)
{
    int start_col = (int)(app_random_next(random_state) % (uint32_t)state->formation_cols);

    for (int offset = 0; offset < state->formation_cols; ++offset) {
        int col = (start_col + offset) % state->formation_cols;

        for (int row = state->formation_rows - 1; row >= 0; --row) {
            if (state->invaders[row][col]) {
                for (size_t i = 0; i < ARRAY_SIZE(state->enemy_bullets); ++i) {
                    if (!state->enemy_bullets[i].active) {
                        state->enemy_bullets[i].active = true;
                        state->enemy_bullets[i].x = (((int)(state->formation_x / 1000) +
                                                      (col * INVADER_SPACING_X) +
                                                      (INVADER_SPRITE_W / 2)) * 1000);
                        state->enemy_bullets[i].y = ((state->formation_y +
                                                      (row * INVADER_SPACING_Y) +
                                                      INVADER_SPRITE_H + 2) * 1000);
                        state->enemy_bullets[i].speed = invader_enemy_bullet_speed(state);
                        return true;
                    }
                }
                return false;
            }
        }
    }

    return false;
}

static bool invader_update_enemy_bullets(invader_state_t *state,
                                         uint32_t delta_ms,
                                         uint32_t elapsed_ms,
                                         uint32_t *random_state)
{
    int player_left = state->player_x - (INVADER_PLAYER_W / 2);
    int player_top = INVADER_PLAYER_Y - (INVADER_PLAYER_H / 2);

    if (elapsed_ms >= state->next_enemy_shot_ms) {
        (void)invader_spawn_enemy_bullet(state, random_state);
        state->next_enemy_shot_ms = elapsed_ms + invader_enemy_shot_interval_ms(state);
    }

    for (size_t i = 0; i < ARRAY_SIZE(state->enemy_bullets); ++i) {
        int bullet_x;
        int bullet_y;

        if (!state->enemy_bullets[i].active) {
            continue;
        }

        state->enemy_bullets[i].y += state->enemy_bullets[i].speed * (int32_t)delta_ms;
        bullet_x = (int)(state->enemy_bullets[i].x / 1000);
        bullet_y = (int)(state->enemy_bullets[i].y / 1000);

        if (bullet_y - (INVADER_ENEMY_BULLET_H / 2) > LCD_HEIGHT) {
            state->enemy_bullets[i].active = false;
            continue;
        }

        if (invader_damage_bunkers_rect(state,
                                        bullet_x - 4,
                                        bullet_y - 4,
                                        bullet_x + 4,
                                        bullet_y + 6)) {
            state->enemy_bullets[i].active = false;
            continue;
        }

        if (invader_rect_overlap(bullet_x - (INVADER_ENEMY_BULLET_W / 2),
                                 bullet_y - (INVADER_ENEMY_BULLET_H / 2),
                                 bullet_x + (INVADER_ENEMY_BULLET_W / 2),
                                 bullet_y + (INVADER_ENEMY_BULLET_H / 2),
                                 player_left,
                                 player_top,
                                 player_left + INVADER_PLAYER_W,
                                 player_top + INVADER_PLAYER_H)) {
            return false;
        }
    }

    return true;
}

static uint16_t invader_enemy_color_for_row(int row)
{
    if (row == 0) {
        return INVADER_COLOR_BLUE;
    }
    if (row == 1) {
        return COLOR_RED;
    }
    if (row == 2) {
        return INVADER_COLOR_GREEN;
    }
    return COLOR_WHITE;
}

static void invader_draw_scene(const invader_state_t *state)
{
    char score_text[16];

    invader_format_score(score_text, sizeof(score_text), state->score);

    app_fill_screen(COLOR_BLACK);
    app_draw_text_centered(LCD_WIDTH / 2, INVADER_HUD_LABEL_Y, "SCORE", 2, COLOR_WHITE);
    app_draw_text_centered(LCD_WIDTH / 2, INVADER_HUD_VALUE_Y, score_text, 4, COLOR_WHITE);

    for (int row = 0; row < state->formation_rows; ++row) {
        for (int col = 0; col < state->formation_cols; ++col) {
            int x;
            int y;
            uint16_t color;

            if (!state->invaders[row][col]) {
                continue;
            }

            x = (int)(state->formation_x / 1000) + (col * INVADER_SPACING_X) + (INVADER_SPRITE_W / 2);
            y = state->formation_y + (row * INVADER_SPACING_Y) + (INVADER_SPRITE_H / 2);
            color = invader_enemy_color_for_row(row);
            app_draw_bitmap_mask_centered(x, y, INVADER_SPRITE_W, INVADER_SPRITE_H, invader_enemy_rows, color);
        }
    }

    if (state->ufo.active) {
        int ufo_center_x = (int)(state->ufo.x / 1000) + (INVADER_UFO_W / 2);

        app_draw_bitmap_mask_centered(ufo_center_x, INVADER_UFO_Y,
                                      INVADER_UFO_W, INVADER_UFO_H, invader_ufo_rows, COLOR_WHITE);
        app_fill_rect(ufo_center_x - 7, INVADER_UFO_Y - 1, 14, 2, INVADER_COLOR_BLUE);
        app_fill_rect(ufo_center_x - 4, INVADER_UFO_Y + 2, 8, 2, INVADER_COLOR_GREEN);
    }

    if (state->bunkers_active) {
        for (int bunker = 0; bunker < INVADER_BUNKER_COUNT; ++bunker) {
            int bunker_left = invader_bunker_left(bunker);

            for (int row = 0; row < INVADER_BUNKER_GRID_H; ++row) {
                for (int col = 0; col < INVADER_BUNKER_GRID_W; ++col) {
                    if (!state->bunkers[bunker][row][col]) {
                        continue;
                    }

                    app_fill_rect(bunker_left + (col * INVADER_BUNKER_CELL_SIZE),
                                  INVADER_BUNKER_Y + (row * INVADER_BUNKER_CELL_SIZE),
                                  INVADER_BUNKER_CELL_SIZE,
                                  INVADER_BUNKER_CELL_SIZE,
                                  INVADER_COLOR_GREEN);
                }
            }
        }
    }

    if (state->player_bullet_active) {
        app_fill_rect((int)(state->player_bullet_x / 1000) - (INVADER_PLAYER_BULLET_W / 2),
                      (int)(state->player_bullet_y / 1000) - (INVADER_PLAYER_BULLET_H / 2),
                      INVADER_PLAYER_BULLET_W,
                      INVADER_PLAYER_BULLET_H,
                      COLOR_WHITE);
    }

    for (size_t i = 0; i < ARRAY_SIZE(state->enemy_bullets); ++i) {
        if (state->enemy_bullets[i].active) {
            app_fill_rect((int)(state->enemy_bullets[i].x / 1000) - (INVADER_ENEMY_BULLET_W / 2),
                          (int)(state->enemy_bullets[i].y / 1000) - (INVADER_ENEMY_BULLET_H / 2),
                          INVADER_ENEMY_BULLET_W,
                          INVADER_ENEMY_BULLET_H,
                          COLOR_RED);
        }
    }

    app_draw_bitmap_mask_centered(state->player_x, INVADER_PLAYER_Y,
                                  11, 8, invader_player_rows, COLOR_WHITE);
    app_draw_text_centered(58, INVADER_CONTROL_Y, "LEFT", 2, COLOR_WHITE);
    app_draw_text_centered(LCD_WIDTH - 58, INVADER_CONTROL_Y, "RIGHT", 2, COLOR_WHITE);
    app_present_frame();
}

void game_8_invader_get_best_record(char *buffer, size_t buffer_size)
{
    invader_load_best();

    if (best_score == 0u) {
        snprintf(buffer, buffer_size, "-");
        return;
    }

    invader_format_score(buffer, buffer_size, best_score);
}

void run_game_8_invader(game_run_context_t *context)
{
    invader_load_best();

    while (true) {
        touch_event_t touch;
        invader_state_t state;
        absolute_time_t round_start;
        absolute_time_t previous_frame;
        char result_text[16];
        char best_text[16];
        app_result_view_t result_view;

        (void)game_consume_first_start(context);
        app_wait_for_touch_release();
        hit20_show_start_count_in();

        invader_init_round(&state);
        round_start = get_absolute_time();
        previous_frame = round_start;

        while (true) {
            absolute_time_t frame_start = get_absolute_time();
            int64_t delta_us = absolute_time_diff_us(previous_frame, frame_start);
            int64_t elapsed_us = absolute_time_diff_us(round_start, frame_start);
            uint32_t delta_ms = delta_us <= 0 ? 1u : (uint32_t)(delta_us / 1000);
            uint32_t elapsed_ms = elapsed_us <= 0 ? 0u : (uint32_t)(elapsed_us / 1000);
            int move_dir = 0;

            if (delta_ms > 40u) {
                delta_ms = 40u;
            }
            previous_frame = frame_start;

            if (app_read_touch_state(&touch)) {
                move_dir = touch.x < (LCD_WIDTH / 2) ? -1 : 1;
            }

            state.player_x = invader_clamp_player_x(state.player_x +
                                                    ((move_dir * INVADER_PLAYER_SPEED * (int)delta_ms) / 1000));

            invader_move_formation(&state, delta_ms);
            invader_damage_bunkers_by_invaders(&state);
            if (invader_invaders_reached_player(&state)) {
                break;
            }

            invader_update_ufo(&state, delta_ms, elapsed_ms, context->random_state);
            invader_update_player_bullet(&state, delta_ms, elapsed_ms, context->random_state);
            if (!invader_update_enemy_bullets(&state, delta_ms, elapsed_ms, context->random_state)) {
                break;
            }

            invader_draw_scene(&state);
            sleep_until(delayed_by_us(frame_start, INVADER_FRAME_US));
        }

        if (state.score > best_score) {
            best_score = state.score;
            invader_save_best();
        }

        invader_format_score(result_text, sizeof(result_text), state.score);
        invader_format_score(best_text, sizeof(best_text), best_score);
        result_view.game_name = context->game_name;
        result_view.record_label = "SCORE";
        result_view.current_record = result_text;
        result_view.best_record = best_text;
        printf("8_invader: %lu score\n", (unsigned long)state.score);
        app_draw_result_screen(&result_view);

        if (app_wait_post_game_action() == POST_ACTION_MENU) {
            return;
        }
    }
}
