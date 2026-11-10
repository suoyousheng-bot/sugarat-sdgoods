/* SPDX-License-Identifier: Apache-2.0 */
/** Sugarat Game feedback, data, time, media, companion and launcher components. */
#include "sg_internal.h"
#include <string.h>

static lv_obj_t *wrapped(lv_obj_t *parent, const char *text, sg_font_t font, sg_tone_t tone, lv_coord_t width)
{
    lv_obj_t *label = sg_label(parent, text, font, tone);
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

static lv_obj_t *boxed(lv_obj_t *parent, sg_box_style_t style, sg_tone_t tone, lv_coord_t pad_h, lv_coord_t pad_v)
{
    lv_obj_t *obj = sg_plain(parent);
    sg_box(obj, style, tone, true);
    lv_obj_set_style_pad_left(obj, pad_h, 0);
    lv_obj_set_style_pad_right(obj, pad_h + SG_UNIT, 0);
    lv_obj_set_style_pad_top(obj, pad_v, 0);
    lv_obj_set_style_pad_bottom(obj, pad_v + SG_UNIT, 0);
    return obj;
}

/* ---------------------------------------------------------------- Feedback */

lv_obj_t *sg_badge(lv_obj_t *parent, const char *text, sg_tone_t tone)
{
    lv_obj_t *badge = sg_plain(parent);
    lv_obj_set_style_bg_color(badge, sg_color(tone), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(badge, SG_UNIT, 0);
    lv_obj_set_style_border_color(badge, lv_color_hex(SG_INK), 0);
    if (!text) {
        lv_obj_set_size(badge, 14, 14);
        return badge;
    }
    lv_obj_set_style_pad_hor(badge, 6, 0);
    lv_obj_set_style_pad_ver(badge, 2, 0);
    sg_label(badge, text, SG_FONT_CAPTION, sg_on_tone(tone));
    return badge;
}

static void slide_in(void *obj, int32_t v)
{
    lv_obj_set_style_translate_y(obj, v - v % SG_UNIT, 0);
}

static void drop_in(lv_obj_t *obj)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, slide_in);
    lv_anim_set_values(&a, -80, 0);
    lv_anim_set_time(&a, 240);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

lv_obj_t *sg_toast(lv_obj_t *parent, sg_icon_t icon, sg_tone_t tone, const char *text, uint32_t duration_ms)
{
    lv_obj_t *toast = boxed(parent ? parent : lv_layer_top(), SG_BOX_FRAME, SG_TONE_TEXT, 14, 10);
    sg_flex(toast, LV_FLEX_FLOW_ROW, 10);
    lv_obj_set_style_max_width(toast, 270, 0);
    if (icon != SG_ICON_NONE) sg_icon(toast, icon, SG_SIZE_M, tone);
    sg_label(toast, text, SG_FONT_BODY, SG_TONE_TEXT);
    lv_obj_align(toast, LV_ALIGN_TOP_MID, 0, 36);
    drop_in(toast);
    if (duration_ms) lv_obj_del_delayed(toast, duration_ms);
    return toast;
}

lv_obj_t *sg_notification(lv_obj_t *parent, sg_icon_t icon, sg_tone_t tone, const char *app, const char *title, const char *body)
{
    lv_obj_t *card = boxed(parent, SG_BOX_FRAME, SG_TONE_TEXT, 16, 14);
    lv_obj_set_width(card, 288);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    sg_flex(card, LV_FLEX_FLOW_COLUMN, 6);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t *head = sg_row(card);
    lv_obj_set_style_pad_column(head, 8, 0);
    sg_icon(head, icon, SG_SIZE_S, tone);
    sg_label(head, app, SG_FONT_CAPTION, tone);
    lv_obj_t *label = sg_label(card, title, SG_FONT_BODY, SG_TONE_TEXT);
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    if (body) {
        label = sg_label(card, body, SG_FONT_CAPTION, SG_TONE_MUTED);
        lv_obj_set_width(label, LV_PCT(100));
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    }
    return card;
}

static void alert_close(lv_event_t *e)
{
    if (sg_event_action(e) != SG_ACTION_NONE) lv_obj_del_async(lv_event_get_current_target(e));
}

lv_obj_t *sg_alert(lv_obj_t *parent, sg_icon_t icon, sg_tone_t tone, const char *title, const char *message,
                   const char *confirm, const char *cancel)
{
    lv_obj_t *modal = sg_plain(parent ? parent : lv_layer_top());
    lv_obj_add_flag(modal, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(modal, SG_SCREEN, SG_SCREEN);
    lv_obj_center(modal);
    lv_obj_set_style_bg_color(modal, sg_color(SG_TONE_BG), 0);
    lv_obj_set_style_bg_opa(modal, LV_OPA_COVER, 0);
    lv_obj_t *window = boxed(modal, SG_BOX_FRAME, tone, 18, 18);
    lv_obj_set_width(window, 292);
    lv_obj_center(window);
    sg_flex(window, LV_FLEX_FLOW_COLUMN, 10);
    if (icon != SG_ICON_NONE) sg_icon(window, icon, SG_SIZE_XL, tone);
    wrapped(window, title, SG_FONT_BODY, tone, 240);
    if (message) wrapped(window, message, SG_FONT_CAPTION, SG_TONE_TEXT, 240);
    lv_obj_t *row = sg_row(window);
    lv_obj_set_style_pad_top(row, 6, 0);
    lv_obj_set_style_pad_column(row, 12, 0);
    lv_obj_t *button;
    if (cancel) {
        button = sg_button(row, SG_ICON_NONE, cancel, SG_BUTTON_PLAIN, SG_TONE_TEXT);
        sg_set_action(button, SG_ACTION_CANCEL);
    }
    button = sg_button(row, SG_ICON_NONE, confirm, SG_BUTTON_FILLED, tone);
    sg_set_action(button, SG_ACTION_CONFIRM);
    lv_obj_add_event_cb(modal, alert_close, LV_EVENT_CLICKED, NULL);
    return modal;
}

lv_obj_t *sg_empty_state(lv_obj_t *parent, sg_icon_t icon, const char *title, const char *hint)
{
    lv_obj_t *block = sg_column(parent);
    lv_obj_set_style_pad_row(block, 12, 0);
    lv_obj_t *slot = boxed(block, SG_BOX_PANEL, SG_TONE_SURFACE, 22, 22);
    sg_icon(slot, icon, SG_SIZE_XL, SG_TONE_MUTED);
    wrapped(block, title, SG_FONT_BODY, SG_TONE_TEXT, 240);
    if (hint) wrapped(block, hint, SG_FONT_CAPTION, SG_TONE_MUTED, 240);
    return block;
}

lv_obj_t *sg_loading(lv_obj_t *parent, const char *text)
{
    lv_obj_t *block = sg_column(parent);
    lv_obj_set_style_pad_row(block, 16, 0);
    sg_spinner(block, 64, SG_TONE_ACCENT);
    if (text) sg_label(block, text, SG_FONT_BODY, SG_TONE_MUTED);
    return block;
}

/* ---------------------------------------------------------------- Data */

lv_obj_t *sg_stat(lv_obj_t *parent, const char *value, const char *unit, const char *label, sg_tone_t tone)
{
    lv_obj_t *stat = sg_plain(parent);
    lv_obj_set_flex_flow(stat, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(stat, 2, 0);
    sg_label(stat, label, SG_FONT_CAPTION, tone);
    lv_obj_t *row = sg_plain(stat);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(row, 6, 0);
    sg_label(row, value, SG_FONT_NUM_M, SG_TONE_TEXT);
    if (unit) sg_label(row, unit, SG_FONT_CAPTION, tone);
    return stat;
}

void sg_stat_set(lv_obj_t *stat, const char *value)
{
    lv_label_set_text(lv_obj_get_child(lv_obj_get_child(stat, 1), 0), value);
}

lv_obj_t *sg_gauge(lv_obj_t *parent, lv_coord_t size, int32_t min, int32_t max, int32_t value, const char *label, sg_tone_t tone)
{
    uint8_t count = (uint8_t)LV_CLAMP(9, size / 8, 31);
    sg_blocks_t b = {135, 270, count, 0, (uint8_t)LV_CLAMP(6, size / 11 / SG_UNIT * SG_UNIT, 10), -1, tone};
    lv_obj_t *gauge = sg_block_ring(parent, size, &b);
    lv_obj_t *range = sg_plain(gauge);
    lv_obj_add_flag(range, LV_OBJ_FLAG_HIDDEN);
    int32_t *bounds = sg_state(range, sizeof(int32_t) * 2);
    bounds[0] = min;
    bounds[1] = max;
    lv_obj_t *number = sg_label(gauge, "", size >= 120 ? SG_FONT_NUM_M : SG_FONT_NUM_S, SG_TONE_TEXT);
    lv_obj_align(number, LV_ALIGN_CENTER, 0, -2);
    lv_obj_t *caption = sg_label(gauge, label, SG_FONT_CAPTION, tone);
    lv_obj_align(caption, LV_ALIGN_BOTTOM_MID, 0, size >= 120 ? -10 : 2);
    sg_gauge_set(gauge, value);
    return gauge;
}

void sg_gauge_set(lv_obj_t *gauge, int32_t value)
{
    const int32_t *range = lv_obj_get_user_data(lv_obj_get_child(gauge, 0));
    sg_blocks_t *b = lv_obj_get_user_data(gauge);
    int32_t span = range[1] > range[0] ? range[1] - range[0] : 1, v = LV_CLAMP(range[0], value, range[1]);
    b->filled = (uint8_t)(((v - range[0]) * b->count + span / 2) / span);
    lv_label_set_text_fmt(lv_obj_get_child(gauge, 1), "%" LV_PRId32, value);
    lv_obj_invalidate(gauge);
}

lv_obj_t *sg_tile(lv_obj_t *parent, sg_icon_t icon, sg_tone_t tone, const char *value, const char *label)
{
    lv_obj_t *tile = boxed(parent, SG_BOX_PANEL, SG_TONE_SURFACE, 12, 12);
    lv_obj_set_size(tile, 132, 112);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tile, 4, 0);
    sg_icon(tile, icon, SG_SIZE_M, tone);
    sg_label(tile, value, SG_FONT_NUM_S, SG_TONE_TEXT);
    lv_obj_t *caption = sg_label(tile, label, SG_FONT_CAPTION, SG_TONE_MUTED);
    lv_label_set_long_mode(caption, LV_LABEL_LONG_DOT);
    lv_obj_set_width(caption, LV_PCT(100));
    return tile;
}

void sg_tile_set(lv_obj_t *tile, const char *value)
{
    lv_label_set_text(lv_obj_get_child(tile, 1), value);
}

typedef struct {
    int16_t values[32];
    uint8_t count;
    bool bars;
    sg_tone_t tone;
} chart_t;

static void chart_draw(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    const chart_t *c = lv_obj_get_user_data(obj);
    if (!c->count) return;
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    int16_t lo = c->values[0], hi = c->values[0];
    for (uint8_t i = 0; i < c->count; ++i) {
        lo = LV_MIN(lo, c->values[i]);
        hi = LV_MAX(hi, c->values[i]);
    }
    if (c->bars) lo = LV_MIN(0, lo);
    int32_t span = hi > lo ? hi - lo : 1;
    lv_coord_t w = lv_area_get_width(&a), h = lv_area_get_height(&a), col = w / c->count;
    lv_color_t on = sg_color(c->tone);
    const lv_coord_t cell = 3 * SG_UNIT;
    lv_coord_t prev = 0;
    for (uint8_t i = 0; i < c->count; ++i) {
        lv_coord_t x = a.x1 + i * col;
        int32_t level = (c->values[i] - lo) * (h / cell - 1) / span;
        if (c->bars) {
            lv_coord_t bw = col - 2 * SG_UNIT;
            bw -= bw % SG_UNIT;
            for (int32_t k = 0; k <= level; ++k) {
                lv_coord_t y2 = a.y2 - k * cell;
                sg_rect(ctx, x, y2 - cell + SG_UNIT + 1, x + bw - 1, y2, k == level ? sg_shade(on, 90) : on);
            }
        } else {
            lv_coord_t y = a.y2 - (lv_coord_t)level * cell - cell + 1;
            sg_rect(ctx, x, y, x + col - 1, y + cell - 1, on);
            if (i > 0 && prev != y) sg_rect(ctx, x, LV_MIN(prev, y), x + cell - 1, LV_MAX(prev, y) + cell - 1, on);
            prev = y;
        }
    }
}

static lv_obj_t *chart(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, bool bars, sg_tone_t tone)
{
    lv_obj_t *obj = sg_plain(parent);
    lv_obj_set_size(obj, w, h);
    chart_t *c = sg_state(obj, sizeof(chart_t));
    c->bars = bars;
    c->tone = tone;
    lv_obj_add_event_cb(obj, chart_draw, LV_EVENT_DRAW_MAIN, NULL);
    return obj;
}

static void chart_set(lv_obj_t *obj, const int16_t *values, uint8_t count)
{
    chart_t *c = lv_obj_get_user_data(obj);
    c->count = LV_MIN(count, 32);
    memcpy(c->values, values, c->count * sizeof(int16_t));
    lv_obj_invalidate(obj);
}

lv_obj_t *sg_bar_chart(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, const int16_t *values, uint8_t count, sg_tone_t tone)
{
    lv_obj_t *obj = chart(parent, w, h, true, tone);
    chart_set(obj, values, count);
    return obj;
}

void sg_bar_chart_set(lv_obj_t *chart_obj, const int16_t *values, uint8_t count)
{
    chart_set(chart_obj, values, count);
}

lv_obj_t *sg_sparkline(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, const int16_t *values, uint8_t count, sg_tone_t tone)
{
    lv_obj_t *obj = chart(parent, w, h, false, tone);
    chart_set(obj, values, count);
    return obj;
}

void sg_sparkline_set(lv_obj_t *chart_obj, const int16_t *values, uint8_t count)
{
    chart_set(chart_obj, values, count);
}

/* ---------------------------------------------------------------- Time */

lv_obj_t *sg_clock(lv_obj_t *parent, const char *time, const char *date)
{
    lv_obj_t *clock = sg_column(parent);
    lv_obj_set_style_pad_row(clock, 6, 0);
    sg_label(clock, date, SG_FONT_CAPTION, SG_TONE_ACCENT);
    sg_label(clock, time, SG_FONT_NUM_L, SG_TONE_TEXT);
    return clock;
}

void sg_clock_set(lv_obj_t *clock, const char *time, const char *date)
{
    if (date) lv_label_set_text(lv_obj_get_child(clock, 0), date);
    if (time) lv_label_set_text(lv_obj_get_child(clock, 1), time);
}

typedef struct {
    uint8_t hour, minute, second;
} analog_t;

static void hand(lv_draw_ctx_t *ctx, lv_coord_t cx, lv_coord_t cy, int32_t angle, lv_coord_t length, lv_coord_t thick, lv_color_t color)
{
    for (lv_coord_t r = 0; r <= length; r += SG_UNIT) {
        lv_coord_t x = cx + (lv_coord_t)((int32_t)r * lv_trigo_cos((int16_t)angle) >> LV_TRIGO_SHIFT);
        lv_coord_t y = cy + (lv_coord_t)((int32_t)r * lv_trigo_sin((int16_t)angle) >> LV_TRIGO_SHIFT);
        x -= (x - cx) % SG_UNIT + thick / 2;
        y -= (y - cy) % SG_UNIT + thick / 2;
        sg_rect(ctx, x, y, x + thick - 1, y + thick - 1, color);
    }
}

static void analog_draw(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    const analog_t *t = lv_obj_get_user_data(obj);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    lv_coord_t cx = (a.x1 + a.x2 + 1) / 2, cy = (a.y1 + a.y2 + 1) / 2, r = lv_area_get_width(&a) / 2;
    for (int i = 0; i < 60; ++i) {
        bool major = i % 5 == 0, quarter = i % 15 == 0;
        lv_coord_t s = quarter ? 12 : major ? 8 : 4, rr = r - 10 - s / 2;
        lv_coord_t x = cx + (lv_coord_t)((int32_t)rr * lv_trigo_cos((int16_t)(i * 6 - 90)) >> LV_TRIGO_SHIFT);
        lv_coord_t y = cy + (lv_coord_t)((int32_t)rr * lv_trigo_sin((int16_t)(i * 6 - 90)) >> LV_TRIGO_SHIFT);
        x -= (x - cx) % SG_UNIT + s / 2;
        y -= (y - cy) % SG_UNIT + s / 2;
        lv_color_t c = quarter ? sg_color(SG_TONE_ACCENT) : major ? sg_color(SG_TONE_TEXT) : sg_color(SG_TONE_RAISED);
        if (major) sg_rect(ctx, x - SG_UNIT, y - SG_UNIT, x + s + SG_UNIT - 1, y + s + SG_UNIT - 1, lv_color_hex(SG_INK));
        sg_rect(ctx, x, y, x + s - 1, y + s - 1, c);
    }
    int32_t hour = (t->hour % 12) * 30 + t->minute / 2 - 90, minute = t->minute * 6 + t->second / 10 - 90;
    hand(ctx, cx, cy, hour, r * 45 / 100, 10, lv_color_hex(SG_INK));
    hand(ctx, cx, cy, hour, r * 45 / 100, 6, sg_color(SG_TONE_TEXT));
    hand(ctx, cx, cy, minute, r * 68 / 100, 8, lv_color_hex(SG_INK));
    hand(ctx, cx, cy, minute, r * 68 / 100, 4, sg_color(SG_TONE_TEXT));
    hand(ctx, cx, cy, t->second * 6 - 90, r * 74 / 100, 2, sg_color(SG_TONE_RED));
    sg_rect(ctx, cx - 6, cy - 6, cx + 5, cy + 5, lv_color_hex(SG_INK));
    sg_rect(ctx, cx - 4, cy - 4, cx + 3, cy + 3, sg_color(SG_TONE_RED));
}

lv_obj_t *sg_analog_clock(lv_obj_t *parent, lv_coord_t size, uint8_t hour, uint8_t minute, uint8_t second)
{
    lv_obj_t *clock = sg_plain(parent);
    lv_obj_set_size(clock, size, size);
    sg_state(clock, sizeof(analog_t));
    lv_obj_add_event_cb(clock, analog_draw, LV_EVENT_DRAW_MAIN, NULL);
    sg_analog_clock_set(clock, hour, minute, second);
    return clock;
}

void sg_analog_clock_set(lv_obj_t *clock, uint8_t hour, uint8_t minute, uint8_t second)
{
    analog_t *t = lv_obj_get_user_data(clock);
    *t = (analog_t){hour, minute, second};
    lv_obj_invalidate(clock);
}

lv_obj_t *sg_timer(lv_obj_t *parent, lv_coord_t size, uint32_t total_s, uint32_t remaining_s, sg_tone_t tone)
{
    sg_blocks_t b = {270, 360, (uint8_t)LV_CLAMP(12, size / 10, 40), 0, (uint8_t)LV_CLAMP(6, size / 22 / SG_UNIT * SG_UNIT, 14), -1, tone};
    lv_obj_t *ring = sg_block_ring(parent, size, &b);
    lv_obj_t *total = sg_plain(ring);
    lv_obj_add_flag(total, LV_OBJ_FLAG_HIDDEN);
    *(uint32_t *)sg_state(total, sizeof(uint32_t)) = total_s ? total_s : 1;
    lv_obj_t *time = sg_label(ring, "", size >= 200 ? SG_FONT_NUM_M : SG_FONT_NUM_S, SG_TONE_TEXT);
    lv_obj_center(time);
    sg_timer_set(ring, remaining_s);
    return ring;
}

void sg_timer_set(lv_obj_t *timer, uint32_t remaining_s)
{
    uint32_t total = *(uint32_t *)lv_obj_get_user_data(lv_obj_get_child(timer, 0));
    sg_blocks_t *b = lv_obj_get_user_data(timer);
    b->filled = (uint8_t)((LV_MIN(remaining_s, total) * b->count + total - 1) / total);
    lv_label_set_text_fmt(lv_obj_get_child(timer, 1), "%02u:%02u", (unsigned)(remaining_s / 60), (unsigned)(remaining_s % 60));
    lv_obj_invalidate(timer);
}

/* ---------------------------------------------------------------- Media */

lv_obj_t *sg_now_playing(lv_obj_t *parent, const char *title, const char *artist, bool playing)
{
    lv_obj_t *player = sg_column(parent);
    lv_obj_set_style_pad_row(player, 6, 0);
    lv_obj_t *label = sg_label(player, title, SG_FONT_TITLE, SG_TONE_ACCENT);
    lv_obj_set_width(label, 240);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    label = sg_label(player, artist, SG_FONT_CAPTION, SG_TONE_MUTED);
    lv_obj_set_style_pad_bottom(label, 12, 0);
    lv_obj_t *controls = sg_row(player);
    lv_obj_set_style_pad_column(controls, 14, 0);
    lv_obj_t *button = sg_icon_button(controls, SG_ICON_PREV, SG_TONE_TEXT);
    sg_set_action(button, SG_ACTION_PREV);
    button = sg_button(controls, SG_ICON_PLAY, NULL, SG_BUTTON_FILLED, SG_TONE_ACCENT);
    lv_obj_set_size(button, 76, 76);
    sg_set_action(button, SG_ACTION_PLAY);
    button = sg_icon_button(controls, SG_ICON_NEXT, SG_TONE_TEXT);
    sg_set_action(button, SG_ACTION_NEXT);
    sg_now_playing_set(player, NULL, NULL, playing);
    return player;
}

void sg_now_playing_set(lv_obj_t *player, const char *title, const char *artist, bool playing)
{
    if (title) lv_label_set_text(lv_obj_get_child(player, 0), title);
    if (artist) lv_label_set_text(lv_obj_get_child(player, 1), artist);
    lv_obj_t *play = lv_obj_get_child(lv_obj_get_child(player, 2), 1);
    sg_icon_set(lv_obj_get_child(play, 0), playing ? SG_ICON_PAUSE : SG_ICON_PLAY, SG_TONE_BG);
}

lv_obj_t *sg_volume(lv_obj_t *screen, uint8_t percent)
{
    sg_blocks_t b = {40, -80, 9, 0, 10, -1, SG_TONE_GREEN};
    lv_obj_t *volume = sg_block_ring(screen, SG_SCREEN - 8, &b);
    lv_obj_center(volume);
    lv_obj_t *icon = sg_icon(volume, SG_ICON_VOLUME, SG_SIZE_S, SG_TONE_GREEN);
    lv_obj_align(icon, LV_ALIGN_RIGHT_MID, -28, -88);
    sg_volume_set(volume, percent);
    return volume;
}

void sg_volume_set(lv_obj_t *volume, uint8_t percent)
{
    sg_blocks_t *b = lv_obj_get_user_data(volume);
    b->filled = (uint8_t)((LV_MIN(percent, 100) * b->count + 50) / 100);
    lv_obj_invalidate(volume);
}

/* ---------------------------------------------------------------- Companion */

lv_obj_t *sg_voice_button(lv_obj_t *parent, sg_voice_t state)
{
    lv_obj_t *button = sg_button(parent, SG_ICON_MIC, "", SG_BUTTON_FILLED, SG_TONE_SURFACE);
    lv_obj_set_width(button, 200);
    sg_set_action(button, SG_ACTION_VOICE);
    sg_voice_button_set(button, state);
    return button;
}

void sg_voice_button_set(lv_obj_t *button, sg_voice_t state)
{
    static const struct { sg_icon_t icon; sg_tone_t tone; const char *text; } looks[] = {
        [SG_VOICE_IDLE] = {SG_ICON_MIC, SG_TONE_RAISED, "按住说话"},
        [SG_VOICE_LISTENING] = {SG_ICON_MIC, SG_TONE_BLUE, "正在聆听"},
        [SG_VOICE_THINKING] = {SG_ICON_SYNC, SG_TONE_PURPLE, "思考中..."},
        [SG_VOICE_SPEAKING] = {SG_ICON_VOLUME, SG_TONE_GREEN, "正在回答"},
        [SG_VOICE_OFFLINE] = {SG_ICON_WIFI, SG_TONE_SURFACE, "未连接"},
    };
    sg_voice_t s = state <= SG_VOICE_OFFLINE ? state : SG_VOICE_IDLE;
    sg_tone_t ink = s == SG_VOICE_OFFLINE ? SG_TONE_MUTED : SG_TONE_TEXT;
    sg_box_set_tone(button, looks[s].tone);
    sg_icon_set(lv_obj_get_child(button, 0), looks[s].icon, s == SG_VOICE_OFFLINE ? SG_TONE_RED : ink);
    lv_obj_t *label = lv_obj_get_child(button, 1);
    lv_label_set_text_static(label, looks[s].text);
    lv_obj_set_style_text_color(label, sg_color(ink), 0);
}

typedef struct {
    lv_timer_t *timer;
    uint32_t tick;
    bool active;
    sg_tone_t tone;
} wave_t;

static void wave_draw(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    const wave_t *w = lv_obj_get_user_data(obj);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    const lv_coord_t cell = 4 * SG_UNIT, step = cell + SG_UNIT;
    int n = lv_area_get_width(&a) / step, rows = (lv_area_get_height(&a) - cell) / step;
    lv_coord_t x0 = a.x1 + (lv_area_get_width(&a) - (n * step - 2 * SG_UNIT)) / 2;
    lv_coord_t mid = (a.y1 + a.y2) / 2;
    uint32_t seed = w->tick * 2654435761u;
    for (int i = 0; i < n; ++i) {
        int center = LV_ABS(i * 2 - (n - 1));
        int peak = rows / 2 - center * rows / (4 * n);
        seed = seed * 1103515245u + 12345u + (uint32_t)i;
        int level = w->active ? LV_MAX(0, (int)((peak * (35 + (int)((seed >> 16) % 66)) + 50) / 100)) : 0;
        for (int k = -level; k <= level; ++k) {
            lv_coord_t y = mid + k * step - cell / 2;
            lv_color_t c = k == 0 ? sg_color(w->tone) : sg_shade(sg_color(w->tone), LV_ABS(k) * -30);
            sg_rect(ctx, x0 + i * step, y, x0 + i * step + cell - 1, y + cell - 1, c);
        }
    }
}

static void wave_step(lv_timer_t *timer)
{
    wave_t *w = lv_obj_get_user_data(timer->user_data);
    w->tick++;
    lv_obj_invalidate(timer->user_data);
}

static void wave_delete(lv_event_t *e)
{
    lv_timer_del(((wave_t *)lv_obj_get_user_data(lv_event_get_target(e)))->timer);
}

lv_obj_t *sg_waveform(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, sg_tone_t tone)
{
    lv_obj_t *wave = sg_plain(parent);
    lv_obj_set_size(wave, w, h);
    lv_obj_add_event_cb(wave, wave_delete, LV_EVENT_DELETE, NULL);
    wave_t *state = sg_state(wave, sizeof(wave_t));
    state->active = true;
    state->tone = tone;
    state->timer = lv_timer_create(wave_step, 120, wave);
    lv_obj_add_event_cb(wave, wave_draw, LV_EVENT_DRAW_MAIN, NULL);
    return wave;
}

void sg_waveform_set_active(lv_obj_t *waveform, bool active)
{
    ((wave_t *)lv_obj_get_user_data(waveform))->active = active;
    lv_obj_invalidate(waveform);
}

lv_obj_t *sg_chat_bubble(lv_obj_t *parent, const char *text, bool from_user)
{
    lv_obj_t *row = sg_plain(parent);
    lv_obj_set_width(row, 280);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, from_user ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *bubble = boxed(row, from_user ? SG_BOX_BUTTON : SG_BOX_FRAME, from_user ? SG_TONE_BLUE : SG_TONE_TEXT, 12, 8);
    lv_obj_t *label = sg_label(bubble, text, SG_FONT_CAPTION, SG_TONE_TEXT);
    lv_point_t size;
    lv_txt_get_size(&size, text, sg_font(SG_FONT_CAPTION), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if (size.x > 196) {
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(label, 196);
    }
    return row;
}

lv_obj_t *sg_avatar_frame(lv_obj_t *parent, lv_coord_t size, sg_tone_t tone)
{
    lv_obj_t *frame = sg_plain(parent);
    lv_obj_set_size(frame, size, size);
    sg_box(frame, SG_BOX_FRAME, tone, true);
    lv_obj_set_style_pad_right(frame, SG_UNIT, 0);
    lv_obj_set_style_pad_bottom(frame, SG_UNIT, 0);
    sg_flex(frame, LV_FLEX_FLOW_COLUMN, 0);
    return frame;
}

/* ---------------------------------------------------------------- Launcher */

lv_obj_t *sg_app_grid(lv_obj_t *screen)
{
    lv_obj_t *grid = sg_plain(screen);
    lv_obj_set_width(grid, 270);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(grid, 12, 0);
    lv_obj_set_style_pad_row(grid, 8, 0);
    lv_obj_center(grid);
    return grid;
}

lv_obj_t *sg_app_icon(lv_obj_t *grid, sg_icon_t icon, sg_tone_t tone, const char *label)
{
    lv_obj_t *app = sg_column(grid);
    lv_obj_set_width(app, 80);
    lv_obj_set_style_pad_row(app, 4, 0);
    sg_tappable(app);
    lv_obj_t *tile = sg_plain(app);
    lv_obj_set_size(tile, 62, 62);
    sg_box(tile, SG_BOX_BUTTON, tone, true);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_style_pad_right(tile, SG_UNIT, 0);
    lv_obj_set_style_pad_bottom(tile, SG_UNIT, 0);
    lv_obj_center(sg_icon(tile, icon, SG_SIZE_L, sg_on_tone(tone)));
    if (label) sg_label(app, label, SG_FONT_CAPTION, SG_TONE_TEXT);
    return app;
}
