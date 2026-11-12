/* SPDX-License-Identifier: Apache-2.0 */
/** Sugarat Watch feedback, data, time, media, companion and launcher components. */
#include "sw_internal.h"
#include <math.h>

static void fill(lv_obj_t *obj, sw_tone_t tone, lv_opa_t opa)
{
    lv_obj_set_style_bg_color(obj, sw_color(tone), 0);
    lv_obj_set_style_bg_opa(obj, opa, 0);
}

static lv_obj_t *circle(lv_obj_t *parent, lv_coord_t size, sw_tone_t tone, lv_opa_t opa)
{
    lv_obj_t *obj = sw_plain(parent);
    lv_obj_set_size(obj, size, size);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    fill(obj, tone, opa);
    return obj;
}

static lv_obj_t *wrapped(lv_obj_t *parent, const char *text, sw_font_t font, sw_tone_t tone, lv_coord_t width)
{
    lv_obj_t *label = sw_label(parent, text, font, tone);
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

/* ---------------------------------------------------------------- Feedback */

lv_obj_t *sw_badge(lv_obj_t *parent, const char *text, sw_tone_t tone)
{
    lv_obj_t *badge = sw_plain(parent);
    lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
    fill(badge, tone, LV_OPA_COVER);
    if (!text) {
        lv_obj_set_size(badge, 12, 12);
        return badge;
    }
    lv_obj_set_style_pad_hor(badge, 8, 0);
    lv_obj_set_style_pad_ver(badge, 2, 0);
    lv_obj_set_style_min_width(badge, 24, 0);
    lv_obj_t *label = sw_label(badge, text, SW_FONT_CAPTION, sw_on_tone(tone));
    lv_obj_center(label);
    return badge;
}

static void toast_in(void *obj, int32_t v)
{
    lv_obj_set_style_translate_y(obj, v, 0);
    lv_obj_set_style_opa(obj, (lv_opa_t)LV_MIN(255, 255 + v * 8), 0);
}

lv_obj_t *sw_toast(lv_obj_t *parent, sw_icon_t icon, sw_tone_t tone, const char *text, uint32_t duration_ms)
{
    lv_obj_t *toast = sw_row(parent ? parent : lv_layer_top());
    lv_obj_set_style_radius(toast, LV_RADIUS_CIRCLE, 0);
    fill(toast, SW_TONE_RAISED, LV_OPA_COVER);
    lv_obj_set_style_pad_hor(toast, 18, 0);
    lv_obj_set_style_pad_ver(toast, 10, 0);
    lv_obj_set_style_max_width(toast, 260, 0);
    if (icon != SW_ICON_NONE) sw_icon(toast, icon, SW_SIZE_M, tone);
    sw_label(toast, text, SW_FONT_BODY, SW_TONE_TEXT);
    lv_obj_align(toast, LV_ALIGN_TOP_MID, 0, 40);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, toast);
    lv_anim_set_exec_cb(&a, toast_in);
    lv_anim_set_values(&a, -30, 0);
    lv_anim_set_time(&a, 220);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
    if (duration_ms) lv_obj_del_delayed(toast, duration_ms);
    return toast;
}

lv_obj_t *sw_notification(lv_obj_t *parent, sw_icon_t icon, sw_tone_t tone, const char *app, const char *title, const char *body)
{
    lv_obj_t *card = sw_card(parent);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_t *head = sw_row(card);
    lv_obj_set_style_pad_column(head, 8, 0);
    lv_obj_t *badge = circle(head, 28, tone, LV_OPA_COVER);
    lv_obj_center(sw_icon(badge, icon, SW_SIZE_S, sw_on_tone(tone)));
    sw_label(head, app, SW_FONT_CAPTION, SW_TONE_MUTED);
    lv_obj_t *label = sw_label(card, title, SW_FONT_HEADLINE, SW_TONE_TEXT);
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    if (body) {
        label = sw_label(card, body, SW_FONT_BODY, SW_TONE_MUTED);
        lv_obj_set_width(label, LV_PCT(100));
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    }
    return card;
}

static void alert_close(lv_event_t *e)
{
    if (sw_event_action(e) != SW_ACTION_NONE) lv_obj_del_async(lv_event_get_current_target(e));
}

lv_obj_t *sw_alert(lv_obj_t *parent, sw_icon_t icon, sw_tone_t tone, const char *title, const char *message,
                   const char *confirm, const char *cancel)
{
    lv_obj_t *modal = sw_plain(parent ? parent : lv_layer_top());
    lv_obj_add_flag(modal, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(modal, SW_SCREEN, SW_SCREEN);
    lv_obj_center(modal);
    fill(modal, SW_TONE_BG, LV_OPA_COVER);
    sw_flex(modal, LV_FLEX_FLOW_COLUMN, 10);
    if (icon != SW_ICON_NONE) {
        lv_obj_t *icon_obj = sw_icon(modal, icon, SW_SIZE_XL, tone);
        lv_obj_set_style_pad_bottom(icon_obj, 4, 0);
    }
    wrapped(modal, title, SW_FONT_HEADLINE, SW_TONE_TEXT, 250);
    if (message) wrapped(modal, message, SW_FONT_BODY, SW_TONE_MUTED, 250);
    lv_obj_t *gap = sw_plain(modal);
    lv_obj_set_height(gap, 4);
    lv_obj_t *button = sw_button(modal, SW_ICON_NONE, confirm, SW_BUTTON_FILLED, tone);
    lv_obj_set_width(button, 220);
    sw_set_action(button, SW_ACTION_CONFIRM);
    if (cancel) {
        button = sw_button(modal, SW_ICON_NONE, cancel, SW_BUTTON_PLAIN, SW_TONE_TEXT);
        lv_obj_set_width(button, 220);
        sw_set_action(button, SW_ACTION_CANCEL);
    }
    lv_obj_add_event_cb(modal, alert_close, LV_EVENT_CLICKED, NULL);
    return modal;
}

lv_obj_t *sw_empty_state(lv_obj_t *parent, sw_icon_t icon, const char *title, const char *hint)
{
    lv_obj_t *block = sw_column(parent);
    lv_obj_set_style_pad_row(block, 10, 0);
    lv_obj_t *disc = circle(block, 96, SW_TONE_SURFACE, LV_OPA_COVER);
    lv_obj_center(sw_icon(disc, icon, SW_SIZE_XL, SW_TONE_MUTED));
    wrapped(block, title, SW_FONT_HEADLINE, SW_TONE_TEXT, 240);
    if (hint) wrapped(block, hint, SW_FONT_BODY, SW_TONE_MUTED, 240);
    return block;
}

lv_obj_t *sw_loading(lv_obj_t *parent, const char *text)
{
    lv_obj_t *block = sw_column(parent);
    lv_obj_set_style_pad_row(block, 14, 0);
    sw_spinner(block, 56, SW_TONE_ACCENT);
    if (text) sw_label(block, text, SW_FONT_BODY, SW_TONE_MUTED);
    return block;
}

/* ---------------------------------------------------------------- Data */

lv_obj_t *sw_stat(lv_obj_t *parent, const char *value, const char *unit, const char *label, sw_tone_t tone)
{
    lv_obj_t *stat = sw_plain(parent);
    lv_obj_set_flex_flow(stat, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(stat, 0, 0);
    sw_label(stat, label, SW_FONT_CAPTION, tone);
    lv_obj_t *row = sw_plain(stat);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(row, 4, 0);
    sw_label(row, value, SW_FONT_NUM_M, SW_TONE_TEXT);
    if (unit) {
        lv_obj_t *u = sw_label(row, unit, SW_FONT_HEADLINE, tone);
        lv_obj_set_style_pad_bottom(u, 6, 0);
    }
    return stat;
}

void sw_stat_set(lv_obj_t *stat, const char *value)
{
    lv_label_set_text(lv_obj_get_child(lv_obj_get_child(stat, 1), 0), value);
}

static lv_obj_t *arc(lv_obj_t *parent, lv_coord_t size, lv_coord_t width, lv_color_t color, lv_opa_t track_opa)
{
    lv_obj_t *a = lv_arc_create(parent);
    lv_obj_remove_style_all(a);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(a, size, size);
    lv_obj_set_style_arc_width(a, width, LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(a, color, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(a, track_opa, LV_PART_MAIN);
    lv_obj_set_style_arc_color(a, color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
    return a;
}

lv_obj_t *sw_gauge(lv_obj_t *parent, lv_coord_t size, int32_t min, int32_t max, int32_t value, const char *label, sw_tone_t tone)
{
    lv_obj_t *gauge = arc(parent, size, LV_MAX(6, size / 12), sw_color(tone), LV_OPA_30);
    lv_arc_set_rotation(gauge, 135);
    lv_arc_set_bg_angles(gauge, 0, 270);
    lv_arc_set_range(gauge, (int16_t)min, (int16_t)max);
    lv_obj_t *number = sw_label(gauge, "", size >= 120 ? SW_FONT_NUM_M : SW_FONT_NUM_S, SW_TONE_TEXT);
    lv_obj_align(number, LV_ALIGN_CENTER, 0, -4);
    lv_obj_t *caption = sw_label(gauge, label, SW_FONT_CAPTION, tone);
    lv_obj_align(caption, LV_ALIGN_BOTTOM_MID, 0, size >= 120 ? -14 : 0);
    sw_gauge_set(gauge, value);
    return gauge;
}

void sw_gauge_set(lv_obj_t *gauge, int32_t value)
{
    lv_arc_set_value(gauge, (int16_t)value);
    lv_label_set_text_fmt(lv_obj_get_child(gauge, 0), "%" LV_PRId32, value);
}

lv_obj_t *sw_tile(lv_obj_t *parent, sw_icon_t icon, sw_tone_t tone, const char *value, const char *label)
{
    lv_obj_t *tile = sw_plain(parent);
    lv_obj_set_size(tile, 132, 108);
    lv_obj_set_style_radius(tile, 26, 0);
    fill(tile, SW_TONE_SURFACE, LV_OPA_COVER);
    lv_obj_set_style_pad_all(tile, 14, 0);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tile, 2, 0);
    sw_icon(tile, icon, SW_SIZE_M, tone);
    sw_label(tile, value, SW_FONT_NUM_S, SW_TONE_TEXT);
    lv_obj_t *caption = sw_label(tile, label, SW_FONT_CAPTION, SW_TONE_MUTED);
    lv_label_set_long_mode(caption, LV_LABEL_LONG_DOT);
    lv_obj_set_width(caption, LV_PCT(100));
    return tile;
}

void sw_tile_set(lv_obj_t *tile, const char *value)
{
    lv_label_set_text(lv_obj_get_child(tile, 1), value);
}

static lv_obj_t *chart(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, lv_chart_type_t type, sw_tone_t tone)
{
    lv_obj_t *c = lv_chart_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(c, w, h);
    lv_chart_set_type(c, type);
    lv_chart_set_div_line_count(c, 0, 0);
    lv_chart_add_series(c, sw_color(tone), LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_set_style_radius(c, 4, LV_PART_ITEMS);
    lv_obj_set_style_line_width(c, 4, LV_PART_ITEMS);
    lv_obj_set_style_line_rounded(c, true, LV_PART_ITEMS);
    lv_obj_set_style_size(c, 0, LV_PART_INDICATOR);
    lv_obj_set_style_pad_column(c, 6, 0);
    return c;
}

static void chart_fill(lv_obj_t *c, const int16_t *values, uint8_t count, bool from_zero)
{
    lv_chart_series_t *series = lv_chart_get_series_next(c, NULL);
    int16_t lo = values[0], hi = values[0];
    for (uint8_t i = 0; i < count; ++i) {
        lo = LV_MIN(lo, values[i]);
        hi = LV_MAX(hi, values[i]);
    }
    if (from_zero) lo = LV_MIN(0, lo);
    lv_coord_t margin = from_zero ? 0 : (hi - lo) / 6 + 1;
    lv_chart_set_range(c, LV_CHART_AXIS_PRIMARY_Y, lo - margin, hi + margin);
    lv_chart_set_point_count(c, count);
    for (uint8_t i = 0; i < count; ++i) series->y_points[i] = values[i];
    lv_chart_refresh(c);
}

lv_obj_t *sw_bar_chart(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, const int16_t *values, uint8_t count, sw_tone_t tone)
{
    lv_obj_t *c = chart(parent, w, h, LV_CHART_TYPE_BAR, tone);
    chart_fill(c, values, count, true);
    return c;
}

void sw_bar_chart_set(lv_obj_t *c, const int16_t *values, uint8_t count)
{
    chart_fill(c, values, count, true);
}

lv_obj_t *sw_sparkline(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, const int16_t *values, uint8_t count, sw_tone_t tone)
{
    lv_obj_t *c = chart(parent, w, h, LV_CHART_TYPE_LINE, tone);
    chart_fill(c, values, count, false);
    return c;
}

void sw_sparkline_set(lv_obj_t *c, const int16_t *values, uint8_t count)
{
    chart_fill(c, values, count, false);
}

/* ---------------------------------------------------------------- Time */

lv_obj_t *sw_clock(lv_obj_t *parent, const char *time, const char *date)
{
    lv_obj_t *clock = sw_column(parent);
    lv_obj_set_style_pad_row(clock, 0, 0);
    sw_label(clock, date, SW_FONT_HEADLINE, SW_TONE_ORANGE);
    sw_label(clock, time, SW_FONT_NUM_L, SW_TONE_TEXT);
    return clock;
}

void sw_clock_set(lv_obj_t *clock, const char *time, const char *date)
{
    if (date) lv_label_set_text(lv_obj_get_child(clock, 0), date);
    if (time) lv_label_set_text(lv_obj_get_child(clock, 1), time);
}

typedef struct {
    lv_meter_indicator_t *hour, *minute, *second;
    lv_meter_scale_t *scale;
} analog_t;

lv_obj_t *sw_analog_clock(lv_obj_t *parent, lv_coord_t size, uint8_t hour, uint8_t minute, uint8_t second)
{
    lv_obj_t *meter = lv_meter_create(parent);
    lv_obj_remove_style_all(meter);
    lv_obj_clear_flag(meter, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(meter, size, size);
    lv_obj_set_style_text_opa(meter, LV_OPA_TRANSP, LV_PART_TICKS);
    lv_obj_set_style_pad_all(meter, 4, 0);
    analog_t *a = sw_state(meter, sizeof(*a));
    a->scale = lv_meter_add_scale(meter);
    lv_meter_set_scale_range(meter, a->scale, 0, 720, 360, 270);
    lv_meter_set_scale_ticks(meter, a->scale, 61, 2, size / 30, sw_color(SW_TONE_MUTED));
    lv_meter_set_scale_major_ticks(meter, a->scale, 5, 5, size / 12, sw_color(SW_TONE_TEXT), 0);
    a->hour = lv_meter_add_needle_line(meter, a->scale, 8, sw_color(SW_TONE_TEXT), -size / 4);
    a->minute = lv_meter_add_needle_line(meter, a->scale, 6, sw_color(SW_TONE_TEXT), -size / 14);
    a->second = lv_meter_add_needle_line(meter, a->scale, 2, sw_color(SW_TONE_ORANGE), -size / 22);
    lv_obj_set_style_size(meter, 14, LV_PART_INDICATOR);
    lv_obj_set_style_radius(meter, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(meter, sw_color(SW_TONE_ORANGE), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(meter, LV_OPA_COVER, LV_PART_INDICATOR);
    sw_analog_clock_set(meter, hour, minute, second);
    return meter;
}

void sw_analog_clock_set(lv_obj_t *clock, uint8_t hour, uint8_t minute, uint8_t second)
{
    analog_t *a = lv_obj_get_user_data(clock);
    lv_meter_set_indicator_value(clock, a->hour, (hour % 12) * 60 + minute);
    lv_meter_set_indicator_value(clock, a->minute, minute * 12 + second / 5);
    lv_meter_set_indicator_value(clock, a->second, second * 12);
}

lv_obj_t *sw_timer(lv_obj_t *parent, lv_coord_t size, uint32_t total_s, uint32_t remaining_s, sw_tone_t tone)
{
    lv_obj_t *ring = arc(parent, size, LV_MAX(8, size / 14), sw_color(tone), LV_OPA_20);
    lv_arc_set_rotation(ring, 270);
    lv_arc_set_bg_angles(ring, 0, 360);
    lv_arc_set_range(ring, 0, (int16_t)LV_MIN(total_s ? total_s : 1, INT16_MAX));
    lv_obj_t *time = sw_label(ring, "", size >= 200 ? SW_FONT_NUM_M : SW_FONT_NUM_S, SW_TONE_TEXT);
    lv_obj_center(time);
    sw_timer_set(ring, remaining_s);
    return ring;
}

void sw_timer_set(lv_obj_t *timer, uint32_t remaining_s)
{
    lv_arc_set_value(timer, (int16_t)LV_MIN(remaining_s, (uint32_t)lv_arc_get_max_value(timer)));
    lv_label_set_text_fmt(lv_obj_get_child(timer, 0), "%02u:%02u", (unsigned)(remaining_s / 60), (unsigned)(remaining_s % 60));
}

/* ---------------------------------------------------------------- Media */

lv_obj_t *sw_now_playing(lv_obj_t *parent, const char *title, const char *artist, bool playing)
{
    lv_obj_t *player = sw_column(parent);
    lv_obj_set_style_pad_row(player, 4, 0);
    lv_obj_t *label = sw_label(player, title, SW_FONT_HEADLINE, SW_TONE_TEXT);
    lv_obj_set_width(label, 240);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    label = sw_label(player, artist, SW_FONT_BODY, SW_TONE_MUTED);
    lv_obj_set_style_pad_bottom(label, 14, 0);
    lv_obj_t *controls = sw_row(player);
    lv_obj_set_style_pad_column(controls, 18, 0);
    lv_obj_t *button = sw_button(controls, SW_ICON_PREV, NULL, SW_BUTTON_TINTED, SW_TONE_TEXT);
    lv_obj_set_size(button, 60, 60);
    sw_set_action(button, SW_ACTION_PREV);
    button = sw_button(controls, SW_ICON_PLAY, NULL, SW_BUTTON_PLAIN, SW_TONE_TEXT);
    lv_obj_set_size(button, 84, 84);
    lv_obj_set_style_text_font(lv_obj_get_child(button, 0), sw_icon_font(SW_SIZE_XL), 0);
    sw_set_action(button, SW_ACTION_PLAY);
    button = sw_button(controls, SW_ICON_NEXT, NULL, SW_BUTTON_TINTED, SW_TONE_TEXT);
    lv_obj_set_size(button, 60, 60);
    sw_set_action(button, SW_ACTION_NEXT);
    sw_now_playing_set(player, NULL, NULL, playing);
    return player;
}

void sw_now_playing_set(lv_obj_t *player, const char *title, const char *artist, bool playing)
{
    if (title) lv_label_set_text(lv_obj_get_child(player, 0), title);
    if (artist) lv_label_set_text(lv_obj_get_child(player, 1), artist);
    lv_obj_t *play = lv_obj_get_child(lv_obj_get_child(player, 2), 1);
    sw_icon_set(lv_obj_get_child(play, 0), playing ? SW_ICON_PAUSE : SW_ICON_PLAY, SW_TONE_TEXT);
}

lv_obj_t *sw_volume(lv_obj_t *screen, uint8_t percent)
{
    lv_obj_t *volume = arc(screen, SW_SCREEN - 14, 8, sw_color(SW_TONE_GREEN), LV_OPA_20);
    lv_obj_center(volume);
    lv_arc_set_mode(volume, LV_ARC_MODE_REVERSE);
    lv_arc_set_bg_angles(volume, 320, 40);
    lv_arc_set_range(volume, 0, 100);
    lv_obj_t *icon = sw_icon(volume, SW_ICON_VOLUME, SW_SIZE_S, SW_TONE_GREEN);
    lv_obj_align(icon, LV_ALIGN_RIGHT_MID, -20, -66);
    sw_volume_set(volume, percent);
    return volume;
}

void sw_volume_set(lv_obj_t *volume, uint8_t percent)
{
    lv_arc_set_value(volume, LV_MIN(percent, 100));
}

/* ---------------------------------------------------------------- Companion */

lv_obj_t *sw_voice_button(lv_obj_t *parent, sw_voice_t state)
{
    lv_obj_t *button = sw_button(parent, SW_ICON_MIC, "", SW_BUTTON_FILLED, SW_TONE_RAISED);
    lv_obj_set_width(button, 196);
    sw_set_action(button, SW_ACTION_VOICE);
    sw_voice_button_set(button, state);
    return button;
}

void sw_voice_button_set(lv_obj_t *button, sw_voice_t state)
{
    static const struct { sw_icon_t icon; sw_tone_t tone; const char *text; } looks[] = {
        [SW_VOICE_IDLE] = {SW_ICON_MIC, SW_TONE_RAISED, "按住说话"},
        [SW_VOICE_LISTENING] = {SW_ICON_MIC, SW_TONE_BLUE, "正在聆听"},
        [SW_VOICE_THINKING] = {SW_ICON_SYNC, SW_TONE_PURPLE, "思考中…"},
        [SW_VOICE_SPEAKING] = {SW_ICON_VOLUME, SW_TONE_GREEN, "正在回答"},
        [SW_VOICE_OFFLINE] = {SW_ICON_WIFI, SW_TONE_SURFACE, "未连接"},
    };
    sw_voice_t s = state <= SW_VOICE_OFFLINE ? state : SW_VOICE_IDLE;
    sw_tone_t ink = s == SW_VOICE_OFFLINE ? SW_TONE_MUTED : SW_TONE_TEXT;
    fill(button, looks[s].tone, LV_OPA_COVER);
    sw_icon_set(lv_obj_get_child(button, 0), looks[s].icon, s == SW_VOICE_OFFLINE ? SW_TONE_RED : ink);
    lv_obj_t *label = lv_obj_get_child(button, 1);
    lv_label_set_text_static(label, looks[s].text);
    lv_obj_set_style_text_color(label, sw_color(ink), 0);
}

typedef struct {
    lv_timer_t *timer;
    uint32_t tick;
    bool active;
} wave_t;

static void wave_step(lv_timer_t *timer)
{
    lv_obj_t *wave = timer->user_data;
    wave_t *w = lv_obj_get_user_data(wave);
    w->tick++;
    lv_coord_t h = lv_obj_get_content_height(wave);
    uint32_t n = lv_obj_get_child_cnt(wave);
    for (uint32_t i = 0; i < n; ++i) {
        float envelope = 1.0f - fabsf((float)i - (n - 1) / 2.0f) / (n / 2.0f + 1);
        float wobble = 0.55f + 0.45f * sinf(w->tick * 0.7f + i * 1.9f) * cosf(w->tick * 0.31f + i * 0.8f);
        lv_coord_t bar = w->active ? (lv_coord_t)LV_MAX(8, h * envelope * wobble) : 8;
        lv_obj_set_height(lv_obj_get_child(wave, i), bar);
    }
}

static void wave_delete(lv_event_t *e)
{
    lv_timer_del(((wave_t *)lv_obj_get_user_data(lv_event_get_target(e)))->timer);
}

lv_obj_t *sw_waveform(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, sw_tone_t tone)
{
    lv_obj_t *wave = sw_plain(parent);
    lv_obj_set_size(wave, w, h);
    sw_flex(wave, LV_FLEX_FLOW_ROW, 6);
    for (lv_coord_t x = 0; x + 8 <= w; x += 14) {
        lv_obj_t *bar = sw_plain(wave);
        lv_obj_set_size(bar, 8, 8);
        lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, 0);
        fill(bar, tone, LV_OPA_COVER);
    }
    lv_obj_add_event_cb(wave, wave_delete, LV_EVENT_DELETE, NULL);
    wave_t *state = sw_state(wave, sizeof(wave_t));
    state->active = true;
    state->timer = lv_timer_create(wave_step, 80, wave);
    wave_step(state->timer);
    return wave;
}

void sw_waveform_set_active(lv_obj_t *waveform, bool active)
{
    wave_t *w = lv_obj_get_user_data(waveform);
    w->active = active;
    wave_step(w->timer);
}

lv_obj_t *sw_chat_bubble(lv_obj_t *parent, const char *text, bool from_user)
{
    lv_obj_t *row = sw_plain(parent);
    lv_obj_set_width(row, 280);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, from_user ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *bubble = sw_label(row, text, SW_FONT_BODY, SW_TONE_TEXT);
    lv_point_t size;
    lv_txt_get_size(&size, text, sw_font(SW_FONT_BODY), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if (size.x > 220 - 28) {
        lv_label_set_long_mode(bubble, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(bubble, 220);
    }
    fill(bubble, from_user ? SW_TONE_BLUE : SW_TONE_RAISED, LV_OPA_COVER);
    lv_obj_set_style_radius(bubble, 20, 0);
    lv_obj_set_style_pad_hor(bubble, 14, 0);
    lv_obj_set_style_pad_ver(bubble, 9, 0);
    return row;
}

lv_obj_t *sw_avatar_frame(lv_obj_t *parent, lv_coord_t size, sw_tone_t tone)
{
    lv_obj_t *frame = circle(parent, size, tone, LV_OPA_20);
    lv_obj_set_style_border_width(frame, 3, 0);
    lv_obj_set_style_border_color(frame, sw_color(tone), 0);
    lv_obj_set_style_clip_corner(frame, true, 0);
    lv_obj_set_layout(frame, LV_LAYOUT_FLEX);
    sw_flex(frame, LV_FLEX_FLOW_COLUMN, 0);
    return frame;
}

/* ---------------------------------------------------------------- Launcher */

static void grid_place(lv_obj_t *icon)
{
    uint32_t index = lv_obj_get_index(icon);
    static const int8_t q[] = {0, 1, 0, -1, -1, 0, 1, 2, 1, 0, -1, -2, -2, -2, -1, 0, 1, 2, 2};
    static const int8_t r[] = {0, -1, -1, 0, 1, 1, 0, -2, -2, -2, -1, 0, 1, 2, 2, 2, 1, 0, -1};
    if (index >= sizeof(q)) {
        lv_obj_add_flag(icon, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    const float spacing = 82.0f;
    float x = spacing * (q[index] + r[index] / 2.0f), y = spacing * 0.866f * r[index];
    float distance = sqrtf(x * x + y * y);
    lv_coord_t size = (lv_coord_t)(distance < 1 ? 78 : distance < 100 ? 70 : distance < 150 ? 56 : 44);
    lv_obj_set_size(icon, size, size);
    lv_obj_align(icon, LV_ALIGN_CENTER, (lv_coord_t)x, (lv_coord_t)y);
    if (size < 70) lv_obj_set_style_text_font(lv_obj_get_child(icon, 0), sw_icon_font(size < 56 ? SW_SIZE_M : SW_SIZE_L), 0);
}

lv_obj_t *sw_app_grid(lv_obj_t *screen)
{
    lv_obj_t *grid = sw_plain(screen);
    lv_obj_set_size(grid, SW_SCREEN, SW_SCREEN);
    lv_obj_center(grid);
    return grid;
}

lv_obj_t *sw_app_icon(lv_obj_t *grid, sw_icon_t icon, sw_tone_t tone, const char *label)
{
    lv_obj_t *app = circle(grid, 70, tone, LV_OPA_COVER);
    sw_tappable(app);
    lv_obj_t *glyph = sw_icon(app, icon, SW_SIZE_XL, sw_on_tone(tone));
    lv_obj_center(glyph);
    grid_place(app);
    (void)label;
    return app;
}

/* ---------------------------------------------------------------- Watch only */

lv_obj_t *sw_activity_rings(lv_obj_t *parent, lv_coord_t size, uint16_t move, uint16_t exercise, uint16_t stand)
{
    static const uint32_t colors[] = {0xFA114F, 0xA6FF00, 0x00F0FF};
    lv_obj_t *rings = sw_plain(parent);
    lv_obj_set_size(rings, size, size);
    lv_coord_t width = size / 9, gap = 3;
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *ring = arc(rings, size - i * 2 * (width + gap), width, lv_color_hex(colors[i]), LV_OPA_20);
        lv_obj_center(ring);
        lv_arc_set_rotation(ring, 270);
        lv_arc_set_bg_angles(ring, 0, 360);
        lv_arc_set_range(ring, 0, 100);
    }
    sw_activity_rings_set(rings, move, exercise, stand);
    return rings;
}

void sw_activity_rings_set(lv_obj_t *rings, uint16_t move, uint16_t exercise, uint16_t stand)
{
    const uint16_t values[] = {move, exercise, stand};
    for (int i = 0; i < 3; ++i) lv_arc_set_value(lv_obj_get_child(rings, i), LV_MIN(values[i], 100));
}
