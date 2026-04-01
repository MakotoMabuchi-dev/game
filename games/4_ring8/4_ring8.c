#include <stdio.h>
#include "pico/stdlib.h"
#include "app.h"
#include "best_store.h"
#include "2_hit20/2_hit20.h"
#include "4_ring8.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define RING8_BUTTON_COUNT 8
#define RING8_CENTER_X (LCD_WIDTH / 2)
#define RING8_CENTER_Y (LCD_HEIGHT / 2)
#define RING8_BUTTON_RADIUS 26
#define RING8_BUTTON_HIT_RADIUS 32
#define RING8_BUTTON_BORDER 4
#define RING8_INFO_SCALE 3
#define RING8_SCORE_Y (RING8_CENTER_Y - 12)
#define RING8_TIME_Y (RING8_SCORE_Y - 36)
#define RING8_ROUND_MS 20000u
#define RING8_FRAME_MS 16u
#define RING8_NO_BUTTON 0xFFu

typedef struct {
    int x;
    int y;
} ring8_button_t;

static uint32_t best_hits = 0;
static bool best_loaded = false;

static const ring8_button_t ring8_buttons[RING8_BUTTON_COUNT] = {
    {RING8_CENTER_X, 44},
    {275, 85},
    {316, RING8_CENTER_Y},
    {275, 275},
    {RING8_CENTER_X, 316},
    {85, 275},
    {44, RING8_CENTER_Y},
    {85, 85},
};

static void ring8_load_best(void)
{
    uint32_t stored_value = 0;
    bool has_value = false;

    if (best_loaded) {
        return;
    }
    if (!best_store_load_u32("RING8", &stored_value, &has_value)) {
        return;
    }

    if (has_value) {
        best_hits = stored_value;
    }
    best_loaded = true;
}

static void ring8_save_best(void)
{
    best_loaded = true;
    best_store_save_u32("RING8", best_hits, best_hits != 0u);
}

static bool ring8_point_in_circle(uint16_t x, uint16_t y, int center_x, int center_y, int radius)
{
    int dx = (int)x - center_x;
    int dy = (int)y - center_y;

    return (dx * dx) + (dy * dy) <= (radius * radius);
}

static uint8_t ring8_pick_next_target(uint8_t current_index, uint32_t *random_state)
{
    if (current_index >= RING8_BUTTON_COUNT) {
        return (uint8_t)(app_random_next(random_state) % RING8_BUTTON_COUNT);
    }

    return (uint8_t)((current_index + 1u + (app_random_next(random_state) % (RING8_BUTTON_COUNT - 1u))) %
                     RING8_BUTTON_COUNT);
}

static int ring8_find_touched_button(const touch_event_t *event)
{
    for (size_t i = 0; i < ARRAY_SIZE(ring8_buttons); ++i) {
        if (ring8_point_in_circle(event->x, event->y,
                                  ring8_buttons[i].x, ring8_buttons[i].y,
                                  RING8_BUTTON_HIT_RADIUS)) {
            return (int)i;
        }
    }

    return -1;
}

static void ring8_draw_button(const ring8_button_t *button, bool is_active)
{
    app_draw_filled_circle(button->x, button->y, RING8_BUTTON_RADIUS, COLOR_WHITE);
    app_draw_filled_circle(button->x, button->y,
                           RING8_BUTTON_RADIUS - RING8_BUTTON_BORDER,
                           is_active ? COLOR_RED : COLOR_BLACK);
}

static void ring8_draw_scene(uint32_t hits, uint32_t seconds_left, uint8_t active_index)
{
    char score_text[16];
    char time_text[16];

    snprintf(score_text, sizeof(score_text), "SCORE:%lu", (unsigned long)hits);
    snprintf(time_text, sizeof(time_text), "TIME:%lu", (unsigned long)seconds_left);

    app_fill_screen(COLOR_BLACK);
    for (size_t i = 0; i < ARRAY_SIZE(ring8_buttons); ++i) {
        ring8_draw_button(&ring8_buttons[i], i == active_index);
    }
    app_draw_text_centered(RING8_CENTER_X, RING8_TIME_Y, time_text, RING8_INFO_SCALE, COLOR_WHITE);
    app_draw_text_centered(RING8_CENTER_X, RING8_SCORE_Y, score_text, RING8_INFO_SCALE, COLOR_WHITE);
    app_present_frame();
}

void game_4_ring8_get_best_record(char *buffer, size_t buffer_size)
{
    ring8_load_best();

    if (best_hits == 0) {
        snprintf(buffer, buffer_size, "-");
        return;
    }

    snprintf(buffer, buffer_size, "%lu", (unsigned long)best_hits);
}

void run_game_4_ring8(game_run_context_t *context)
{
    ring8_load_best();

    while (true) {
        touch_event_t event;
        uint32_t hits = 0;
        uint8_t active_index;
        char result_text[16];
        char best_text[16];
        app_result_view_t result_view;
        absolute_time_t round_deadline;

        (void)game_consume_first_start(context);
        app_wait_for_touch_release();
        hit20_show_start_count_in();

        active_index = ring8_pick_next_target(RING8_NO_BUTTON, context->random_state);
        round_deadline = make_timeout_time_ms(RING8_ROUND_MS);

        while (true) {
            absolute_time_t now = get_absolute_time();
            int64_t remaining_us = absolute_time_diff_us(now, round_deadline);
            uint32_t seconds_left;

            if (remaining_us <= 0) {
                break;
            }

            seconds_left = (uint32_t)((remaining_us + 999999) / 1000000);
            if (app_poll_touch_event(&event)) {
                int touched_index = ring8_find_touched_button(&event);

                if (touched_index >= 0) {
                    if (touched_index == (int)active_index) {
                        hits++;
                    }
                    active_index = ring8_pick_next_target(active_index, context->random_state);
                }
            }

            ring8_draw_scene(hits, seconds_left, active_index);
            sleep_ms(RING8_FRAME_MS);
        }

        if (hits > best_hits) {
            best_hits = hits;
            ring8_save_best();
        }

        snprintf(result_text, sizeof(result_text), "%lu", (unsigned long)hits);
        snprintf(best_text, sizeof(best_text), "%lu", (unsigned long)best_hits);
        result_view.game_name = context->game_name;
        result_view.record_label = "HITS";
        result_view.current_record = result_text;
        result_view.best_record = best_text;
        printf("4_ring8: %lu hits\n", (unsigned long)hits);
        app_draw_result_screen(&result_view);

        if (app_wait_post_game_action() == POST_ACTION_MENU) {
            return;
        }
    }
}
