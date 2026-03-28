#include "pico/stdlib.h"
#include "app.h"
#include "games/games.h"

int main(void)
{
    uint32_t random_state = (uint32_t)to_us_since_boot(get_absolute_time());
    size_t selected = 0;
    size_t game_count = 0;
    const game_descriptor_t *games = games_get_all(&game_count);

    stdio_init_all();
    sleep_ms(2000);
    app_init();

    while (true) {
        touch_event_t event;

        app_draw_menu_screen("SELECT", games[selected].name);
        app_wait_for_touch_release();

        while (true) {
            if (app_poll_touch_event(&event)) {
                if (app_is_left_edge_touch(event.x)) {
                    selected = (selected + game_count - 1) % game_count;
                    app_draw_menu_screen("SELECT", games[selected].name);
                    app_wait_for_touch_release();
                } else if (app_is_right_edge_touch(event.x)) {
                    selected = (selected + 1) % game_count;
                    app_draw_menu_screen("SELECT", games[selected].name);
                    app_wait_for_touch_release();
                } else {
                    break;
                }
            }
            sleep_ms(10);
        }

        game_run_context_t context = {
            .random_state = &random_state,
            .game_name = games[selected].name,
            .is_first_start = true,
        };

        games[selected].run(&context);
    }
}
