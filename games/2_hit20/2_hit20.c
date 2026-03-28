#include <stdio.h>
#include "pico/stdlib.h"
#include "app.h"
#include "2_hit20.h"

void run_game_2_hit20(game_run_context_t *context)
{
    static uint32_t best_hundredths = 0;

    while (true) {
        bool is_first_start = game_consume_first_start(context);
        int remaining = 20;
        absolute_time_t start_time = nil_time;
        absolute_time_t finish_time = nil_time;
        touch_event_t event;
        char label[16];
        char result_text[16];
        char best_text[16];
        app_result_view_t result_view;

        (void)is_first_start;

        app_wait_for_touch_release();

        while (remaining > 0) {
            snprintf(label, sizeof(label), "%d", remaining);
            app_fill_screen(COLOR_BLACK);
            app_draw_text_centered(LCD_WIDTH / 2, 80, label, remaining >= 10 ? 16 : 24, COLOR_WHITE);
            app_present_frame();

            while (!app_poll_touch_event(&event)) {
                sleep_ms(5);
            }

            if (remaining == 20) {
                start_time = get_absolute_time();
            }
            remaining--;
            finish_time = get_absolute_time();
            app_wait_for_touch_release();
        }

        int64_t elapsed_us = absolute_time_diff_us(start_time, finish_time);
        uint32_t hundredths = (uint32_t)((elapsed_us + 5000) / 10000);
        app_update_best_hundredths(hundredths, &best_hundredths);
        app_format_hundredths(result_text, sizeof(result_text), hundredths);
        app_format_hundredths(best_text, sizeof(best_text), best_hundredths);
        result_view.game_name = context->game_name;
        result_view.current_record = result_text;
        result_view.best_record = best_text;
        printf("2_hit20: %s s\n", result_text);
        app_draw_result_screen(&result_view);

        if (app_wait_post_game_action() == POST_ACTION_MENU) {
            return;
        }
    }
}
