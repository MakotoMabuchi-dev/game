#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "app.h"
#include "best_store.h"
#include "2_hit20/2_hit20.h"
#include "7_block.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define BLOCK_FRAME_US 16666
#define BLOCK_BRICK_ROWS 4
#define BLOCK_BRICK_COLS 7
#define BLOCK_BRICK_W 42
#define BLOCK_BRICK_H 16
#define BLOCK_BRICK_GAP 6
#define BLOCK_BRICK_START_X ((LCD_WIDTH - ((BLOCK_BRICK_COLS * BLOCK_BRICK_W) + ((BLOCK_BRICK_COLS - 1) * BLOCK_BRICK_GAP))) / 2)
#define BLOCK_BRICK_START_Y 56
#define BLOCK_PADDLE_W 68
#define BLOCK_PADDLE_H 10
#define BLOCK_PADDLE_Y 304
#define BLOCK_PADDLE_MIN_X ((BLOCK_PADDLE_W / 2) + 10)
#define BLOCK_PADDLE_MAX_X (LCD_WIDTH - BLOCK_PADDLE_MIN_X)
#define BLOCK_PADDLE_MOVE_SPEED 640
#define BLOCK_PADDLE_DEAD_ZONE 10
#define BLOCK_BALL_RADIUS 5
#define BLOCK_BALL_START_Y (BLOCK_PADDLE_Y - 18)
#define BLOCK_HUD_LABEL_Y 14
#define BLOCK_HUD_VALUE_Y 36

typedef struct {
    bool bricks[BLOCK_BRICK_ROWS][BLOCK_BRICK_COLS];
    uint32_t score;
    uint32_t wave;
    uint32_t remaining_bricks;
    int paddle_x;
    int32_t ball_x;
    int32_t ball_y;
    int ball_vx;
    int ball_vy;
} block_state_t;

static uint32_t best_score = 0;
static bool best_loaded = false;

static void block_load_best(void)
{
    uint32_t stored_value = 0;
    bool has_value = false;

    if (best_loaded) {
        return;
    }
    if (!best_store_load_u32("BLOCK", &stored_value, &has_value)) {
        return;
    }

    if (has_value) {
        best_score = stored_value;
    }
    best_loaded = true;
}

static void block_save_best(void)
{
    best_loaded = true;
    best_store_save_u32("BLOCK", best_score, best_score != 0u);
}

static void block_format_score(char *buffer, size_t buffer_size, uint32_t score)
{
    snprintf(buffer, buffer_size, "%lu", (unsigned long)score);
}

static int block_clamp_paddle_x(int paddle_x)
{
    if (paddle_x < BLOCK_PADDLE_MIN_X) {
        return BLOCK_PADDLE_MIN_X;
    }
    if (paddle_x > BLOCK_PADDLE_MAX_X) {
        return BLOCK_PADDLE_MAX_X;
    }
    return paddle_x;
}

static uint32_t block_speed_scale_permille(const block_state_t *state)
{
    uint32_t scale = 1000u;
    uint32_t cleared_waves = state->wave > 0u ? state->wave - 1u : 0u;

    for (uint32_t i = 0; i < cleared_waves; ++i) {
        scale = (scale * 110u + 50u) / 100u;
    }

    return scale;
}

static int block_scale_speed(const block_state_t *state, int base_speed)
{
    uint32_t scale = block_speed_scale_permille(state);
    int sign = base_speed < 0 ? -1 : 1;
    uint32_t magnitude = (uint32_t)(base_speed < 0 ? -base_speed : base_speed);

    return sign * (int)((magnitude * scale + 500u) / 1000u);
}

static int block_paddle_direction_from_touch(int paddle_x, const touch_event_t *touch)
{
    int delta = (int)touch->x - paddle_x;

    if (delta > BLOCK_PADDLE_DEAD_ZONE) {
        return 1;
    }
    if (delta < -BLOCK_PADDLE_DEAD_ZONE) {
        return -1;
    }
    return 0;
}

static int block_ball_speed_y(const block_state_t *state)
{
    return block_scale_speed(state, 220);
}

static void block_reset_ball(block_state_t *state, uint32_t *random_state)
{
    int speed_x = 110 + (int)(app_random_next(random_state) % 70u);

    state->ball_x = state->paddle_x * 1000;
    state->ball_y = BLOCK_BALL_START_Y * 1000;
    speed_x = block_scale_speed(state, speed_x);
    state->ball_vx = (app_random_next(random_state) & 1u) == 0u ? -speed_x : speed_x;
    state->ball_vy = -block_ball_speed_y(state);
}

static void block_fill_bricks(block_state_t *state)
{
    state->wave++;
    state->remaining_bricks = BLOCK_BRICK_ROWS * BLOCK_BRICK_COLS;

    for (int row = 0; row < BLOCK_BRICK_ROWS; ++row) {
        for (int col = 0; col < BLOCK_BRICK_COLS; ++col) {
            state->bricks[row][col] = true;
        }
    }
}

static void block_init_round(block_state_t *state, uint32_t *random_state)
{
    memset(state, 0, sizeof(*state));
    state->paddle_x = LCD_WIDTH / 2;
    block_fill_bricks(state);
    block_reset_ball(state, random_state);
}

static void block_draw_bricks(const block_state_t *state)
{
    for (int row = 0; row < BLOCK_BRICK_ROWS; ++row) {
        for (int col = 0; col < BLOCK_BRICK_COLS; ++col) {
            int x;
            int y;
            uint16_t color;

            if (!state->bricks[row][col]) {
                continue;
            }

            x = BLOCK_BRICK_START_X + col * (BLOCK_BRICK_W + BLOCK_BRICK_GAP);
            y = BLOCK_BRICK_START_Y + row * (BLOCK_BRICK_H + BLOCK_BRICK_GAP);
            color = (row & 1) == 0 ? COLOR_WHITE : COLOR_RED;
            app_fill_rect(x, y, BLOCK_BRICK_W, BLOCK_BRICK_H, color);
        }
    }
}

static void block_draw_paddle(const block_state_t *state)
{
    app_fill_rect(state->paddle_x - (BLOCK_PADDLE_W / 2),
                  BLOCK_PADDLE_Y,
                  BLOCK_PADDLE_W,
                  BLOCK_PADDLE_H,
                  COLOR_WHITE);
}

static void block_draw_ball(const block_state_t *state)
{
    app_draw_filled_circle((int)(state->ball_x / 1000),
                           (int)(state->ball_y / 1000),
                           BLOCK_BALL_RADIUS,
                           COLOR_WHITE);
}

static void block_draw_scene(const block_state_t *state)
{
    char score_text[16];

    block_format_score(score_text, sizeof(score_text), state->score);

    app_fill_screen(COLOR_BLACK);
    app_draw_text_centered(LCD_WIDTH / 2, BLOCK_HUD_LABEL_Y, "SCORE", 2, COLOR_WHITE);
    app_draw_text_centered(LCD_WIDTH / 2, BLOCK_HUD_VALUE_Y, score_text, 4, COLOR_WHITE);
    block_draw_bricks(state);
    block_draw_paddle(state);
    block_draw_ball(state);
    app_present_frame();
}

static bool block_rect_overlap(int left_a, int top_a, int right_a, int bottom_a,
                               int left_b, int top_b, int right_b, int bottom_b)
{
    return left_a < right_b && right_a > left_b &&
           top_a < bottom_b && bottom_a > top_b;
}

static int block_abs(int value)
{
    return value < 0 ? -value : value;
}

static bool block_hit_brick(block_state_t *state)
{
    int ball_x = (int)(state->ball_x / 1000);
    int ball_y = (int)(state->ball_y / 1000);
    int ball_left = ball_x - BLOCK_BALL_RADIUS;
    int ball_right = ball_x + BLOCK_BALL_RADIUS;
    int ball_top = ball_y - BLOCK_BALL_RADIUS;
    int ball_bottom = ball_y + BLOCK_BALL_RADIUS;

    for (int row = 0; row < BLOCK_BRICK_ROWS; ++row) {
        for (int col = 0; col < BLOCK_BRICK_COLS; ++col) {
            int brick_left;
            int brick_top;
            int brick_right;
            int brick_bottom;
            int overlap_left;
            int overlap_right;
            int overlap_top;
            int overlap_bottom;
            int min_x;
            int min_y;

            if (!state->bricks[row][col]) {
                continue;
            }

            brick_left = BLOCK_BRICK_START_X + col * (BLOCK_BRICK_W + BLOCK_BRICK_GAP);
            brick_top = BLOCK_BRICK_START_Y + row * (BLOCK_BRICK_H + BLOCK_BRICK_GAP);
            brick_right = brick_left + BLOCK_BRICK_W;
            brick_bottom = brick_top + BLOCK_BRICK_H;

            if (!block_rect_overlap(ball_left, ball_top, ball_right, ball_bottom,
                                    brick_left, brick_top, brick_right, brick_bottom)) {
                continue;
            }

            state->bricks[row][col] = false;
            state->score++;
            state->remaining_bricks--;

            overlap_left = ball_right - brick_left;
            overlap_right = brick_right - ball_left;
            overlap_top = ball_bottom - brick_top;
            overlap_bottom = brick_bottom - ball_top;
            min_x = overlap_left < overlap_right ? overlap_left : overlap_right;
            min_y = overlap_top < overlap_bottom ? overlap_top : overlap_bottom;

            if (min_x < min_y) {
                if (ball_x < (brick_left + brick_right) / 2) {
                    state->ball_x = (brick_left - BLOCK_BALL_RADIUS - 1) * 1000;
                } else {
                    state->ball_x = (brick_right + BLOCK_BALL_RADIUS + 1) * 1000;
                }
                state->ball_vx = -state->ball_vx;
            } else {
                if (ball_y < (brick_top + brick_bottom) / 2) {
                    state->ball_y = (brick_top - BLOCK_BALL_RADIUS - 1) * 1000;
                } else {
                    state->ball_y = (brick_bottom + BLOCK_BALL_RADIUS + 1) * 1000;
                }
                state->ball_vy = -state->ball_vy;
            }

            return true;
        }
    }

    return false;
}

static bool block_step_ball(block_state_t *state, uint32_t *random_state, uint32_t delta_ms)
{
    int ball_x;
    int ball_y;
    int paddle_left;
    int paddle_right;
    int paddle_top = BLOCK_PADDLE_Y;
    int paddle_bottom = BLOCK_PADDLE_Y + BLOCK_PADDLE_H;

    state->ball_x += state->ball_vx * (int32_t)delta_ms;
    state->ball_y += state->ball_vy * (int32_t)delta_ms;
    ball_x = (int)(state->ball_x / 1000);
    ball_y = (int)(state->ball_y / 1000);

    if (ball_x - BLOCK_BALL_RADIUS <= 0) {
        state->ball_x = (BLOCK_BALL_RADIUS + 1) * 1000;
        state->ball_vx = block_abs(state->ball_vx);
        ball_x = (int)(state->ball_x / 1000);
    } else if (ball_x + BLOCK_BALL_RADIUS >= LCD_WIDTH - 1) {
        state->ball_x = (LCD_WIDTH - BLOCK_BALL_RADIUS - 2) * 1000;
        state->ball_vx = -block_abs(state->ball_vx);
        ball_x = (int)(state->ball_x / 1000);
    }

    if (ball_y - BLOCK_BALL_RADIUS <= 0) {
        state->ball_y = (BLOCK_BALL_RADIUS + 1) * 1000;
        state->ball_vy = block_abs(state->ball_vy);
        ball_y = (int)(state->ball_y / 1000);
    } else if (ball_y - BLOCK_BALL_RADIUS > LCD_HEIGHT) {
        return false;
    }

    paddle_left = state->paddle_x - (BLOCK_PADDLE_W / 2);
    paddle_right = state->paddle_x + (BLOCK_PADDLE_W / 2);
    if (state->ball_vy > 0 &&
        block_rect_overlap(ball_x - BLOCK_BALL_RADIUS,
                           ball_y - BLOCK_BALL_RADIUS,
                           ball_x + BLOCK_BALL_RADIUS,
                           ball_y + BLOCK_BALL_RADIUS,
                           paddle_left,
                           paddle_top,
                           paddle_right,
                           paddle_bottom)) {
        int relative = ball_x - state->paddle_x;
        int new_vx = block_scale_speed(state, relative * 7);
        int max_vx = block_scale_speed(state, 240);
        int min_vx = block_scale_speed(state, 70);

        if (new_vx > max_vx) {
            new_vx = max_vx;
        } else if (new_vx < -max_vx) {
            new_vx = -max_vx;
        } else if (new_vx >= 0 && new_vx < min_vx) {
            new_vx = min_vx;
        } else if (new_vx < 0 && new_vx > -min_vx) {
            new_vx = -min_vx;
        }

        state->ball_x = ball_x * 1000;
        state->ball_y = (paddle_top - BLOCK_BALL_RADIUS - 1) * 1000;
        state->ball_vx = new_vx;
        state->ball_vy = -block_ball_speed_y(state);
    }

    if (block_hit_brick(state) && state->remaining_bricks == 0u) {
        block_fill_bricks(state);
        block_reset_ball(state, random_state);
    }

    return true;
}

void game_7_block_get_best_record(char *buffer, size_t buffer_size)
{
    block_load_best();

    if (best_score == 0u) {
        snprintf(buffer, buffer_size, "-");
        return;
    }

    block_format_score(buffer, buffer_size, best_score);
}

void run_game_7_block(game_run_context_t *context)
{
    block_load_best();

    while (true) {
        touch_event_t touch;
        block_state_t state;
        absolute_time_t frame_start;
        absolute_time_t previous_frame;
        char result_text[16];
        char best_text[16];
        app_result_view_t result_view;

        (void)game_consume_first_start(context);
        app_wait_for_touch_release();
        hit20_show_start_count_in();

        block_init_round(&state, context->random_state);
        previous_frame = get_absolute_time();

        while (true) {
            absolute_time_t now = get_absolute_time();
            int64_t delta_us = absolute_time_diff_us(previous_frame, now);
            uint32_t delta_ms = delta_us <= 0 ? 1u : (uint32_t)(delta_us / 1000);
            int move_dir = 0;

            if (delta_ms > 40u) {
                delta_ms = 40u;
            }
            previous_frame = now;

            if (app_read_touch_state(&touch)) {
                move_dir = block_paddle_direction_from_touch(state.paddle_x, &touch);
            }
            state.paddle_x = block_clamp_paddle_x(state.paddle_x +
                                                  ((move_dir * BLOCK_PADDLE_MOVE_SPEED * (int)delta_ms) / 1000));

            if (!block_step_ball(&state, context->random_state, delta_ms)) {
                break;
            }

            frame_start = get_absolute_time();
            block_draw_scene(&state);
            sleep_until(delayed_by_us(frame_start, BLOCK_FRAME_US));
        }

        if (state.score > best_score) {
            best_score = state.score;
            block_save_best();
        }

        block_format_score(result_text, sizeof(result_text), state.score);
        block_format_score(best_text, sizeof(best_text), best_score);
        result_view.game_name = context->game_name;
        result_view.record_label = "SCORE";
        result_view.current_record = result_text;
        result_view.best_record = best_text;
        printf("7_block: %lu score\n", (unsigned long)state.score);
        app_draw_result_screen(&result_view);

        if (app_wait_post_game_action() == POST_ACTION_MENU) {
            return;
        }
    }
}
