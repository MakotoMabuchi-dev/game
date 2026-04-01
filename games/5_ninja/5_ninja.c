#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "app.h"
#include "best_store.h"
#include "2_hit20/2_hit20.h"
#include "5_ninja.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define NINJA_FRAME_US 16666
#define NINJA_MAX_WEAPONS 10
#define NINJA_HUD_LABEL_Y 14
#define NINJA_HUD_VALUE_Y 34
#define NINJA_GROUND_Y 320
#define NINJA_HEAD_Y 268
#define NINJA_HEAD_RADIUS 11
#define NINJA_BODY_TOP (NINJA_HEAD_Y + 8)
#define NINJA_BODY_W 24
#define NINJA_BODY_H 20
#define NINJA_ARM_W 6
#define NINJA_ARM_H 12
#define NINJA_LEG_W 6
#define NINJA_LEG_H 14
#define NINJA_MOVE_SPEED 220
#define NINJA_HIT_HALF_W 14
#define NINJA_HIT_TOP (NINJA_HEAD_Y - NINJA_HEAD_RADIUS)
#define NINJA_HIT_BOTTOM (NINJA_BODY_TOP + NINJA_BODY_H + NINJA_LEG_H)
#define NINJA_MIN_X (NINJA_HIT_HALF_W + 8)
#define NINJA_MAX_X (LCD_WIDTH - NINJA_MIN_X)
#define NINJA_CONTROL_Y 334

typedef struct {
    bool active;
    int x;
    int y;
    int size;
    int speed;
} ninja_weapon_t;

static uint32_t best_tenths = 0;
static bool best_loaded = false;

static void ninja_load_best(void)
{
    uint32_t stored_value = 0;
    bool has_value = false;

    if (best_loaded) {
        return;
    }
    if (!best_store_load_u32("NINJA", &stored_value, &has_value)) {
        return;
    }

    if (has_value) {
        best_tenths = stored_value;
    }
    best_loaded = true;
}

static void ninja_save_best(void)
{
    best_loaded = true;
    best_store_save_u32("NINJA", best_tenths, best_tenths != 0u);
}

static void ninja_format_tenths(char *buffer, size_t buffer_size, uint32_t tenths)
{
    snprintf(buffer, buffer_size, "%lu.%01lu",
             (unsigned long)(tenths / 10u),
             (unsigned long)(tenths % 10u));
}

static uint32_t ninja_elapsed_tenths(absolute_time_t start_time, absolute_time_t now)
{
    int64_t elapsed_us = absolute_time_diff_us(start_time, now);
    uint32_t elapsed_ms = elapsed_us <= 0 ? 0u : (uint32_t)(elapsed_us / 1000);

    return (elapsed_ms + 50u) / 100u;
}

static int ninja_clamp_x(int x)
{
    if (x < NINJA_MIN_X) {
        return NINJA_MIN_X;
    }
    if (x > NINJA_MAX_X) {
        return NINJA_MAX_X;
    }
    return x;
}

static uint8_t ninja_max_active_weapons(uint32_t elapsed_ms)
{
    uint32_t count = 2u + (elapsed_ms / 5000u);

    if (count > 8u) {
        count = 8u;
    }
    return (uint8_t)count;
}

static uint32_t ninja_spawn_interval_ms(uint32_t elapsed_ms)
{
    uint32_t interval = 720u;
    uint32_t reduction = (elapsed_ms / 80u) + ((elapsed_ms / 5000u) * 80u);

    if (reduction >= 540u) {
        return 180u;
    }
    interval -= reduction;
    if (interval < 180u) {
        interval = 180u;
    }
    return interval;
}

static int ninja_weapon_size(uint32_t elapsed_ms, uint32_t *random_state)
{
    int stage = (int)(elapsed_ms / 5000u);

    return 18 + (stage * 3) + (int)(app_random_next(random_state) % 5u);
}

static int ninja_weapon_speed(uint32_t elapsed_ms, uint32_t *random_state)
{
    int stage = (int)(elapsed_ms / 4000u);

    return 120 + (stage * 22) + (int)(app_random_next(random_state) % 45u);
}

static size_t ninja_count_active_weapons(const ninja_weapon_t *weapons)
{
    size_t count = 0;

    for (size_t i = 0; i < NINJA_MAX_WEAPONS; ++i) {
        if (weapons[i].active) {
            count++;
        }
    }
    return count;
}

static void ninja_spawn_weapon(ninja_weapon_t *weapons,
                               uint32_t elapsed_ms,
                               uint32_t *random_state)
{
    for (size_t i = 0; i < NINJA_MAX_WEAPONS; ++i) {
        int size;
        int x_min;
        int x_range;

        if (weapons[i].active) {
            continue;
        }

        size = ninja_weapon_size(elapsed_ms, random_state);
        x_min = (size / 2) + 8;
        x_range = LCD_WIDTH - (x_min * 2);
        if (x_range < 1) {
            x_range = 1;
        }

        weapons[i].active = true;
        weapons[i].size = size;
        weapons[i].speed = ninja_weapon_speed(elapsed_ms, random_state);
        weapons[i].x = x_min + (int)(app_random_next(random_state) % (uint32_t)x_range);
        weapons[i].y = -size;
        return;
    }
}

static void ninja_draw_weapon(const ninja_weapon_t *weapon)
{
    int arm = weapon->size;
    int core = arm / 3;
    int hub = core / 2;

    if (core < 4) {
        core = 4;
    }
    if (hub < 2) {
        hub = 2;
    }

    app_fill_rect(weapon->x - (core / 2),
                  weapon->y - (arm / 2),
                  core,
                  arm,
                  COLOR_RED);
    app_fill_rect(weapon->x - (arm / 2),
                  weapon->y - (core / 2),
                  arm,
                  core,
                  COLOR_RED);
    app_draw_filled_circle(weapon->x, weapon->y, hub, COLOR_BLACK);
}

static void ninja_draw_ninja(int ninja_x)
{
    app_draw_filled_circle(ninja_x, NINJA_HEAD_Y, NINJA_HEAD_RADIUS, COLOR_WHITE);
    app_fill_rect(ninja_x - 10, NINJA_HEAD_Y - 3, 20, 6, COLOR_BLACK);
    app_fill_rect(ninja_x - 6, NINJA_HEAD_Y - 1, 3, 2, COLOR_WHITE);
    app_fill_rect(ninja_x + 3, NINJA_HEAD_Y - 1, 3, 2, COLOR_WHITE);

    app_fill_rect(ninja_x - (NINJA_BODY_W / 2),
                  NINJA_BODY_TOP,
                  NINJA_BODY_W,
                  NINJA_BODY_H,
                  COLOR_WHITE);
    app_fill_rect(ninja_x - (NINJA_BODY_W / 2) - NINJA_ARM_W,
                  NINJA_BODY_TOP + 2,
                  NINJA_ARM_W,
                  NINJA_ARM_H,
                  COLOR_WHITE);
    app_fill_rect(ninja_x + (NINJA_BODY_W / 2),
                  NINJA_BODY_TOP + 2,
                  NINJA_ARM_W,
                  NINJA_ARM_H,
                  COLOR_WHITE);
    app_fill_rect(ninja_x - 10,
                  NINJA_BODY_TOP + NINJA_BODY_H,
                  NINJA_LEG_W,
                  NINJA_LEG_H,
                  COLOR_WHITE);
    app_fill_rect(ninja_x + 4,
                  NINJA_BODY_TOP + NINJA_BODY_H,
                  NINJA_LEG_W,
                  NINJA_LEG_H,
                  COLOR_WHITE);
}

static bool ninja_rect_overlap(int left_a, int top_a, int right_a, int bottom_a,
                               int left_b, int top_b, int right_b, int bottom_b)
{
    return left_a < right_b && right_a > left_b &&
           top_a < bottom_b && bottom_a > top_b;
}

static bool ninja_check_collision(const ninja_weapon_t *weapons, int ninja_x)
{
    int ninja_left = ninja_x - NINJA_HIT_HALF_W;
    int ninja_right = ninja_x + NINJA_HIT_HALF_W;

    for (size_t i = 0; i < NINJA_MAX_WEAPONS; ++i) {
        int weapon_half;

        if (!weapons[i].active) {
            continue;
        }

        weapon_half = weapons[i].size / 2;
        if (ninja_rect_overlap(ninja_left,
                               NINJA_HIT_TOP,
                               ninja_right,
                               NINJA_HIT_BOTTOM,
                               weapons[i].x - weapon_half,
                               weapons[i].y - weapon_half,
                               weapons[i].x + weapon_half,
                               weapons[i].y + weapon_half)) {
            return true;
        }
    }

    return false;
}

static void ninja_update_weapons(ninja_weapon_t *weapons, uint32_t delta_ms)
{
    for (size_t i = 0; i < NINJA_MAX_WEAPONS; ++i) {
        int dy;

        if (!weapons[i].active) {
            continue;
        }

        dy = (weapons[i].speed * (int)delta_ms) / 1000;
        if (dy < 1) {
            dy = 1;
        }
        weapons[i].y += dy;

        if (weapons[i].y - (weapons[i].size / 2) > LCD_HEIGHT) {
            weapons[i].active = false;
        }
    }
}

static void ninja_draw_scene(const ninja_weapon_t *weapons, int ninja_x, uint32_t tenths)
{
    char time_text[16];

    ninja_format_tenths(time_text, sizeof(time_text), tenths);

    app_fill_screen(COLOR_BLACK);
    app_draw_text_centered(LCD_WIDTH / 2, NINJA_HUD_LABEL_Y, "TIME", 2, COLOR_WHITE);
    app_draw_text_centered(LCD_WIDTH / 2, NINJA_HUD_VALUE_Y, time_text, 4, COLOR_WHITE);

    for (size_t i = 0; i < NINJA_MAX_WEAPONS; ++i) {
        if (weapons[i].active) {
            ninja_draw_weapon(&weapons[i]);
        }
    }

    app_fill_rect(0, NINJA_GROUND_Y, LCD_WIDTH, 2, COLOR_WHITE);
    ninja_draw_ninja(ninja_x);
    app_draw_text_centered(58, NINJA_CONTROL_Y, "LEFT", 2, COLOR_WHITE);
    app_draw_text_centered(LCD_WIDTH - 58, NINJA_CONTROL_Y, "RIGHT", 2, COLOR_WHITE);
    app_present_frame();
}

void game_5_ninja_get_best_record(char *buffer, size_t buffer_size)
{
    ninja_load_best();

    if (best_tenths == 0u) {
        snprintf(buffer, buffer_size, "--.-");
        return;
    }

    ninja_format_tenths(buffer, buffer_size, best_tenths);
}

void run_game_5_ninja(game_run_context_t *context)
{
    ninja_load_best();

    while (true) {
        touch_event_t touch;
        ninja_weapon_t weapons[NINJA_MAX_WEAPONS];
        absolute_time_t start_time;
        absolute_time_t previous_frame;
        uint32_t next_spawn_ms = 0u;
        uint32_t final_tenths;
        int ninja_x = LCD_WIDTH / 2;
        char result_text[16];
        char best_text[16];
        app_result_view_t result_view;

        (void)game_consume_first_start(context);
        memset(weapons, 0, sizeof(weapons));

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
            int move_dir = 0;

            if (delta_ms > 40u) {
                delta_ms = 40u;
            }
            previous_frame = frame_start;

            if (app_read_touch_state(&touch)) {
                move_dir = touch.x < (LCD_WIDTH / 2) ? -1 : 1;
            }

            ninja_x += (move_dir * NINJA_MOVE_SPEED * (int)delta_ms) / 1000;
            ninja_x = ninja_clamp_x(ninja_x);

            if (ninja_count_active_weapons(weapons) < ninja_max_active_weapons(elapsed_ms) &&
                elapsed_ms >= next_spawn_ms) {
                ninja_spawn_weapon(weapons, elapsed_ms, context->random_state);
                next_spawn_ms = elapsed_ms + ninja_spawn_interval_ms(elapsed_ms);
            }

            ninja_update_weapons(weapons, delta_ms);
            if (ninja_check_collision(weapons, ninja_x)) {
                break;
            }

            ninja_draw_scene(weapons, ninja_x, ninja_elapsed_tenths(start_time, frame_start));
            sleep_until(delayed_by_us(frame_start, NINJA_FRAME_US));
        }

        final_tenths = ninja_elapsed_tenths(start_time, get_absolute_time());
        if (final_tenths > best_tenths) {
            best_tenths = final_tenths;
            ninja_save_best();
        }

        ninja_format_tenths(result_text, sizeof(result_text), final_tenths);
        ninja_format_tenths(best_text, sizeof(best_text), best_tenths);
        result_view.game_name = context->game_name;
        result_view.record_label = "TIME";
        result_view.current_record = result_text;
        result_view.best_record = best_text;
        printf("5_ninja: %s s\n", result_text);
        app_draw_result_screen(&result_view);

        if (app_wait_post_game_action() == POST_ACTION_MENU) {
            return;
        }
    }
}
