#include <stdio.h>
#include "pico/stdlib.h"
#include "app.h"
#include "assets/1_push_fast/1_push_fast_wav.h"
#include "1_push_fast.h"

static uint32_t best_hundredths = 0;

void game_1_push_fast_get_best_record(char *buffer, size_t buffer_size)
{
    if (best_hundredths == 0) {
        snprintf(buffer, buffer_size, "--.--");
        return;
    }

    app_format_hundredths(buffer, buffer_size, best_hundredths);
}

void run_game_1_push_fast(game_run_context_t *context)
{
    while (true) {
        bool is_first_start = game_consume_first_start(context);
        bool is_reference_record = false;
        touch_event_t event;
        char result_text[16];
        char best_text[16];
        app_result_view_t result_view;

        app_wait_for_touch_release();
        if (is_first_start) {
            app_play_wav(assets_1_push_fast_1_push_fast_wav,
                         assets_1_push_fast_1_push_fast_wav_len);
        }

        for (int count = 3; count >= 1; --count) {
            char digit[2] = {(char)('0' + count), '\0'};
            app_fill_screen(COLOR_BLACK);
            app_draw_text_centered(LCD_WIDTH / 2, 80, digit, 24, COLOR_WHITE);
            app_present_frame();

            absolute_time_t until = make_timeout_time_ms(1000);
            while (absolute_time_diff_us(get_absolute_time(), until) > 0) {
                app_poll_touch_event(&event);
                sleep_ms(10);
            }
        }

        app_fill_screen(COLOR_BLACK);
        app_present_frame();

        uint32_t wait_ms = 1000u + (app_random_next(context->random_state) % 9000u);
        absolute_time_t red_deadline = make_timeout_time_ms((int64_t)wait_ms);
        while (absolute_time_diff_us(get_absolute_time(), red_deadline) > 0) {
            if (app_poll_touch_event(&event)) {
                is_reference_record = true;
                app_draw_black_message("WAIT", NULL);
                sleep_ms(700);
                app_fill_screen(COLOR_BLACK);
                app_present_frame();
            }
            sleep_ms(5);
        }

        app_fill_screen(COLOR_RED);
        app_present_frame();
        absolute_time_t red_time = get_absolute_time();

        while (true) {
            if (app_poll_touch_event(&event)) {
                int64_t reaction_us = absolute_time_diff_us(red_time, get_absolute_time());
                uint32_t hundredths = (uint32_t)((reaction_us + 5000) / 10000);
                if (!is_reference_record) {
                    app_update_best_hundredths(hundredths, &best_hundredths);
                }
                app_format_hundredths(result_text, sizeof(result_text), hundredths);
                if (best_hundredths == 0) {
                    snprintf(best_text, sizeof(best_text), "--.--");
                } else {
                    app_format_hundredths(best_text, sizeof(best_text), best_hundredths);
                }
                result_view.game_name = context->game_name;
                result_view.record_label = is_reference_record ? "REFERENCE" : NULL;
                result_view.current_record = result_text;
                result_view.best_record = best_text;
                printf("1_push_fast%s: %s s\n",
                       is_reference_record ? " reference" : "",
                       result_text);
                app_draw_result_screen(&result_view);
                break;
            }
            sleep_ms(2);
        }

        if (app_wait_post_game_action() == POST_ACTION_MENU) {
            return;
        }
    }
}
