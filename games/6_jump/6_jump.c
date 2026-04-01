#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "app.h"
#include "best_store.h"
#include "2_hit20/2_hit20.h"
#include "6_jump.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define JUMP_FRAME_US 16666
#define JUMP_GROUND_Y 226
#define JUMP_RUNNER_X 116
#define JUMP_RUNNER_FOOT_Y JUMP_GROUND_Y
#define JUMP_RUNNER_HIT_HALF_W 8
#define JUMP_RUNNER_HIT_TOP_OFFSET 40
#define JUMP_RUNNER_HIT_BOTTOM_OFFSET 2
#define JUMP_JUMP_VELOCITY -450
#define JUMP_GRAVITY 1320
#define JUMP_SCROLL_SPEED_START 150
#define JUMP_SCROLL_SPEED_MAX 220
#define JUMP_MAX_HURDLES 5
#define JUMP_HUD_LABEL_Y 14
#define JUMP_HUD_VALUE_Y 36
#define JUMP_TAP_Y 330

typedef struct {
    bool active;
    bool counted;
    int x;
    int width;
    int height;
} jump_hurdle_t;

static uint32_t best_clear = 0;
static bool best_loaded = false;
static const int jump_hurdle_heights[] = {18, 28, 38, 48, 54};
static const uint8_t jump_height_weights[][5] = {
    {52, 28, 13, 5, 2},
    {40, 28, 18, 10, 4},
    {28, 26, 22, 16, 8},
    {18, 22, 24, 22, 14},
    {10, 18, 24, 24, 24},
};

static void jump_load_best(void)
{
    uint32_t stored_value = 0;
    bool has_value = false;

    if (best_loaded) {
        return;
    }
    if (!best_store_load_u32("JUMP", &stored_value, &has_value)) {
        return;
    }

    if (has_value) {
        best_clear = stored_value;
    }
    best_loaded = true;
}

static void jump_save_best(void)
{
    best_loaded = true;
    best_store_save_u32("JUMP", best_clear, best_clear != 0u);
}

static void jump_format_score(char *buffer, size_t buffer_size, uint32_t score)
{
    snprintf(buffer, buffer_size, "%lu", (unsigned long)score);
}

static int jump_scroll_speed(uint32_t elapsed_ms)
{
    int speed = JUMP_SCROLL_SPEED_START + (int)(elapsed_ms / 700u);

    if (speed > JUMP_SCROLL_SPEED_MAX) {
        speed = JUMP_SCROLL_SPEED_MAX;
    }
    return speed;
}

static int jump_pick_hurdle_height(uint32_t elapsed_ms, uint32_t *random_state)
{
    uint32_t stage = elapsed_ms / 10000u;
    uint32_t roll = app_random_next(random_state) % 100u;
    uint32_t accumulated = 0u;

    if (stage >= ARRAY_SIZE(jump_height_weights)) {
        stage = ARRAY_SIZE(jump_height_weights) - 1u;
    }

    for (size_t i = 0; i < ARRAY_SIZE(jump_hurdle_heights); ++i) {
        accumulated += jump_height_weights[stage][i];
        if (roll < accumulated) {
            return jump_hurdle_heights[i];
        }
    }

    return jump_hurdle_heights[ARRAY_SIZE(jump_hurdle_heights) - 1u];
}

static uint32_t jump_min_spawn_gap_ms(int height, int scroll_speed, uint32_t *random_state)
{
    return 820u + ((uint32_t)height * 7u) + ((uint32_t)scroll_speed / 2u) +
           (app_random_next(random_state) % 180u);
}

static void jump_spawn_hurdle(jump_hurdle_t *hurdles, int height)
{
    for (size_t i = 0; i < JUMP_MAX_HURDLES; ++i) {
        if (hurdles[i].active) {
            continue;
        }

        hurdles[i].active = true;
        hurdles[i].counted = false;
        hurdles[i].height = height;
        hurdles[i].width = 12 + (height / 24);
        hurdles[i].x = LCD_WIDTH + 20;
        return;
    }
}

static void jump_draw_runner(int foot_y, bool stride)
{
    int head_y = foot_y - 34;
    int torso_top = foot_y - 26;

    app_draw_filled_circle(JUMP_RUNNER_X, head_y, 7, COLOR_WHITE);
    app_fill_rect(JUMP_RUNNER_X - 5, torso_top, 10, 14, COLOR_WHITE);
    app_fill_rect(JUMP_RUNNER_X - 10, torso_top + 2, 5, 8, COLOR_WHITE);
    app_fill_rect(JUMP_RUNNER_X + 5, torso_top + 2, 5, 8, COLOR_WHITE);

    if (stride) {
        app_fill_rect(JUMP_RUNNER_X - 7, torso_top + 14, 5, 12, COLOR_WHITE);
        app_fill_rect(JUMP_RUNNER_X + 2, torso_top + 14, 5, 8, COLOR_WHITE);
    } else {
        app_fill_rect(JUMP_RUNNER_X - 6, torso_top + 14, 5, 8, COLOR_WHITE);
        app_fill_rect(JUMP_RUNNER_X + 3, torso_top + 14, 5, 12, COLOR_WHITE);
    }
}

static void jump_draw_hurdle(const jump_hurdle_t *hurdle)
{
    int top_y = JUMP_GROUND_Y - hurdle->height;
    int leg_w = 4;
    int bar_h = 4;

    app_fill_rect(hurdle->x, top_y, hurdle->width, bar_h, COLOR_WHITE);
    app_fill_rect(hurdle->x + 2,
                  top_y + bar_h,
                  leg_w,
                  hurdle->height - bar_h,
                  COLOR_WHITE);
    app_fill_rect(hurdle->x + hurdle->width - leg_w - 2,
                  top_y + bar_h,
                  leg_w,
                  hurdle->height - bar_h,
                  COLOR_WHITE);
}

static bool jump_rect_overlap(int left_a, int top_a, int right_a, int bottom_a,
                              int left_b, int top_b, int right_b, int bottom_b)
{
    return left_a < right_b && right_a > left_b &&
           top_a < bottom_b && bottom_a > top_b;
}

static bool jump_update_hurdles(jump_hurdle_t *hurdles,
                                uint32_t delta_ms,
                                int scroll_speed,
                                int runner_feet_y,
                                uint32_t *clear_count)
{
    int runner_left = JUMP_RUNNER_X - JUMP_RUNNER_HIT_HALF_W;
    int runner_right = JUMP_RUNNER_X + JUMP_RUNNER_HIT_HALF_W;
    int runner_top = runner_feet_y - JUMP_RUNNER_HIT_TOP_OFFSET;
    int runner_bottom = runner_feet_y - JUMP_RUNNER_HIT_BOTTOM_OFFSET;

    for (size_t i = 0; i < JUMP_MAX_HURDLES; ++i) {
        int dx;

        if (!hurdles[i].active) {
            continue;
        }

        dx = (scroll_speed * (int)delta_ms) / 1000;
        if (dx < 1) {
            dx = 1;
        }
        hurdles[i].x -= dx;

        if (!hurdles[i].counted && hurdles[i].x + hurdles[i].width < JUMP_RUNNER_X) {
            hurdles[i].counted = true;
            (*clear_count)++;
        }

        if (jump_rect_overlap(runner_left,
                              runner_top,
                              runner_right,
                              runner_bottom,
                              hurdles[i].x,
                              JUMP_GROUND_Y - hurdles[i].height,
                              hurdles[i].x + hurdles[i].width,
                              JUMP_GROUND_Y)) {
            return true;
        }

        if (hurdles[i].x + hurdles[i].width < 0) {
            hurdles[i].active = false;
        }
    }

    return false;
}

static void jump_draw_scene(const jump_hurdle_t *hurdles, int runner_feet_y, uint32_t clear_count, uint32_t elapsed_ms)
{
    char score_text[16];
    int score_scale = clear_count >= 100u ? 4 : 5;
    bool stride = ((elapsed_ms / 120u) % 2u) == 0u;

    jump_format_score(score_text, sizeof(score_text), clear_count);

    app_fill_screen(COLOR_BLACK);
    app_draw_text_centered(LCD_WIDTH / 2, JUMP_HUD_LABEL_Y, "CLEAR", 2, COLOR_WHITE);
    app_draw_text_centered(LCD_WIDTH / 2, JUMP_HUD_VALUE_Y, score_text, score_scale, COLOR_WHITE);

    for (size_t i = 0; i < JUMP_MAX_HURDLES; ++i) {
        if (hurdles[i].active) {
            jump_draw_hurdle(&hurdles[i]);
        }
    }

    app_fill_rect(0, JUMP_GROUND_Y, LCD_WIDTH, 2, COLOR_WHITE);
    jump_draw_runner(runner_feet_y, stride);
    app_draw_text_centered(LCD_WIDTH / 2, JUMP_TAP_Y, "TAP", 2, COLOR_WHITE);
    app_present_frame();
}

void game_6_jump_get_best_record(char *buffer, size_t buffer_size)
{
    jump_load_best();

    if (best_clear == 0u) {
        snprintf(buffer, buffer_size, "-");
        return;
    }

    jump_format_score(buffer, buffer_size, best_clear);
}

void run_game_6_jump(game_run_context_t *context)
{
    jump_load_best();

    while (true) {
        touch_event_t event;
        jump_hurdle_t hurdles[JUMP_MAX_HURDLES];
        absolute_time_t start_time;
        absolute_time_t previous_frame;
        uint32_t next_spawn_ms = 1100u;
        uint32_t clear_count = 0u;
        int runner_feet_y = JUMP_RUNNER_FOOT_Y;
        int runner_vy = 0;
        char result_text[16];
        char best_text[16];
        app_result_view_t result_view;

        (void)game_consume_first_start(context);
        memset(hurdles, 0, sizeof(hurdles));

        app_wait_for_touch_release();
        hit20_show_start_count_in();

        start_time = get_absolute_time();
        previous_frame = start_time;

        while (true) {
            absolute_time_t frame_start = get_absolute_time();
            int64_t elapsed_us = absolute_time_diff_us(start_time, frame_start);
            int64_t delta_us = absolute_time_diff_us(previous_frame, frame_start);
            uint32_t elapsed_ms = elapsed_us <= 0 ? 0u : (uint32_t)(elapsed_us / 1000);
            uint32_t delta_ms = delta_us <= 0 ? 1u : (uint32_t)(delta_us / 1000);
            int scroll_speed = jump_scroll_speed(elapsed_ms);

            if (delta_ms > 40u) {
                delta_ms = 40u;
            }
            previous_frame = frame_start;

            if (app_poll_touch_event(&event) && runner_feet_y >= JUMP_RUNNER_FOOT_Y) {
                runner_vy = JUMP_JUMP_VELOCITY;
            }

            runner_vy += (JUMP_GRAVITY * (int)delta_ms) / 1000;
            runner_feet_y += (runner_vy * (int)delta_ms) / 1000;
            if (runner_feet_y > JUMP_RUNNER_FOOT_Y) {
                runner_feet_y = JUMP_RUNNER_FOOT_Y;
                runner_vy = 0;
            }

            if (elapsed_ms >= next_spawn_ms) {
                int height = jump_pick_hurdle_height(elapsed_ms, context->random_state);

                jump_spawn_hurdle(hurdles, height);
                next_spawn_ms = elapsed_ms + jump_min_spawn_gap_ms(height,
                                                                   scroll_speed,
                                                                   context->random_state);
            }

            if (jump_update_hurdles(hurdles, delta_ms, scroll_speed, runner_feet_y, &clear_count)) {
                break;
            }

            jump_draw_scene(hurdles, runner_feet_y, clear_count, elapsed_ms);
            sleep_until(delayed_by_us(frame_start, JUMP_FRAME_US));
        }

        if (clear_count > best_clear) {
            best_clear = clear_count;
            jump_save_best();
        }

        jump_format_score(result_text, sizeof(result_text), clear_count);
        jump_format_score(best_text, sizeof(best_text), best_clear);
        result_view.game_name = context->game_name;
        result_view.record_label = "CLEAR";
        result_view.current_record = result_text;
        result_view.best_record = best_text;
        printf("6_jump: %lu clear\n", (unsigned long)clear_count);
        app_draw_result_screen(&result_view);

        if (app_wait_post_game_action() == POST_ACTION_MENU) {
            return;
        }
    }
}
