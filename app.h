#ifndef APP_H
#define APP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LCD_WIDTH 360
#define LCD_HEIGHT 360
#define COLOR_BLACK 0x0000
#define COLOR_WHITE 0xFFFF
#define COLOR_RED 0x00F8

typedef struct {
    uint16_t x;
    uint16_t y;
} touch_event_t;

typedef enum {
    POST_ACTION_REPLAY,
    POST_ACTION_MENU,
} post_game_action_t;

typedef struct {
    const char *game_name;
    const char *current_record;
    const char *best_record;
} app_result_view_t;

void app_init(void);
void app_fill_screen(uint16_t color);
void app_present_frame(void);
void app_draw_text_centered(int center_x, int y, const char *text, int scale, uint16_t color);
void app_draw_menu_screen(const char *title, const char *item_name);
void app_draw_result_screen(const app_result_view_t *view);
void app_draw_black_message(const char *line1, const char *line2);
bool app_poll_touch_event(touch_event_t *event);
void app_wait_for_touch_release(void);
bool app_is_left_edge_touch(uint16_t x);
bool app_is_right_edge_touch(uint16_t x);
post_game_action_t app_wait_post_game_action(void);
void app_play_wav(const unsigned char *wav, uint32_t wav_len);
void app_format_hundredths(char *buffer, size_t buffer_size, uint32_t hundredths);
void app_update_best_hundredths(uint32_t current_hundredths, uint32_t *best_hundredths);
uint32_t app_random_next(uint32_t *state);

#endif
