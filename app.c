#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "bsp_cst816.h"
#include "bsp_es8311.h"
#include "bsp_i2c.h"
#include "bsp_st77916.h"
#include "pio_audio.h"
#include "assets/ui/result_icons.h"
#include "app.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define LEFT_EDGE_WIDTH 64
#define RIGHT_EDGE_X (LCD_WIDTH - LEFT_EDGE_WIDTH)
#define RESULT_BUTTON_W 164
#define RESULT_BUTTON_H 56
#define RESULT_BUTTON_GAP 0
#define RESULT_BUTTON_Y 246
#define RESULT_BUTTON_X_MARGIN ((LCD_WIDTH - ((RESULT_BUTTON_W * 2) + RESULT_BUTTON_GAP)) / 2)
#define RESULT_CONTINUE_X RESULT_BUTTON_X_MARGIN
#define RESULT_FINISH_X (RESULT_CONTINUE_X + RESULT_BUTTON_W + RESULT_BUTTON_GAP)
#define RESULT_ICON_INSET 24
#define PA_CTRL 0

typedef struct {
    char c;
    uint8_t rows[7];
} glyph_t;

static volatile bool flush_done_flag = true;
static uint16_t framebuffer[LCD_WIDTH * LCD_HEIGHT];
static bool previous_touched = false;
static bool audio_ready = false;

static const glyph_t k_font[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}},
    {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},
    {'_', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F}},
    {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
    {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}},
    {'3', {0x1E, 0x01, 0x01, 0x06, 0x01, 0x01, 0x1E}},
    {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
    {'5', {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E}},
    {'6', {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}},
    {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}},
    {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x1C}},
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    {'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E}},
    {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}},
};

static const bsp_display_area_t full_area = {
    .x1 = 0,
    .y1 = 0,
    .x2 = LCD_WIDTH - 1,
    .y2 = LCD_HEIGHT - 1,
};

static bsp_display_interface_t *display = NULL;
static bsp_touch_interface_t *touch = NULL;

static bool prepare_audio_playback(void)
{
    if (!audio_ready) {
        return false;
    }

    // Keep the codec/output recovery in one place so every future playback path
    // goes through the same known-good setup.
    es8311_voice_mute(false);
    audio_reset_output();
    return true;
}

static void flush_done(void)
{
    flush_done_flag = true;
}

static const glyph_t *find_glyph(char c)
{
    for (size_t i = 0; i < ARRAY_SIZE(k_font); ++i) {
        if (k_font[i].c == c) {
            return &k_font[i];
        }
    }
    return &k_font[0];
}

static void set_pixel(int x, int y, uint16_t color)
{
    if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT) {
        return;
    }
    framebuffer[y * LCD_WIDTH + x] = color;
}

static void draw_rect(int x, int y, int w, int h, uint16_t color)
{
    for (int py = 0; py < h; ++py) {
        for (int px = 0; px < w; ++px) {
            set_pixel(x + px, y + py, color);
        }
    }
}

static void draw_char(int x, int y, char c, int scale, uint16_t color)
{
    const glyph_t *glyph = find_glyph(c);

    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if (glyph->rows[row] & (1u << (4 - col))) {
                draw_rect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

static int text_width(const char *text, int scale)
{
    return ((int)strlen(text) * 6 - 1) * scale;
}

static int fit_text_scale(const char *text, int max_scale, int min_scale, int max_width)
{
    int scale = max_scale;
    int width_at_scale_1 = text_width(text, 1);

    if (width_at_scale_1 > 0) {
        int fitted_scale = max_width / width_at_scale_1;
        if (fitted_scale < scale) {
            scale = fitted_scale;
        }
    }
    if (scale < min_scale) {
        scale = min_scale;
    }
    return scale;
}

static void draw_left_triangle(int center_x, int center_y, int size, uint16_t color)
{
    for (int dx = 0; dx < size; ++dx) {
        int span = size - dx;
        for (int dy = -span; dy <= span; ++dy) {
            set_pixel(center_x - dx, center_y + dy, color);
        }
    }
}

static void draw_right_triangle(int center_x, int center_y, int size, uint16_t color)
{
    for (int dx = 0; dx < size; ++dx) {
        int span = size - dx;
        for (int dy = -span; dy <= span; ++dy) {
            set_pixel(center_x + dx, center_y + dy, color);
        }
    }
}

static void draw_bitmap_mask_centered(int center_x, int center_y,
                                      int width, int height,
                                      const char *const *rows,
                                      uint16_t color)
{
    int origin_x = center_x - (width / 2);
    int origin_y = center_y - (height / 2);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (rows[y][x] == '1') {
                set_pixel(origin_x + x, origin_y + y, color);
            }
        }
    }
}

static uint16_t read_le16(const unsigned char *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool get_embedded_wav_pcm(const unsigned char *wav, uint32_t wav_len,
                                 const unsigned char **pcm, uint32_t *pcm_bytes)
{
    uint32_t offset = 12;
    bool fmt_ok = false;

    if (wav_len < 44 || memcmp(wav, "RIFF", 4) != 0 || memcmp(wav + 8, "WAVE", 4) != 0) {
        return false;
    }

    while (offset + 8 <= wav_len) {
        uint32_t chunk_size = read_le32(wav + offset + 4);
        const unsigned char *chunk_data = wav + offset + 8;
        if (offset + 8 + chunk_size > wav_len) {
            return false;
        }

        if (memcmp(wav + offset, "fmt ", 4) == 0) {
            if (chunk_size < 16) {
                return false;
            }
            fmt_ok = read_le16(chunk_data + 0) == 1 &&
                     read_le16(chunk_data + 2) == 1 &&
                     read_le32(chunk_data + 4) == 24000 &&
                     read_le16(chunk_data + 14) == 16;
        } else if (memcmp(wav + offset, "data", 4) == 0) {
            if (!fmt_ok) {
                return false;
            }
            *pcm = chunk_data;
            *pcm_bytes = chunk_size;
            return true;
        }

        offset += 8 + chunk_size + (chunk_size & 1u);
    }

    return false;
}

static void init_audio(void)
{
    if (audio_ready) {
        return;
    }

    gpio_init(PA_CTRL);
    gpio_set_dir(PA_CTRL, GPIO_OUT);
    gpio_put(PA_CTRL, 1);

    mclk_pio_init();
    dout_pio_init();
    es8311_init(pico_audio);
    es8311_sample_frequency_config(pico_audio.mclk_freq, pico_audio.sample_freq);
    es8311_voice_volume_set(pico_audio.volume);
    es8311_voice_mute(false);
    audio_ready = true;
}

void app_init(void)
{
    bsp_i2c_init();

    bsp_display_info_t display_info = {
        .width = LCD_WIDTH,
        .height = LCD_HEIGHT,
        .x_offset = 0,
        .y_offset = 0,
        .rotation = 0,
        .brightness = 80,
        .dma_flush_done_cb = flush_done,
    };
    bsp_touch_info_t touch_info = {
        .width = LCD_WIDTH,
        .height = LCD_HEIGHT,
        .rotation = 0,
    };

    if (!bsp_display_new_st77916(&display, &display_info)) {
        while (true) {
            sleep_ms(1000);
        }
    }
    if (!bsp_touch_new_cst816(&touch, &touch_info)) {
        while (true) {
            sleep_ms(1000);
        }
    }

    display->init();
    touch->init();
    init_audio();
}

void app_fill_screen(uint16_t color)
{
    for (size_t i = 0; i < ARRAY_SIZE(framebuffer); ++i) {
        framebuffer[i] = color;
    }
}

void app_present_frame(void)
{
    flush_done_flag = false;
    display->flush_dma((bsp_display_area_t *)&full_area, framebuffer);
    while (!flush_done_flag) {
        tight_loop_contents();
    }
}

void app_draw_text_centered(int center_x, int y, const char *text, int scale, uint16_t color)
{
    int x = center_x - text_width(text, scale) / 2;
    for (size_t i = 0; i < strlen(text); ++i) {
        draw_char(x + (int)i * 6 * scale, y, text[i], scale, color);
    }
}

static void draw_result_button(int x, int y, int w, int h, post_game_action_t action)
{
    int center_x = x + (w / 2);
    int center_y = y + (h / 2);

    if (action == POST_ACTION_REPLAY) {
        center_x += RESULT_ICON_INSET;
        draw_bitmap_mask_centered(center_x, center_y,
                                  continue_replay_icon_width,
                                  continue_replay_icon_height,
                                  continue_replay_icon_rows,
                                  COLOR_WHITE);
    } else {
        center_x -= RESULT_ICON_INSET;
        draw_bitmap_mask_centered(center_x, center_y,
                                  finish_logout_icon_width,
                                  finish_logout_icon_height,
                                  finish_logout_icon_rows,
                                  COLOR_WHITE);
    }
}

static void draw_crowned_record_row(const char *record, int center_y, int text_scale, int icon_gap)
{
    int group_width = best_crown_icon_width + icon_gap + text_width(record, text_scale);
    int group_left = (LCD_WIDTH - group_width) / 2;
    int icon_center_x = group_left + (best_crown_icon_width / 2);
    int text_center_x = group_left + best_crown_icon_width + icon_gap + (text_width(record, text_scale) / 2);

    draw_bitmap_mask_centered(icon_center_x, center_y,
                              best_crown_icon_width,
                              best_crown_icon_height,
                              best_crown_icon_rows,
                              COLOR_WHITE);
    app_draw_text_centered(text_center_x,
                           center_y - ((7 * text_scale) / 2),
                           record,
                           text_scale,
                           COLOR_WHITE);
}

static bool point_in_rect(uint16_t x, uint16_t y, int rect_x, int rect_y, int rect_w, int rect_h)
{
    return x >= rect_x && x < (rect_x + rect_w) &&
           y >= rect_y && y < (rect_y + rect_h);
}

void app_draw_menu_screen(const char *title, const char *item_name, const char *best_record)
{
    int title_scale = 4;
    int item_scale = fit_text_scale(item_name, 8, 3, LCD_WIDTH - 100);
    int title_y = 56;

    app_fill_screen(COLOR_BLACK);
    app_draw_text_centered(LCD_WIDTH / 2, title_y, title, title_scale, COLOR_WHITE);
    draw_left_triangle(28, LCD_HEIGHT / 2, 18, COLOR_WHITE);
    draw_right_triangle(LCD_WIDTH - 29, LCD_HEIGHT / 2, 18, COLOR_WHITE);
    app_draw_text_centered(LCD_WIDTH / 2,
                           (LCD_HEIGHT / 2) - ((7 * item_scale) / 2),
                           item_name,
                           item_scale,
                           COLOR_WHITE);
    draw_crowned_record_row(best_record, 248, 3, 10);
    app_present_frame();
}

void app_draw_result_screen(const app_result_view_t *view)
{
    int title_scale = fit_text_scale(view->game_name, 5, 3, LCD_WIDTH - 60);

    app_fill_screen(COLOR_BLACK);
    app_draw_text_centered(LCD_WIDTH / 2, 28, view->game_name, title_scale, COLOR_WHITE);
    app_draw_text_centered(LCD_WIDTH / 2, 102, view->current_record, 6, COLOR_WHITE);
    draw_crowned_record_row(view->best_record, 196, 4, 12);
    draw_result_button(RESULT_CONTINUE_X, RESULT_BUTTON_Y,
                       RESULT_BUTTON_W, RESULT_BUTTON_H, POST_ACTION_REPLAY);
    draw_result_button(RESULT_FINISH_X, RESULT_BUTTON_Y,
                       RESULT_BUTTON_W, RESULT_BUTTON_H, POST_ACTION_MENU);
    app_present_frame();
}

void app_draw_black_message(const char *line1, const char *line2)
{
    app_fill_screen(COLOR_BLACK);
    if (line1 != NULL) {
        app_draw_text_centered(LCD_WIDTH / 2, 110, line1, 6, COLOR_WHITE);
    }
    if (line2 != NULL) {
        app_draw_text_centered(LCD_WIDTH / 2, 220, line2, 4, COLOR_WHITE);
    }
    app_present_frame();
}

bool app_poll_touch_event(touch_event_t *event)
{
    bsp_touch_data_t data;

    touch->read();
    bool touched = touch->get_data(&data);
    bool edge = touched && !previous_touched;
    previous_touched = touched;

    if (edge && event != NULL) {
        event->x = data.coords[0].x;
        event->y = data.coords[0].y;
    }
    return edge;
}

void app_wait_for_touch_release(void)
{
    bsp_touch_data_t data;
    int stable_no_touch = 0;

    while (stable_no_touch < 4) {
        touch->read();
        if (touch->get_data(&data)) {
            stable_no_touch = 0;
        } else {
            stable_no_touch++;
        }
        sleep_ms(10);
    }

    previous_touched = false;
}

bool app_is_left_edge_touch(uint16_t x)
{
    return x < LEFT_EDGE_WIDTH;
}

bool app_is_right_edge_touch(uint16_t x)
{
    return x >= RIGHT_EDGE_X;
}

post_game_action_t app_wait_post_game_action(void)
{
    touch_event_t event;

    app_wait_for_touch_release();
    while (true) {
        if (app_poll_touch_event(&event)) {
            if (point_in_rect(event.x, event.y,
                              RESULT_FINISH_X, RESULT_BUTTON_Y,
                              RESULT_BUTTON_W, RESULT_BUTTON_H)) {
                return POST_ACTION_MENU;
            }
            if (point_in_rect(event.x, event.y,
                              RESULT_CONTINUE_X, RESULT_BUTTON_Y,
                              RESULT_BUTTON_W, RESULT_BUTTON_H)) {
                return POST_ACTION_REPLAY;
            }
        }
        sleep_ms(10);
    }
}

void app_play_wav(const unsigned char *wav, uint32_t wav_len)
{
    const unsigned char *pcm_bytes = NULL;
    uint32_t pcm_len_bytes = 0;

    if (!audio_ready || !get_embedded_wav_pcm(wav, wav_len, &pcm_bytes, &pcm_len_bytes)) {
        return;
    }

    app_play_pcm16_mono((const int16_t *)pcm_bytes, pcm_len_bytes / 2);
}

bool app_play_pcm16_mono(const int16_t *samples, uint32_t sample_count)
{
    if (samples == NULL || sample_count == 0 || !prepare_audio_playback()) {
        return false;
    }

    audio_out_pcm16(samples, (int32_t)sample_count);
    return true;
}

void app_format_hundredths(char *buffer, size_t buffer_size, uint32_t hundredths)
{
    snprintf(buffer, buffer_size, "%lu.%02lu",
             (unsigned long)(hundredths / 100),
             (unsigned long)(hundredths % 100));
}

void app_update_best_hundredths(uint32_t current_hundredths, uint32_t *best_hundredths)
{
    if (*best_hundredths == 0 || current_hundredths < *best_hundredths) {
        *best_hundredths = current_hundredths;
    }
}

uint32_t app_random_next(uint32_t *state)
{
    *state = (*state * 1664525u) + 1013904223u;
    return *state;
}
