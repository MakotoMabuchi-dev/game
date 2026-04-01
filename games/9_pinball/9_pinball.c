#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "app.h"
#include "best_store.h"
#include "2_hit20/2_hit20.h"
#include "9_pinball.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define PINBALL_FRAME_US 16666
#define PINBALL_COLOR_BLUE 0x1F00
#define PINBALL_COLOR_GREEN 0xE007
#define PINBALL_COLOR_YELLOW 0xE0FF
#define PINBALL_BALLS_PER_ROUND 3u
#define PINBALL_BALL_RADIUS 5
#define PINBALL_LEFT_WALL_X 22
#define PINBALL_RIGHT_WALL_X 338
#define PINBALL_TOP_WALL_Y 22
#define PINBALL_SHOOTER_DIVIDER_X 300
#define PINBALL_SHOOTER_GATE_Y 58
#define PINBALL_DRAIN_LEFT_X 150
#define PINBALL_DRAIN_RIGHT_X 210
#define PINBALL_LAUNCH_X 320
#define PINBALL_LAUNCH_Y 308
#define PINBALL_LAUNCH_VX -96
#define PINBALL_LAUNCH_VY -430
#define PINBALL_LAUNCH_DELAY_MS 160u
#define PINBALL_NEXT_BALL_DELAY_MS 700u
#define PINBALL_GRAVITY 430
#define PINBALL_MAX_VX 360
#define PINBALL_MAX_VY 520
#define PINBALL_LEFT_PIVOT_X 122
#define PINBALL_RIGHT_PIVOT_X 238
#define PINBALL_FLIPPER_PIVOT_Y 322
#define PINBALL_LEFT_REST_TIP_X 164
#define PINBALL_LEFT_REST_TIP_Y 334
#define PINBALL_LEFT_ACTIVE_TIP_X 170
#define PINBALL_LEFT_ACTIVE_TIP_Y 298
#define PINBALL_RIGHT_REST_TIP_X 196
#define PINBALL_RIGHT_REST_TIP_Y 334
#define PINBALL_RIGHT_ACTIVE_TIP_X 190
#define PINBALL_RIGHT_ACTIVE_TIP_Y 298
#define PINBALL_FLIPPER_SAMPLES 8
#define PINBALL_TARGET_COUNT 3

typedef struct {
    int x;
    int y;
    int radius;
    int kick;
    uint32_t score;
} pinball_circle_feature_t;

typedef struct {
    uint32_t score;
    uint32_t balls_left;
    uint32_t next_launch_ms;
    bool ball_active;
    int32_t ball_x;
    int32_t ball_y;
    int ball_vx;
    int ball_vy;
    bool targets_lit[PINBALL_TARGET_COUNT];
} pinball_state_t;

static uint32_t best_score = 0;
static bool best_loaded = false;

static const pinball_circle_feature_t pinball_bumpers[] = {
    {126, 120, 19, 130, 10u},
    {180, 92, 21, 142, 12u},
    {234, 120, 19, 130, 10u},
};

static void pinball_load_best(void)
{
    uint32_t stored_value = 0;
    bool has_value = false;

    if (best_loaded) {
        return;
    }
    if (!best_store_load_u32("PINBALL", &stored_value, &has_value)) {
        return;
    }

    if (has_value) {
        best_score = stored_value;
    }
    best_loaded = true;
}

static void pinball_save_best(void)
{
    best_loaded = true;
    best_store_save_u32("PINBALL", best_score, best_score != 0u);
}

static const pinball_circle_feature_t pinball_posts[] = {
    {44, 92, 6, 48, 0u},
    {34, 168, 6, 48, 0u},
    {316, 92, 6, 48, 0u},
    {326, 168, 6, 48, 0u},
    {100, 262, 7, 54, 0u},
    {260, 262, 7, 54, 0u},
};

static const int pinball_target_x[PINBALL_TARGET_COUNT] = {122, 180, 238};

static int pinball_abs(int value)
{
    return value < 0 ? -value : value;
}

static int pinball_sign(int value)
{
    return value < 0 ? -1 : 1;
}

static void pinball_format_score(char *buffer, size_t buffer_size, uint32_t score)
{
    snprintf(buffer, buffer_size, "%lu", (unsigned long)score);
}

static bool pinball_rect_overlap(int left_a, int top_a, int right_a, int bottom_a,
                                 int left_b, int top_b, int right_b, int bottom_b)
{
    return left_a < right_b && right_a > left_b &&
           top_a < bottom_b && bottom_a > top_b;
}

static void pinball_clamp_velocity(pinball_state_t *state)
{
    if (state->ball_vx > PINBALL_MAX_VX) {
        state->ball_vx = PINBALL_MAX_VX;
    } else if (state->ball_vx < -PINBALL_MAX_VX) {
        state->ball_vx = -PINBALL_MAX_VX;
    }

    if (state->ball_vy > PINBALL_MAX_VY) {
        state->ball_vy = PINBALL_MAX_VY;
    } else if (state->ball_vy < -PINBALL_MAX_VY) {
        state->ball_vy = -PINBALL_MAX_VY;
    }

    if (pinball_abs(state->ball_vx) < 18) {
        int direction_hint = (int)(state->ball_x / 1000) - (LCD_WIDTH / 2);

        state->ball_vx = pinball_sign(state->ball_vx == 0 ? direction_hint : state->ball_vx) * 18;
    }
}

static void pinball_draw_segment(int x1, int y1, int x2, int y2, int radius, uint16_t color)
{
    int steps = pinball_abs(x2 - x1);

    if (pinball_abs(y2 - y1) > steps) {
        steps = pinball_abs(y2 - y1);
    }
    if (steps < 1) {
        steps = 1;
    }

    for (int i = 0; i <= steps; ++i) {
        int x = x1 + (((x2 - x1) * i) / steps);
        int y = y1 + (((y2 - y1) * i) / steps);

        app_draw_filled_circle(x, y, radius, color);
    }
}

static void pinball_get_flipper_tip(bool active, bool left_flipper, int *tip_x, int *tip_y)
{
    if (left_flipper) {
        *tip_x = active ? PINBALL_LEFT_ACTIVE_TIP_X : PINBALL_LEFT_REST_TIP_X;
        *tip_y = active ? PINBALL_LEFT_ACTIVE_TIP_Y : PINBALL_LEFT_REST_TIP_Y;
    } else {
        *tip_x = active ? PINBALL_RIGHT_ACTIVE_TIP_X : PINBALL_RIGHT_REST_TIP_X;
        *tip_y = active ? PINBALL_RIGHT_ACTIVE_TIP_Y : PINBALL_RIGHT_REST_TIP_Y;
    }
}

static void pinball_reset_targets(pinball_state_t *state)
{
    memset(state->targets_lit, 0, sizeof(state->targets_lit));
}

static bool pinball_all_targets_lit(const pinball_state_t *state)
{
    for (size_t i = 0; i < ARRAY_SIZE(state->targets_lit); ++i) {
        if (!state->targets_lit[i]) {
            return false;
        }
    }
    return true;
}

static void pinball_launch_ball(pinball_state_t *state)
{
    if (state->balls_left == 0u) {
        return;
    }

    state->ball_active = true;
    state->balls_left--;
    state->ball_x = PINBALL_LAUNCH_X * 1000;
    state->ball_y = PINBALL_LAUNCH_Y * 1000;
    state->ball_vx = PINBALL_LAUNCH_VX;
    state->ball_vy = PINBALL_LAUNCH_VY;
}

static void pinball_init_round(pinball_state_t *state)
{
    memset(state, 0, sizeof(*state));
    state->balls_left = PINBALL_BALLS_PER_ROUND;
    state->next_launch_ms = PINBALL_LAUNCH_DELAY_MS;
    pinball_reset_targets(state);
}

static bool pinball_ball_hits_circle(const pinball_state_t *state, int center_x, int center_y, int radius)
{
    int ball_x = (int)(state->ball_x / 1000);
    int ball_y = (int)(state->ball_y / 1000);
    int dx = ball_x - center_x;
    int dy = ball_y - center_y;
    int reach = radius + PINBALL_BALL_RADIUS;

    return (dx * dx) + (dy * dy) <= (reach * reach);
}

static void pinball_bounce_from_circle(pinball_state_t *state, int center_x, int center_y, int radius, int kick)
{
    int ball_x = (int)(state->ball_x / 1000);
    int ball_y = (int)(state->ball_y / 1000);
    int dx = ball_x - center_x;
    int dy = ball_y - center_y;
    int reach = radius + PINBALL_BALL_RADIUS + 1;

    if (dx == 0 && dy == 0) {
        dy = -1;
    }

    if (pinball_abs(dx) >= pinball_abs(dy)) {
        state->ball_x = (center_x + (pinball_sign(dx) * reach)) * 1000;
        state->ball_vx = pinball_sign(dx) * (pinball_abs(state->ball_vx) + kick);
        state->ball_vy += dy * 2;
    } else {
        state->ball_y = (center_y + (pinball_sign(dy) * reach)) * 1000;
        state->ball_vy = pinball_sign(dy) * (pinball_abs(state->ball_vy) + kick);
        state->ball_vx += dx * 2;
    }

    if (kick >= 100 && state->ball_vy > -160) {
        state->ball_vy -= 90;
    }
    pinball_clamp_velocity(state);
}

static bool pinball_hit_circle_features(pinball_state_t *state,
                                        const pinball_circle_feature_t *features,
                                        size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        if (!pinball_ball_hits_circle(state, features[i].x, features[i].y, features[i].radius)) {
            continue;
        }

        pinball_bounce_from_circle(state, features[i].x, features[i].y, features[i].radius, features[i].kick);
        state->score += features[i].score;
        return true;
    }

    return false;
}

static bool pinball_hit_segment(pinball_state_t *state,
                                int x1,
                                int y1,
                                int x2,
                                int y2,
                                int radius,
                                int kick)
{
    int steps = pinball_abs(x2 - x1);

    if (pinball_abs(y2 - y1) > steps) {
        steps = pinball_abs(y2 - y1);
    }
    steps = (steps / 6) + 1;

    for (int i = 0; i <= steps; ++i) {
        int sample_x = x1 + (((x2 - x1) * i) / steps);
        int sample_y = y1 + (((y2 - y1) * i) / steps);

        if (!pinball_ball_hits_circle(state, sample_x, sample_y, radius)) {
            continue;
        }

        pinball_bounce_from_circle(state, sample_x, sample_y, radius, kick);
        return true;
    }

    return false;
}

static bool pinball_hit_targets(pinball_state_t *state)
{
    int ball_x = (int)(state->ball_x / 1000);
    int ball_y = (int)(state->ball_y / 1000);
    int ball_left = ball_x - PINBALL_BALL_RADIUS;
    int ball_right = ball_x + PINBALL_BALL_RADIUS;
    int ball_top = ball_y - PINBALL_BALL_RADIUS;
    int ball_bottom = ball_y + PINBALL_BALL_RADIUS;

    for (int i = 0; i < PINBALL_TARGET_COUNT; ++i) {
        int left = pinball_target_x[i] - 12;
        int top = 48;
        int right = left + 24;
        int bottom = top + 10;

        if (!pinball_rect_overlap(ball_left, ball_top, ball_right, ball_bottom,
                                  left, top, right, bottom)) {
            continue;
        }

        if (!state->targets_lit[i]) {
            state->targets_lit[i] = true;
            state->score += 20u;
            if (pinball_all_targets_lit(state)) {
                state->score += 80u;
                pinball_reset_targets(state);
            }
        } else {
            state->score += 4u;
        }

        if (ball_y < (top + bottom) / 2) {
            state->ball_y = (top - PINBALL_BALL_RADIUS - 1) * 1000;
            state->ball_vy = -pinball_abs(state->ball_vy) - 30;
        } else {
            state->ball_y = (bottom + PINBALL_BALL_RADIUS + 1) * 1000;
            state->ball_vy = pinball_abs(state->ball_vy) + 30;
        }
        state->ball_vx += (ball_x - pinball_target_x[i]) * 4;
        pinball_clamp_velocity(state);
        return true;
    }

    return false;
}

static bool pinball_hit_guides(pinball_state_t *state)
{
    if (pinball_hit_segment(state, 48, 40, 34, 198, 3, 40)) {
        return true;
    }
    if (pinball_hit_segment(state, 312, 40, 326, 198, 3, 40)) {
        return true;
    }
    if (pinball_hit_segment(state, 36, 228, 116, 322, 3, 44)) {
        return true;
    }
    if (pinball_hit_segment(state, 324, 228, 244, 322, 3, 44)) {
        return true;
    }
    if (pinball_hit_segment(state, 138, 244, 166, 330, 3, 44)) {
        return true;
    }
    if (pinball_hit_segment(state, 222, 244, 194, 330, 3, 44)) {
        return true;
    }
    return false;
}

static bool pinball_hit_slingshots(pinball_state_t *state)
{
    int ball_x = (int)(state->ball_x / 1000);
    int ball_y = (int)(state->ball_y / 1000);

    if (ball_y < 248 || ball_y > 288) {
        return false;
    }

    if (ball_x >= 90 && ball_x <= 152) {
        state->score += 5u;
        state->ball_vx = pinball_abs(state->ball_vx) + 120;
        state->ball_vy = -pinball_abs(state->ball_vy) - 170;
        state->ball_y = 246 * 1000;
        pinball_clamp_velocity(state);
        return true;
    }

    if (ball_x >= 208 && ball_x <= 270) {
        state->score += 5u;
        state->ball_vx = -pinball_abs(state->ball_vx) - 120;
        state->ball_vy = -pinball_abs(state->ball_vy) - 170;
        state->ball_y = 246 * 1000;
        pinball_clamp_velocity(state);
        return true;
    }

    return false;
}

static bool pinball_hit_flipper(pinball_state_t *state, bool left_flipper, bool active)
{
    int pivot_x = left_flipper ? PINBALL_LEFT_PIVOT_X : PINBALL_RIGHT_PIVOT_X;
    int tip_x;
    int tip_y;
    int ball_x = (int)(state->ball_x / 1000);
    int ball_y = (int)(state->ball_y / 1000);

    if (ball_y < 278 || ball_y > 340) {
        return false;
    }

    pinball_get_flipper_tip(active, left_flipper, &tip_x, &tip_y);
    for (int i = 0; i <= PINBALL_FLIPPER_SAMPLES; ++i) {
        int sample_x = pivot_x + (((tip_x - pivot_x) * i) / PINBALL_FLIPPER_SAMPLES);
        int sample_y = PINBALL_FLIPPER_PIVOT_Y + (((tip_y - PINBALL_FLIPPER_PIVOT_Y) * i) / PINBALL_FLIPPER_SAMPLES);
        int dx = ball_x - sample_x;
        int dy = ball_y - sample_y;
        int reach = PINBALL_BALL_RADIUS + 6;

        if ((dx * dx) + (dy * dy) > (reach * reach)) {
            continue;
        }

        state->ball_y = (sample_y - reach - 1) * 1000;
        if (active) {
            int kick = 220 + (i * 20);

            state->ball_vy = -kick;
            state->ball_vx = left_flipper ? (42 + (i * 26)) : -(42 + (i * 26));
        } else {
            state->ball_vy = -pinball_abs(state->ball_vy) - 38;
            state->ball_vx += left_flipper ? 20 : -20;
        }
        pinball_clamp_velocity(state);
        return true;
    }

    return false;
}

static void pinball_collide_walls(pinball_state_t *state)
{
    int ball_x = (int)(state->ball_x / 1000);
    int ball_y = (int)(state->ball_y / 1000);

    if (ball_x - PINBALL_BALL_RADIUS < PINBALL_LEFT_WALL_X) {
        state->ball_x = (PINBALL_LEFT_WALL_X + PINBALL_BALL_RADIUS) * 1000;
        state->ball_vx = pinball_abs(state->ball_vx) + 18;
    } else if (ball_x + PINBALL_BALL_RADIUS > PINBALL_RIGHT_WALL_X) {
        state->ball_x = (PINBALL_RIGHT_WALL_X - PINBALL_BALL_RADIUS) * 1000;
        state->ball_vx = -pinball_abs(state->ball_vx) - 18;
    }

    if (ball_y - PINBALL_BALL_RADIUS < PINBALL_TOP_WALL_Y) {
        state->ball_y = (PINBALL_TOP_WALL_Y + PINBALL_BALL_RADIUS) * 1000;
        state->ball_vy = pinball_abs(state->ball_vy) + 16;
    }

    ball_x = (int)(state->ball_x / 1000);
    ball_y = (int)(state->ball_y / 1000);
    if (ball_y > PINBALL_SHOOTER_GATE_Y) {
        if (ball_x > PINBALL_SHOOTER_DIVIDER_X &&
            ball_x - PINBALL_BALL_RADIUS < PINBALL_SHOOTER_DIVIDER_X) {
            state->ball_x = (PINBALL_SHOOTER_DIVIDER_X + PINBALL_BALL_RADIUS + 1) * 1000;
            state->ball_vx = pinball_abs(state->ball_vx) + 24;
        } else if (ball_x < PINBALL_SHOOTER_DIVIDER_X &&
                   ball_x + PINBALL_BALL_RADIUS > PINBALL_SHOOTER_DIVIDER_X) {
            state->ball_x = (PINBALL_SHOOTER_DIVIDER_X - PINBALL_BALL_RADIUS - 1) * 1000;
            state->ball_vx = -pinball_abs(state->ball_vx) - 24;
        }
    }

    pinball_clamp_velocity(state);
}

static bool pinball_update_ball(pinball_state_t *state,
                                uint32_t delta_ms,
                                uint32_t elapsed_ms,
                                bool left_active,
                                bool right_active)
{
    if (!state->ball_active) {
        if (state->balls_left == 0u) {
            return false;
        }
        if (elapsed_ms >= state->next_launch_ms) {
            pinball_launch_ball(state);
        }
        return true;
    }

    {
        uint32_t steps = delta_ms > 18u ? 3u : (delta_ms > 8u ? 2u : 1u);
        uint32_t base_step_ms = delta_ms / steps;
        uint32_t extra_ms = delta_ms % steps;

        for (uint32_t step = 0; step < steps; ++step) {
            uint32_t step_ms = base_step_ms + (step < extra_ms ? 1u : 0u);
            int ball_x;
            int ball_y;

            if (step_ms == 0u) {
                continue;
            }

            state->ball_vy += (PINBALL_GRAVITY * (int)step_ms) / 1000;
            state->ball_x += state->ball_vx * (int32_t)step_ms;
            state->ball_y += state->ball_vy * (int32_t)step_ms;

            pinball_collide_walls(state);
            (void)pinball_hit_guides(state);
            (void)pinball_hit_targets(state);
            (void)pinball_hit_circle_features(state, pinball_bumpers, ARRAY_SIZE(pinball_bumpers));
            (void)pinball_hit_circle_features(state, pinball_posts, ARRAY_SIZE(pinball_posts));
            (void)pinball_hit_slingshots(state);
            (void)pinball_hit_flipper(state, true, left_active);
            (void)pinball_hit_flipper(state, false, right_active);
            pinball_clamp_velocity(state);

            ball_x = (int)(state->ball_x / 1000);
            ball_y = (int)(state->ball_y / 1000);
            if ((ball_y > 344 && ball_x >= PINBALL_DRAIN_LEFT_X && ball_x <= PINBALL_DRAIN_RIGHT_X) ||
                ball_y > (LCD_HEIGHT + 18)) {
                state->ball_active = false;
                state->next_launch_ms = elapsed_ms + PINBALL_NEXT_BALL_DELAY_MS;
                return state->balls_left > 0u;
            }
        }
    }

    return true;
}

static void pinball_draw_targets(const pinball_state_t *state)
{
    for (int i = 0; i < PINBALL_TARGET_COUNT; ++i) {
        uint16_t color = state->targets_lit[i] ? PINBALL_COLOR_YELLOW : PINBALL_COLOR_BLUE;

        app_fill_rect(pinball_target_x[i] - 12, 48, 24, 10, color);
        app_fill_rect(pinball_target_x[i] - 8, 52, 16, 2, COLOR_WHITE);
    }
}

static void pinball_draw_bumpers(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(pinball_bumpers); ++i) {
        app_draw_filled_circle(pinball_bumpers[i].x, pinball_bumpers[i].y, pinball_bumpers[i].radius + 4, COLOR_WHITE);
        app_draw_filled_circle(pinball_bumpers[i].x, pinball_bumpers[i].y, pinball_bumpers[i].radius, PINBALL_COLOR_GREEN);
        app_draw_filled_circle(pinball_bumpers[i].x, pinball_bumpers[i].y, pinball_bumpers[i].radius / 2, COLOR_RED);
    }
}

static void pinball_draw_posts(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(pinball_posts); ++i) {
        app_draw_filled_circle(pinball_posts[i].x, pinball_posts[i].y, pinball_posts[i].radius + 1, COLOR_WHITE);
        app_draw_filled_circle(pinball_posts[i].x, pinball_posts[i].y, pinball_posts[i].radius - 1, PINBALL_COLOR_BLUE);
    }
}

static void pinball_draw_flipper(bool left_flipper, bool active)
{
    int pivot_x = left_flipper ? PINBALL_LEFT_PIVOT_X : PINBALL_RIGHT_PIVOT_X;
    int tip_x;
    int tip_y;
    uint16_t color = active ? COLOR_RED : COLOR_WHITE;

    pinball_get_flipper_tip(active, left_flipper, &tip_x, &tip_y);
    pinball_draw_segment(pivot_x, PINBALL_FLIPPER_PIVOT_Y, tip_x, tip_y, 4, color);
    app_draw_filled_circle(pivot_x, PINBALL_FLIPPER_PIVOT_Y, 5, PINBALL_COLOR_YELLOW);
}

static void pinball_draw_board(const pinball_state_t *state, bool left_active, bool right_active)
{
    char score_text[16];
    char ball_text[16];
    uint32_t balls_display = state->balls_left + (state->ball_active ? 1u : 0u);

    pinball_format_score(score_text, sizeof(score_text), state->score);
    snprintf(ball_text, sizeof(ball_text), "%lu", (unsigned long)balls_display);

    app_fill_screen(COLOR_BLACK);
    app_draw_text_centered(62, 12, "BALL", 2, COLOR_WHITE);
    app_draw_text_centered(62, 36, ball_text, 3, COLOR_WHITE);
    app_draw_text_centered(LCD_WIDTH / 2, 12, "SCORE", 2, COLOR_WHITE);
    app_draw_text_centered(LCD_WIDTH / 2, 36, score_text, 3, COLOR_WHITE);

    pinball_draw_segment(34, 22, 326, 22, 2, COLOR_WHITE);
    pinball_draw_segment(PINBALL_LEFT_WALL_X, 24, PINBALL_LEFT_WALL_X, 336, 2, COLOR_WHITE);
    pinball_draw_segment(PINBALL_RIGHT_WALL_X, 24, PINBALL_RIGHT_WALL_X, 336, 2, COLOR_WHITE);
    pinball_draw_segment(PINBALL_SHOOTER_DIVIDER_X, 58, PINBALL_SHOOTER_DIVIDER_X, 322, 2, COLOR_WHITE);

    pinball_draw_segment(48, 40, 34, 198, 2, PINBALL_COLOR_BLUE);
    pinball_draw_segment(312, 40, 326, 198, 2, PINBALL_COLOR_BLUE);
    pinball_draw_segment(36, 228, 116, 322, 2, COLOR_WHITE);
    pinball_draw_segment(324, 228, 244, 322, 2, COLOR_WHITE);
    pinball_draw_segment(92, 286, 150, 258, 2, COLOR_RED);
    pinball_draw_segment(268, 286, 210, 258, 2, COLOR_RED);
    pinball_draw_segment(138, 244, 166, 330, 2, PINBALL_COLOR_BLUE);
    pinball_draw_segment(222, 244, 194, 330, 2, PINBALL_COLOR_BLUE);
    pinball_draw_segment(24, 336, PINBALL_DRAIN_LEFT_X, 336, 2, COLOR_WHITE);
    pinball_draw_segment(PINBALL_DRAIN_RIGHT_X, 336, 336, 336, 2, COLOR_WHITE);

    pinball_draw_targets(state);
    pinball_draw_bumpers();
    pinball_draw_posts();
    pinball_draw_flipper(true, left_active);
    pinball_draw_flipper(false, right_active);

    if (state->ball_active) {
        app_draw_filled_circle((int)(state->ball_x / 1000),
                               (int)(state->ball_y / 1000),
                               PINBALL_BALL_RADIUS,
                               COLOR_WHITE);
    } else if (state->balls_left > 0u) {
        app_draw_filled_circle(PINBALL_LAUNCH_X, PINBALL_LAUNCH_Y, PINBALL_BALL_RADIUS, PINBALL_COLOR_YELLOW);
    }

    app_present_frame();
}

void game_9_pinball_get_best_record(char *buffer, size_t buffer_size)
{
    pinball_load_best();

    if (best_score == 0u) {
        snprintf(buffer, buffer_size, "-");
        return;
    }

    pinball_format_score(buffer, buffer_size, best_score);
}

void run_game_9_pinball(game_run_context_t *context)
{
    pinball_load_best();

    while (true) {
        touch_event_t touch;
        pinball_state_t state;
        absolute_time_t round_start;
        absolute_time_t previous_frame;
        char result_text[16];
        char best_text[16];
        app_result_view_t result_view;

        (void)game_consume_first_start(context);
        app_wait_for_touch_release();
        hit20_show_start_count_in();

        pinball_init_round(&state);
        round_start = get_absolute_time();
        previous_frame = round_start;

        while (true) {
            absolute_time_t frame_start = get_absolute_time();
            int64_t delta_us = absolute_time_diff_us(previous_frame, frame_start);
            int64_t elapsed_us = absolute_time_diff_us(round_start, frame_start);
            uint32_t delta_ms = delta_us <= 0 ? 1u : (uint32_t)(delta_us / 1000);
            uint32_t elapsed_ms = elapsed_us <= 0 ? 0u : (uint32_t)(elapsed_us / 1000);
            bool left_active = false;
            bool right_active = false;

            if (delta_ms > 40u) {
                delta_ms = 40u;
            }
            previous_frame = frame_start;

            if (app_read_touch_state(&touch)) {
                left_active = touch.x < (LCD_WIDTH / 2);
                right_active = !left_active;
            }

            if (!pinball_update_ball(&state, delta_ms, elapsed_ms, left_active, right_active)) {
                break;
            }

            pinball_draw_board(&state, left_active, right_active);
            sleep_until(delayed_by_us(frame_start, PINBALL_FRAME_US));
        }

        if (state.score > best_score) {
            best_score = state.score;
            pinball_save_best();
        }

        pinball_format_score(result_text, sizeof(result_text), state.score);
        pinball_format_score(best_text, sizeof(best_text), best_score);
        result_view.game_name = context->game_name;
        result_view.record_label = "SCORE";
        result_view.current_record = result_text;
        result_view.best_record = best_text;
        printf("9_pinball: %lu score\n", (unsigned long)state.score);
        app_draw_result_screen(&result_view);

        if (app_wait_post_game_action() == POST_ACTION_MENU) {
            return;
        }
    }
}
