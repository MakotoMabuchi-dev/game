#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "app.h"
#include "best_store.h"
#include "assets/3_TETRIS/tetris_icons.h"
#include "3_TETRIS.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define TETRIS_BOARD_ROWS 20
#define TETRIS_BOARD_COLS 10
#define TETRIS_CELL_SIZE 14
#define TETRIS_LAYOUT_OFFSET_Y 10
#define TETRIS_BOARD_W (TETRIS_BOARD_COLS * TETRIS_CELL_SIZE)
#define TETRIS_BOARD_H (TETRIS_BOARD_ROWS * TETRIS_CELL_SIZE)
#define TETRIS_BOARD_X ((LCD_WIDTH - TETRIS_BOARD_W) / 2)
#define TETRIS_BOARD_Y (((LCD_HEIGHT - TETRIS_BOARD_H) / 2) + TETRIS_LAYOUT_OFFSET_Y)
#define TETRIS_BUTTON_RADIUS 34
#define TETRIS_BUTTON_HIT_RADIUS 38
#define TETRIS_BUTTON_HOLD_RADIUS 48
#define TETRIS_SIDE_BUTTON_HIT_RADIUS 46
#define TETRIS_SIDE_BUTTON_HOLD_RADIUS 56
#define TETRIS_BUTTON_TOP_LEFT_X 52
#define TETRIS_BUTTON_TOP_RIGHT_X (LCD_WIDTH - TETRIS_BUTTON_TOP_LEFT_X)
#define TETRIS_BUTTON_BOTTOM_LEFT_X 70
#define TETRIS_BUTTON_BOTTOM_RIGHT_X (LCD_WIDTH - TETRIS_BUTTON_BOTTOM_LEFT_X)
#define TETRIS_BUTTON_TOP_Y (186 + TETRIS_LAYOUT_OFFSET_Y)
#define TETRIS_BUTTON_BOTTOM_Y (258 + TETRIS_LAYOUT_OFFSET_Y)
#define TETRIS_TOUCH_RELEASE_GRACE_MS 80u
#define TETRIS_SCORE_X (LCD_WIDTH - TETRIS_PREVIEW_X)
#define TETRIS_PREVIEW_X 296
#define TETRIS_PREVIEW_BOX_Y (TETRIS_BOARD_Y + 22)
#define TETRIS_PANEL_LABEL_CENTER_Y (TETRIS_PREVIEW_BOX_Y - 8)
#define TETRIS_PREVIEW_BOX_SIZE 56
#define TETRIS_TIMER_Y (TETRIS_BOARD_Y - 18)
#define TETRIS_PANEL_LABEL_SCALE 2
#define TETRIS_TIME_TEXT_SCALE 2
#define TETRIS_ROUND_MS 90000u
#define TETRIS_SPEED_STEP_MS 15000u
#define TETRIS_FRAME_US 16666

typedef enum {
    TETRIS_PIECE_I = 0,
    TETRIS_PIECE_O,
    TETRIS_PIECE_T,
    TETRIS_PIECE_J,
    TETRIS_PIECE_L,
    TETRIS_PIECE_S,
    TETRIS_PIECE_Z,
    TETRIS_PIECE_COUNT,
} tetris_piece_t;

typedef struct {
    int8_t x;
    int8_t y;
} tetris_block_t;

typedef enum {
    TETRIS_ACTION_NONE = 0,
    TETRIS_ACTION_LEFT,
    TETRIS_ACTION_FALL,
    TETRIS_ACTION_RIGHT,
    TETRIS_ACTION_ROTATE,
} tetris_action_t;

typedef struct {
    tetris_action_t action;
    int center_x;
    int center_y;
    int hit_radius;
    int hold_radius;
} tetris_button_t;

typedef struct {
    uint8_t board[TETRIS_BOARD_ROWS][TETRIS_BOARD_COLS];
    uint8_t bag[TETRIS_PIECE_COUNT];
    uint8_t bag_index;
    uint8_t current_piece;
    uint8_t next_piece;
    uint8_t rotation;
    int piece_x;
    int piece_y;
    uint32_t score;
    uint32_t lines;
    uint32_t drop_interval_ms;
    absolute_time_t next_fall_time;
} tetris_state_t;

static uint32_t best_score = 0;
static bool has_best_record = false;
static bool best_loaded = false;

static void tetris_load_best(void)
{
    uint32_t stored_value = 0;
    bool has_value = false;

    if (best_loaded) {
        return;
    }
    if (!best_store_load_u32("TETRIS", &stored_value, &has_value)) {
        return;
    }

    best_score = stored_value;
    has_best_record = has_value;
    best_loaded = true;
}

static void tetris_save_best(void)
{
    best_loaded = true;
    best_store_save_u32("TETRIS", best_score, has_best_record);
}

static const tetris_button_t tetris_buttons[] = {
    {TETRIS_ACTION_LEFT, TETRIS_BUTTON_TOP_LEFT_X, TETRIS_BUTTON_TOP_Y,
     TETRIS_SIDE_BUTTON_HIT_RADIUS, TETRIS_SIDE_BUTTON_HOLD_RADIUS},
    {TETRIS_ACTION_FALL, TETRIS_BUTTON_BOTTOM_LEFT_X, TETRIS_BUTTON_BOTTOM_Y,
     TETRIS_BUTTON_HIT_RADIUS, TETRIS_BUTTON_HOLD_RADIUS},
    {TETRIS_ACTION_RIGHT, TETRIS_BUTTON_TOP_RIGHT_X, TETRIS_BUTTON_TOP_Y,
     TETRIS_SIDE_BUTTON_HIT_RADIUS, TETRIS_SIDE_BUTTON_HOLD_RADIUS},
    {TETRIS_ACTION_ROTATE, TETRIS_BUTTON_BOTTOM_RIGHT_X, TETRIS_BUTTON_BOTTOM_Y,
     TETRIS_BUTTON_HIT_RADIUS, TETRIS_BUTTON_HOLD_RADIUS},
};

static const char *const tetris_score_label_rows[5] = {
    "1110111011101100111",
    "1000100010101010100",
    "1110100010101100110",
    "0010100010101010100",
    "1110111011101010111",
};

static const char *const tetris_next_label_rows[5] = {
    "101011101010111",
    "111010001010010",
    "111011000100010",
    "111010001010010",
    "101011101010010",
};

static const tetris_block_t tetris_piece_blocks[TETRIS_PIECE_COUNT][4][4] = {
    {
        {{0, 1}, {1, 1}, {2, 1}, {3, 1}},
        {{2, 0}, {2, 1}, {2, 2}, {2, 3}},
        {{0, 2}, {1, 2}, {2, 2}, {3, 2}},
        {{1, 0}, {1, 1}, {1, 2}, {1, 3}},
    },
    {
        {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
        {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
        {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
        {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
    },
    {
        {{1, 0}, {0, 1}, {1, 1}, {2, 1}},
        {{1, 0}, {1, 1}, {2, 1}, {1, 2}},
        {{0, 1}, {1, 1}, {2, 1}, {1, 2}},
        {{1, 0}, {0, 1}, {1, 1}, {1, 2}},
    },
    {
        {{0, 0}, {0, 1}, {1, 1}, {2, 1}},
        {{1, 0}, {2, 0}, {1, 1}, {1, 2}},
        {{0, 1}, {1, 1}, {2, 1}, {2, 2}},
        {{1, 0}, {1, 1}, {0, 2}, {1, 2}},
    },
    {
        {{2, 0}, {0, 1}, {1, 1}, {2, 1}},
        {{1, 0}, {1, 1}, {1, 2}, {2, 2}},
        {{0, 1}, {1, 1}, {2, 1}, {0, 2}},
        {{0, 0}, {1, 0}, {1, 1}, {1, 2}},
    },
    {
        {{1, 0}, {2, 0}, {0, 1}, {1, 1}},
        {{1, 0}, {1, 1}, {2, 1}, {2, 2}},
        {{1, 1}, {2, 1}, {0, 2}, {1, 2}},
        {{0, 0}, {0, 1}, {1, 1}, {1, 2}},
    },
    {
        {{0, 0}, {1, 0}, {1, 1}, {2, 1}},
        {{2, 0}, {1, 1}, {2, 1}, {1, 2}},
        {{0, 1}, {1, 1}, {1, 2}, {2, 2}},
        {{1, 0}, {0, 1}, {1, 1}, {0, 2}},
    },
};

static void tetris_format_number(char *buffer, size_t buffer_size, uint32_t value)
{
    snprintf(buffer, buffer_size, "%lu", (unsigned long)value);
}

static int tetris_text_width(const char *text, int scale)
{
    return ((int)strlen(text) * 6 - 1) * scale;
}

static void tetris_draw_text_left(int x, int y, const char *text, int scale, uint16_t color)
{
    app_draw_text_centered(x + (tetris_text_width(text, scale) / 2), y, text, scale, color);
}

static void tetris_draw_small_label(int center_x, int center_y,
                                    int width,
                                    int scale,
                                    const char *const *rows)
{
    int draw_w = width * scale;
    int draw_h = 5 * scale;
    int origin_x = center_x - (draw_w / 2);
    int origin_y = center_y - (draw_h / 2);

    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < width; ++col) {
            if (rows[row][col] == '1') {
                app_fill_rect(origin_x + (col * scale),
                              origin_y + (row * scale),
                              scale,
                              scale,
                              COLOR_WHITE);
            }
        }
    }
}

static uint32_t tetris_drop_interval_ms(uint32_t elapsed_ms)
{
    uint32_t stage = elapsed_ms / TETRIS_SPEED_STEP_MS;
    uint32_t reduction = stage * 80u;

    if (reduction >= 520u) {
        return 180u;
    }
    return 700u - reduction;
}

static uint32_t tetris_action_initial_delay_ms(tetris_action_t action)
{
    return action == TETRIS_ACTION_FALL ? 120u : 220u;
}

static uint32_t tetris_action_repeat_delay_ms(tetris_action_t action)
{
    if (action == TETRIS_ACTION_FALL) {
        return 55u;
    }
    if (action == TETRIS_ACTION_ROTATE) {
        return 180u;
    }
    return 90u;
}

static void tetris_schedule_next_fall(tetris_state_t *state)
{
    state->next_fall_time = make_timeout_time_ms(state->drop_interval_ms);
}

static void tetris_shuffle_bag(tetris_state_t *state, uint32_t *random_state)
{
    for (size_t i = 0; i < TETRIS_PIECE_COUNT; ++i) {
        state->bag[i] = (uint8_t)i;
    }

    for (int i = TETRIS_PIECE_COUNT - 1; i > 0; --i) {
        uint32_t j = app_random_next(random_state) % (uint32_t)(i + 1);
        uint8_t tmp = state->bag[i];
        state->bag[i] = state->bag[j];
        state->bag[j] = tmp;
    }

    state->bag_index = 0;
}

static uint8_t tetris_take_piece(tetris_state_t *state, uint32_t *random_state)
{
    if (state->bag_index >= TETRIS_PIECE_COUNT) {
        tetris_shuffle_bag(state, random_state);
    }

    return state->bag[state->bag_index++];
}

static bool tetris_piece_fits(const tetris_state_t *state,
                              uint8_t piece, uint8_t rotation,
                              int piece_x, int piece_y)
{
    const tetris_block_t *blocks = tetris_piece_blocks[piece][rotation];

    for (size_t i = 0; i < 4; ++i) {
        int board_x = piece_x + blocks[i].x;
        int board_y = piece_y + blocks[i].y;

        if (board_x < 0 || board_x >= TETRIS_BOARD_COLS || board_y >= TETRIS_BOARD_ROWS) {
            return false;
        }
        if (board_y >= 0 && state->board[board_y][board_x] != 0) {
            return false;
        }
    }

    return true;
}

static bool tetris_spawn_piece(tetris_state_t *state, uint32_t *random_state)
{
    state->current_piece = state->next_piece;
    state->next_piece = tetris_take_piece(state, random_state);
    state->rotation = 0;
    state->piece_x = 3;
    state->piece_y = 0;
    tetris_schedule_next_fall(state);

    return tetris_piece_fits(state,
                             state->current_piece,
                             state->rotation,
                             state->piece_x,
                             state->piece_y);
}

static void tetris_draw_frame_box(int x, int y, int w, int h)
{
    app_fill_rect(x - 2, y - 2, w + 4, h + 4, COLOR_WHITE);
    app_fill_rect(x, y, w, h, COLOR_BLACK);
}

static void tetris_draw_cell_pixels(int px, int py, int cell_size, uint16_t color)
{
    app_fill_rect(px + 1, py + 1, cell_size - 2, cell_size - 2, color);
}

static void tetris_draw_board_cell(int col, int row, uint16_t color)
{
    tetris_draw_cell_pixels(TETRIS_BOARD_X + (col * TETRIS_CELL_SIZE),
                            TETRIS_BOARD_Y + (row * TETRIS_CELL_SIZE),
                            TETRIS_CELL_SIZE,
                            color);
}

static void tetris_draw_preview_piece(uint8_t piece, uint16_t color)
{
    const tetris_block_t *blocks = tetris_piece_blocks[piece][0];
    const int preview_cell_size = 10;
    int min_x = blocks[0].x;
    int min_y = blocks[0].y;
    int max_x = blocks[0].x;
    int max_y = blocks[0].y;

    for (size_t i = 1; i < 4; ++i) {
        if (blocks[i].x < min_x) {
            min_x = blocks[i].x;
        }
        if (blocks[i].x > max_x) {
            max_x = blocks[i].x;
        }
        if (blocks[i].y < min_y) {
            min_y = blocks[i].y;
        }
        if (blocks[i].y > max_y) {
            max_y = blocks[i].y;
        }
    }

    {
        int piece_w = (max_x - min_x + 1) * preview_cell_size;
        int piece_h = (max_y - min_y + 1) * preview_cell_size;
        int origin_x = TETRIS_PREVIEW_X - (TETRIS_PREVIEW_BOX_SIZE / 2) + ((TETRIS_PREVIEW_BOX_SIZE - piece_w) / 2)
                     - (min_x * preview_cell_size);
        int origin_y = TETRIS_PREVIEW_BOX_Y + ((TETRIS_PREVIEW_BOX_SIZE - piece_h) / 2)
                     - (min_y * preview_cell_size);

        for (size_t i = 0; i < 4; ++i) {
            tetris_draw_cell_pixels(origin_x + (blocks[i].x * preview_cell_size),
                                    origin_y + (blocks[i].y * preview_cell_size),
                                    preview_cell_size,
                                    color);
        }
    }
}

static void tetris_draw_status(uint32_t seconds_left, uint32_t score)
{
    char time_text[16];

    (void)score;
    snprintf(time_text, sizeof(time_text), "TIME %lu", (unsigned long)seconds_left);
    app_draw_text_centered(TETRIS_BOARD_X + (TETRIS_BOARD_W / 2),
                           TETRIS_TIMER_Y,
                           time_text,
                           TETRIS_TIME_TEXT_SCALE,
                           COLOR_WHITE);
}

static void tetris_draw_score(uint32_t score)
{
    char score_text[16];

    tetris_format_number(score_text, sizeof(score_text), score);
    tetris_draw_small_label(TETRIS_SCORE_X, TETRIS_PANEL_LABEL_CENTER_Y,
                            19, TETRIS_PANEL_LABEL_SCALE, tetris_score_label_rows);
    tetris_draw_frame_box(TETRIS_SCORE_X - (TETRIS_PREVIEW_BOX_SIZE / 2),
                          TETRIS_PREVIEW_BOX_Y,
                          TETRIS_PREVIEW_BOX_SIZE,
                          TETRIS_PREVIEW_BOX_SIZE);
    app_draw_text_centered(TETRIS_SCORE_X,
                           TETRIS_PREVIEW_BOX_Y + ((TETRIS_PREVIEW_BOX_SIZE - (7 * 2)) / 2),
                           score_text,
                           2,
                           COLOR_WHITE);
}

static void tetris_draw_control_button(int center_x, int center_y,
                                       int icon_width, int icon_height,
                                       const char *const *icon_rows,
                                       bool is_active)
{
    uint16_t ring_color = is_active ? COLOR_RED : COLOR_WHITE;
    uint16_t fill_color = is_active ? COLOR_WHITE : COLOR_BLACK;
    uint16_t icon_color = is_active ? COLOR_BLACK : COLOR_WHITE;

    app_draw_filled_circle(center_x, center_y, TETRIS_BUTTON_RADIUS, ring_color);
    app_draw_filled_circle(center_x, center_y, TETRIS_BUTTON_RADIUS - 4, fill_color);
    app_draw_bitmap_mask_centered(center_x, center_y,
                                  icon_width, icon_height,
                                  icon_rows,
                                  icon_color);
}

static void tetris_draw_controls(tetris_action_t active_action)
{
    tetris_draw_control_button(TETRIS_BUTTON_TOP_LEFT_X, TETRIS_BUTTON_TOP_Y,
                               tetris_left_icon_width, tetris_left_icon_height, tetris_left_icon_rows,
                               active_action == TETRIS_ACTION_LEFT);
    tetris_draw_control_button(TETRIS_BUTTON_BOTTOM_LEFT_X, TETRIS_BUTTON_BOTTOM_Y,
                               tetris_fall_icon_width, tetris_fall_icon_height, tetris_fall_icon_rows,
                               active_action == TETRIS_ACTION_FALL);
    tetris_draw_control_button(TETRIS_BUTTON_TOP_RIGHT_X, TETRIS_BUTTON_TOP_Y,
                               tetris_right_icon_width, tetris_right_icon_height, tetris_right_icon_rows,
                               active_action == TETRIS_ACTION_RIGHT);
    tetris_draw_control_button(TETRIS_BUTTON_BOTTOM_RIGHT_X, TETRIS_BUTTON_BOTTOM_Y,
                               tetris_rotate_icon_width, tetris_rotate_icon_height, tetris_rotate_icon_rows,
                               active_action == TETRIS_ACTION_ROTATE);
}

static bool tetris_point_in_circle(uint16_t x, uint16_t y, int center_x, int center_y, int radius)
{
    int dx = (int)x - center_x;
    int dy = (int)y - center_y;

    return (dx * dx) + (dy * dy) <= (radius * radius);
}

static const tetris_button_t *tetris_find_button(tetris_action_t action)
{
    for (size_t i = 0; i < ARRAY_SIZE(tetris_buttons); ++i) {
        if (tetris_buttons[i].action == action) {
            return &tetris_buttons[i];
        }
    }

    return NULL;
}

static tetris_action_t tetris_action_from_touch(const touch_event_t *event, tetris_action_t held_action)
{
    const tetris_button_t *held_button = tetris_find_button(held_action);
    tetris_action_t best_action = TETRIS_ACTION_NONE;
    int best_distance_sq = 0;

    if (held_button != NULL &&
        tetris_point_in_circle(event->x, event->y,
                               held_button->center_x, held_button->center_y,
                               held_button->hold_radius)) {
        return held_action;
    }

    for (size_t i = 0; i < ARRAY_SIZE(tetris_buttons); ++i) {
        int dx = (int)event->x - tetris_buttons[i].center_x;
        int dy = (int)event->y - tetris_buttons[i].center_y;
        int distance_sq = (dx * dx) + (dy * dy);

        if (distance_sq > (tetris_buttons[i].hit_radius * tetris_buttons[i].hit_radius)) {
            continue;
        }
        if (best_action == TETRIS_ACTION_NONE || distance_sq < best_distance_sq) {
            best_action = tetris_buttons[i].action;
            best_distance_sq = distance_sq;
        }
    }

    return best_action;
}

static void tetris_draw_scene(const tetris_state_t *state,
                              uint32_t seconds_left,
                              tetris_action_t active_action)
{
    const tetris_block_t *active_blocks = tetris_piece_blocks[state->current_piece][state->rotation];

    app_fill_screen(COLOR_BLACK);
    tetris_draw_status(seconds_left, state->score);
    tetris_draw_score(state->score);
    tetris_draw_small_label(TETRIS_PREVIEW_X, TETRIS_PANEL_LABEL_CENTER_Y,
                            15, TETRIS_PANEL_LABEL_SCALE, tetris_next_label_rows);
    tetris_draw_frame_box(TETRIS_PREVIEW_X - (TETRIS_PREVIEW_BOX_SIZE / 2),
                          TETRIS_PREVIEW_BOX_Y,
                          TETRIS_PREVIEW_BOX_SIZE,
                          TETRIS_PREVIEW_BOX_SIZE);
    tetris_draw_preview_piece(state->next_piece, COLOR_WHITE);
    tetris_draw_frame_box(TETRIS_BOARD_X, TETRIS_BOARD_Y, TETRIS_BOARD_W, TETRIS_BOARD_H);

    for (int row = 0; row < TETRIS_BOARD_ROWS; ++row) {
        for (int col = 0; col < TETRIS_BOARD_COLS; ++col) {
            if (state->board[row][col] != 0) {
                tetris_draw_board_cell(col, row, COLOR_WHITE);
            }
        }
    }

    for (size_t i = 0; i < 4; ++i) {
        int col = state->piece_x + active_blocks[i].x;
        int row = state->piece_y + active_blocks[i].y;

        if (row >= 0) {
            tetris_draw_board_cell(col, row, COLOR_RED);
        }
    }

    tetris_draw_controls(active_action);
    app_present_frame();
}

static bool tetris_try_move(tetris_state_t *state, int dx, int dy)
{
    int next_x = state->piece_x + dx;
    int next_y = state->piece_y + dy;

    if (!tetris_piece_fits(state,
                           state->current_piece,
                           state->rotation,
                           next_x,
                           next_y)) {
        return false;
    }

    state->piece_x = next_x;
    state->piece_y = next_y;
    return true;
}

static bool tetris_try_rotate(tetris_state_t *state)
{
    static const int kicks[][2] = {
        {0, 0},
        {-1, 0},
        {1, 0},
        {-2, 0},
        {2, 0},
        {0, -1},
    };
    uint8_t next_rotation = (uint8_t)((state->rotation + 1u) % 4u);

    for (size_t i = 0; i < ARRAY_SIZE(kicks); ++i) {
        int next_x = state->piece_x + kicks[i][0];
        int next_y = state->piece_y + kicks[i][1];

        if (tetris_piece_fits(state,
                              state->current_piece,
                              next_rotation,
                              next_x,
                              next_y)) {
            state->rotation = next_rotation;
            state->piece_x = next_x;
            state->piece_y = next_y;
            return true;
        }
    }

    return false;
}

static uint32_t tetris_clear_lines(tetris_state_t *state)
{
    int write_row = TETRIS_BOARD_ROWS - 1;
    uint32_t cleared = 0;

    for (int row = TETRIS_BOARD_ROWS - 1; row >= 0; --row) {
        bool full = true;

        for (int col = 0; col < TETRIS_BOARD_COLS; ++col) {
            if (state->board[row][col] == 0) {
                full = false;
                break;
            }
        }

        if (full) {
            cleared++;
            continue;
        }

        if (write_row != row) {
            memcpy(state->board[write_row], state->board[row], sizeof(state->board[row]));
        }
        write_row--;
    }

    while (write_row >= 0) {
        memset(state->board[write_row], 0, sizeof(state->board[write_row]));
        write_row--;
    }

    return cleared;
}

static void tetris_add_line_clear_score(tetris_state_t *state, uint32_t cleared_lines)
{
    static const uint16_t line_scores[] = {0, 10, 22, 36, 52, 70};

    if (cleared_lines < ARRAY_SIZE(line_scores)) {
        state->score += line_scores[cleared_lines];
    }
}

static bool tetris_lock_and_continue(tetris_state_t *state, uint32_t *random_state)
{
    const tetris_block_t *blocks = tetris_piece_blocks[state->current_piece][state->rotation];
    bool hit_top = false;

    for (size_t i = 0; i < 4; ++i) {
        int board_x = state->piece_x + blocks[i].x;
        int board_y = state->piece_y + blocks[i].y;

        if (board_y < 0) {
            hit_top = true;
            continue;
        }
        state->board[board_y][board_x] = state->current_piece + 1u;
    }

    if (hit_top) {
        return false;
    }

    {
        uint32_t cleared_lines = tetris_clear_lines(state);
        tetris_add_line_clear_score(state, cleared_lines);
        state->lines += cleared_lines;
    }
    return tetris_spawn_piece(state, random_state);
}

static bool tetris_step_fall(tetris_state_t *state, uint32_t *random_state)
{
    if (tetris_try_move(state, 0, 1)) {
        tetris_schedule_next_fall(state);
        return true;
    }

    return tetris_lock_and_continue(state, random_state);
}

static bool tetris_soft_drop(tetris_state_t *state, uint32_t *random_state)
{
    if (tetris_try_move(state, 0, 1)) {
        tetris_schedule_next_fall(state);
        return true;
    }

    return tetris_lock_and_continue(state, random_state);
}

static bool tetris_apply_action(tetris_state_t *state,
                                uint32_t *random_state,
                                tetris_action_t action)
{
    switch (action) {
    case TETRIS_ACTION_LEFT:
        (void)tetris_try_move(state, -1, 0);
        return true;
    case TETRIS_ACTION_FALL:
        return tetris_soft_drop(state, random_state);
    case TETRIS_ACTION_RIGHT:
        (void)tetris_try_move(state, 1, 0);
        return true;
    case TETRIS_ACTION_ROTATE:
        (void)tetris_try_rotate(state);
        return true;
    case TETRIS_ACTION_NONE:
    default:
        return true;
    }
}

static void tetris_update_drop_speed(tetris_state_t *state, uint32_t elapsed_ms)
{
    uint32_t next_interval = tetris_drop_interval_ms(elapsed_ms);

    if (next_interval != state->drop_interval_ms) {
        state->drop_interval_ms = next_interval;
        tetris_schedule_next_fall(state);
    }
}

static bool tetris_init_round(tetris_state_t *state, uint32_t *random_state)
{
    memset(state, 0, sizeof(*state));
    state->bag_index = TETRIS_PIECE_COUNT;
    state->drop_interval_ms = tetris_drop_interval_ms(0);
    state->next_piece = tetris_take_piece(state, random_state);
    return tetris_spawn_piece(state, random_state);
}

void game_3_tetris_get_best_record(char *buffer, size_t buffer_size)
{
    tetris_load_best();

    if (!has_best_record) {
        snprintf(buffer, buffer_size, "-");
        return;
    }

    tetris_format_number(buffer, buffer_size, best_score);
}

void run_game_3_tetris(game_run_context_t *context)
{
    tetris_load_best();

    while (true) {
        tetris_state_t state;
        touch_event_t touch;
        absolute_time_t round_start;
        absolute_time_t round_deadline;
        absolute_time_t next_repeat_time = nil_time;
        absolute_time_t touch_grace_deadline = nil_time;
        tetris_action_t held_action = TETRIS_ACTION_NONE;
        char score_text[16];
        char best_text[16];
        app_result_view_t result_view;

        (void)game_consume_first_start(context);
        app_wait_for_touch_release();

        if (!tetris_init_round(&state, context->random_state)) {
            app_draw_black_message("TETRIS", "ERROR");
            return;
        }

        round_start = get_absolute_time();
        round_deadline = make_timeout_time_ms(TETRIS_ROUND_MS);

        while (true) {
            absolute_time_t frame_start = get_absolute_time();
            absolute_time_t now = frame_start;
            int64_t remaining_us = absolute_time_diff_us(now, round_deadline);
            int64_t elapsed_us = absolute_time_diff_us(round_start, now);
            uint32_t elapsed_ms;
            uint32_t seconds_left;
            tetris_action_t active_action = TETRIS_ACTION_NONE;
            bool keep_running = true;

            if (remaining_us <= 0) {
                break;
            }

            elapsed_ms = elapsed_us <= 0 ? 0u : (uint32_t)(elapsed_us / 1000);
            seconds_left = (uint32_t)((remaining_us + 999999) / 1000000);
            tetris_update_drop_speed(&state, elapsed_ms);

            if (app_read_touch_state(&touch)) {
                active_action = tetris_action_from_touch(&touch, held_action);
                if (active_action != TETRIS_ACTION_NONE) {
                    touch_grace_deadline = make_timeout_time_ms(TETRIS_TOUCH_RELEASE_GRACE_MS);
                }
            } else if (held_action != TETRIS_ACTION_NONE &&
                       absolute_time_diff_us(now, touch_grace_deadline) > 0) {
                active_action = held_action;
            }

            if (active_action == TETRIS_ACTION_NONE) {
                held_action = TETRIS_ACTION_NONE;
                next_repeat_time = nil_time;
            } else if (active_action != held_action) {
                held_action = active_action;
                keep_running = tetris_apply_action(&state, context->random_state, active_action);
                next_repeat_time = make_timeout_time_ms(tetris_action_initial_delay_ms(active_action));
            } else if (absolute_time_diff_us(now, next_repeat_time) <= 0) {
                keep_running = tetris_apply_action(&state, context->random_state, active_action);
                next_repeat_time = make_timeout_time_ms(tetris_action_repeat_delay_ms(active_action));
            }

            if (!keep_running) {
                break;
            }

            if (absolute_time_diff_us(now, state.next_fall_time) <= 0) {
                if (!tetris_step_fall(&state, context->random_state)) {
                    break;
                }
            }

            tetris_draw_scene(&state, seconds_left, active_action);
            sleep_until(delayed_by_us(frame_start, TETRIS_FRAME_US));
        }

        bool previous_has_best = has_best_record;
        uint32_t previous_best = best_score;

        if (!has_best_record || state.score > best_score) {
            best_score = state.score;
            has_best_record = true;
        }
        if (has_best_record != previous_has_best || best_score != previous_best) {
            tetris_save_best();
        }

        tetris_format_number(score_text, sizeof(score_text), state.score);
        tetris_format_number(best_text, sizeof(best_text), best_score);
        result_view.game_name = context->game_name;
        result_view.record_label = "SCORE";
        result_view.current_record = score_text;
        result_view.best_record = best_text;
        printf("3_tetris: lines=%lu score=%lu\n",
               (unsigned long)state.lines,
               (unsigned long)state.score);
        app_draw_result_screen(&result_view);

        if (app_wait_post_game_action() == POST_ACTION_MENU) {
            return;
        }
    }
}
