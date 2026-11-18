/* SPDX-License-Identifier: Apache-2.0 */
/** Sugarat Watch foundation (palette, fonts, labels, icons, actions) and layout containers. */
#include "sw_internal.h"
#include <string.h>

extern const lv_font_t sw_font_text_14, sw_font_text_16, sw_font_text_20, sw_font_text_28;
extern const lv_font_t sw_font_num_24, sw_font_num_40, sw_font_num_80;
extern const lv_font_t sw_font_icon_16, sw_font_icon_20, sw_font_icon_28, sw_font_icon_40;

static const uint32_t palette[SW_TONE_COUNT] = {
    [SW_TONE_BG] = 0x000000,     [SW_TONE_SURFACE] = 0x1C1C1E, [SW_TONE_RAISED] = 0x2E2E31,
    [SW_TONE_TEXT] = 0xFFFFFF,   [SW_TONE_MUTED] = 0x8E8E93,   [SW_TONE_ACCENT] = 0x30D158,
    [SW_TONE_RED] = 0xFF453A,    [SW_TONE_ORANGE] = 0xFF9F0A,  [SW_TONE_YELLOW] = 0xFFD60A,
    [SW_TONE_GREEN] = 0x30D158,  [SW_TONE_TEAL] = 0x64D2FF,    [SW_TONE_BLUE] = 0x0A84FF,
    [SW_TONE_PURPLE] = 0xBF5AF2, [SW_TONE_PINK] = 0xFF375F,
};

/* Shared label styles: one per text role and icon size, one per tone. Static, never freed. */
static struct {
    lv_style_t fonts[SW_FONT_COUNT + 4];
    lv_style_t tones[SW_TONE_COUNT];
    bool ready;
} shared;

void sw_init(void)
{
    if (shared.ready) return;
    for (int i = 0; i < SW_FONT_COUNT + 4; ++i) {
        lv_style_init(&shared.fonts[i]);
        lv_style_set_text_font(&shared.fonts[i], i < SW_FONT_COUNT ? sw_font((sw_font_t)i) : sw_icon_font((sw_size_t)(i - SW_FONT_COUNT)));
    }
    for (int i = 0; i < SW_TONE_COUNT; ++i) {
        lv_style_init(&shared.tones[i]);
        lv_style_set_text_color(&shared.tones[i], sw_color((sw_tone_t)i));
    }
    shared.ready = true;
}

/** Create a label that shares the font style at font_index and the tone style. */
static lv_obj_t *shared_label(lv_obj_t *parent, int font_index, sw_tone_t tone)
{
    sw_init();
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_remove_style_all(label);
    lv_obj_add_style(label, &shared.fonts[font_index], 0);
    lv_obj_add_style(label, &shared.tones[tone < SW_TONE_COUNT ? tone : SW_TONE_TEXT], 0);
    return label;
}

lv_color_t sw_color(sw_tone_t tone)
{
    return lv_color_hex(palette[tone < SW_TONE_COUNT ? tone : SW_TONE_TEXT]);
}

const lv_font_t *sw_font(sw_font_t font)
{
    switch (font) {
    case SW_FONT_CAPTION: return &sw_font_text_14;
    case SW_FONT_HEADLINE: return &sw_font_text_20;
    case SW_FONT_TITLE: return &sw_font_text_28;
    case SW_FONT_NUM_S: return &sw_font_num_24;
    case SW_FONT_NUM_M: return &sw_font_num_40;
    case SW_FONT_NUM_L: return &sw_font_num_80;
    case SW_FONT_BODY:
    default: return &sw_font_text_16;
    }
}

const lv_font_t *sw_icon_font(sw_size_t size)
{
    static const lv_font_t *const fonts[] = {&sw_font_icon_16, &sw_font_icon_20, &sw_font_icon_28, &sw_font_icon_40};
    return fonts[size <= SW_SIZE_XL ? size : SW_SIZE_M];
}

sw_tone_t sw_on_tone(sw_tone_t tone)
{
    return tone == SW_TONE_YELLOW || tone == SW_TONE_TEAL || tone == SW_TONE_TEXT ? SW_TONE_BG : SW_TONE_TEXT;
}

static void free_state(lv_event_t *e)
{
    lv_mem_free(lv_event_get_user_data(e));
}

void *sw_state(lv_obj_t *obj, size_t size)
{
    void *state = lv_mem_alloc(size);
    LV_ASSERT_MALLOC(state);
    memset(state, 0, size);
    lv_obj_set_user_data(obj, state);
    lv_obj_add_event_cb(obj, free_state, LV_EVENT_DELETE, state);
    return state;
}

lv_obj_t *sw_plain(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    return obj;
}

void sw_flex(lv_obj_t *obj, lv_flex_flow_t flow, lv_coord_t gap)
{
    lv_obj_set_flex_flow(obj, flow);
    lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(obj, gap, 0);
    lv_obj_set_style_pad_column(obj, gap, 0);
}

void sw_tappable(lv_obj_t *obj)
{
    static lv_style_t pressed, disabled;
    if (!pressed.prop_cnt) {
        lv_style_init(&pressed);
        lv_style_set_transform_width(&pressed, -3);
        lv_style_set_transform_height(&pressed, -3);
        lv_style_init(&disabled);
        lv_style_set_opa(&disabled, LV_OPA_40);
    }
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_style(obj, &pressed, LV_STATE_PRESSED);
    lv_obj_add_style(obj, &disabled, LV_STATE_DISABLED);
}

lv_obj_t *sw_screen(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen, SW_SCREEN, SW_SCREEN);
    lv_obj_set_style_bg_color(screen, sw_color(SW_TONE_BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    return screen;
}

lv_obj_t *sw_label(lv_obj_t *parent, const char *text, sw_font_t font, sw_tone_t tone)
{
    lv_obj_t *label = shared_label(parent, font < SW_FONT_COUNT ? font : SW_FONT_BODY, tone);
    lv_label_set_text(label, text ? text : "");
    return label;
}

lv_obj_t *sw_icon(lv_obj_t *parent, sw_icon_t icon, sw_size_t size, sw_tone_t tone)
{
    lv_obj_t *label = shared_label(parent, SW_FONT_COUNT + (size <= SW_SIZE_XL ? size : SW_SIZE_M), tone);
    lv_label_set_text_static(label, sw_icon_glyphs[icon < SW_ICON_COUNT ? icon : SW_ICON_NONE]);
    return label;
}

void sw_icon_set(lv_obj_t *icon_obj, sw_icon_t icon, sw_tone_t tone)
{
    lv_label_set_text_static(icon_obj, sw_icon_glyphs[icon < SW_ICON_COUNT ? icon : SW_ICON_NONE]);
    lv_obj_set_style_text_color(icon_obj, sw_color(tone), 0);
}

void sw_set_action(lv_obj_t *obj, int action)
{
    lv_obj_add_flag(obj, LV_OBJ_FLAG_USER_1);
    lv_obj_set_user_data(obj, (void *)(intptr_t)action);
}

int sw_event_action(lv_event_t *e)
{
    lv_obj_t *stop = lv_event_get_current_target(e);
    for (lv_obj_t *obj = lv_event_get_target(e); obj; obj = lv_obj_get_parent(obj)) {
        if (lv_obj_has_flag(obj, LV_OBJ_FLAG_USER_1)) return (int)(intptr_t)lv_obj_get_user_data(obj);
        if (obj == stop) break;
    }
    return SW_ACTION_NONE;
}

/* ---------------------------------------------------------------- Layout */

static void fade_page(lv_obj_t *page)
{
    lv_area_t area;
    lv_obj_update_layout(page);
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(page); ++i) {
        lv_obj_t *child = lv_obj_get_child(page, i);
        lv_obj_get_coords(child, &area);
        lv_coord_t center = (area.y1 + area.y2) / 2, distance = LV_ABS(center - SW_SCREEN / 2);
        lv_opa_t opa = distance < 110 ? LV_OPA_COVER : distance > 175 ? LV_OPA_20 : 255 - (distance - 110) * 3;
        lv_obj_set_style_opa(child, opa, 0);
    }
}

static void fade_edges(lv_event_t *e)
{
    fade_page(lv_event_get_target(e));
}

static void fade_cancel(lv_event_t *e)
{
    lv_timer_del(lv_event_get_user_data(e));
}

static void fade_once(lv_timer_t *timer)
{
    lv_obj_remove_event_cb_with_user_data(timer->user_data, fade_cancel, timer);
    fade_page(timer->user_data);
}

lv_obj_t *sw_page(lv_obj_t *screen, const char *title)
{
    lv_obj_t *page = lv_obj_create(screen);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, SW_SCREEN, SW_SCREEN);
    lv_obj_set_scroll_dir(page, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_top(page, title ? 34 : 64, 0);
    lv_obj_set_style_pad_bottom(page, 80, 0);
    sw_flex(page, LV_FLEX_FLOW_COLUMN, 8);
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (title) {
        lv_obj_t *label = sw_label(page, title, SW_FONT_HEADLINE, SW_TONE_ACCENT);
        lv_obj_set_style_pad_bottom(label, 6, 0);
    }
    lv_obj_add_event_cb(page, fade_edges, LV_EVENT_SCROLL, NULL);
    lv_timer_t *timer = lv_timer_create(fade_once, 0, page);
    lv_timer_set_repeat_count(timer, 1);
    lv_obj_add_event_cb(page, fade_cancel, LV_EVENT_DELETE, timer);
    return page;
}

lv_obj_t *sw_row(lv_obj_t *parent)
{
    lv_obj_t *row = sw_plain(parent);
    sw_flex(row, LV_FLEX_FLOW_ROW, 10);
    return row;
}

lv_obj_t *sw_column(lv_obj_t *parent)
{
    lv_obj_t *column = sw_plain(parent);
    sw_flex(column, LV_FLEX_FLOW_COLUMN, 6);
    return column;
}

lv_obj_t *sw_card(lv_obj_t *parent)
{
    lv_obj_t *card = sw_plain(parent);
    lv_obj_set_width(card, 280);
    lv_obj_set_style_bg_color(card, sw_color(SW_TONE_SURFACE), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 24, 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    sw_flex(card, LV_FLEX_FLOW_COLUMN, 8);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    return card;
}

lv_obj_t *sw_section(lv_obj_t *parent, const char *title)
{
    lv_obj_t *label = sw_label(parent, title, SW_FONT_CAPTION, SW_TONE_MUTED);
    lv_obj_set_width(label, 256);
    lv_obj_set_style_pad_top(label, 8, 0);
    return label;
}

lv_obj_t *sw_status_bar(lv_obj_t *screen, const char *time)
{
    lv_obj_t *bar = sw_row(screen);
    lv_obj_set_style_pad_column(bar, 8, 0);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 14);
    sw_icon(bar, SW_ICON_WIFI, SW_SIZE_S, SW_TONE_TEXT);
    sw_label(bar, time, SW_FONT_BODY, SW_TONE_TEXT);
    sw_icon(bar, SW_ICON_BATTERY, SW_SIZE_S, SW_TONE_GREEN);
    return bar;
}

void sw_status_bar_set_time(lv_obj_t *bar, const char *time)
{
    lv_label_set_text(lv_obj_get_child(bar, 1), time);
}

void sw_status_bar_set_battery(lv_obj_t *bar, uint8_t percent, bool charging)
{
    sw_tone_t tone = charging ? SW_TONE_GREEN : percent < 20 ? SW_TONE_RED : SW_TONE_TEXT;
    sw_icon_set(lv_obj_get_child(bar, 2), charging ? SW_ICON_BOLT : SW_ICON_BATTERY, tone);
}

void sw_status_bar_set_connected(lv_obj_t *bar, bool connected)
{
    lv_obj_set_style_text_color(lv_obj_get_child(bar, 0), sw_color(connected ? SW_TONE_TEXT : SW_TONE_RAISED), 0);
}

lv_obj_t *sw_page_dots(lv_obj_t *parent, uint8_t count, uint8_t active)
{
    lv_obj_t *dots = sw_row(parent);
    lv_obj_set_style_pad_column(dots, 6, 0);
    for (uint8_t i = 0; i < count; ++i) {
        lv_obj_t *dot = sw_plain(dots);
        lv_obj_set_size(dot, 7, 7);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    }
    lv_obj_align(dots, LV_ALIGN_BOTTOM_MID, 0, -16);
    sw_page_dots_set(dots, active);
    return dots;
}

void sw_page_dots_set(lv_obj_t *dots, uint8_t active)
{
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(dots); ++i)
        lv_obj_set_style_bg_color(lv_obj_get_child(dots, i), sw_color(i == active ? SW_TONE_TEXT : SW_TONE_RAISED), 0);
}

static void follow_scroll(lv_event_t *e)
{
    lv_obj_t *page = lv_event_get_target(e), *arc = lv_event_get_user_data(e);
    lv_coord_t y = lv_obj_get_scroll_y(page), max = y + lv_obj_get_scroll_bottom(page);
    int32_t span = 64, thumb = 18, start = 328 + (max > 0 ? (int32_t)LV_CLAMP(0, y, max) * (span - thumb) / max : 0);
    lv_arc_set_angles(arc, start % 360, (start + thumb) % 360);
}

lv_obj_t *sw_scroll_indicator(lv_obj_t *page)
{
    lv_obj_t *arc = lv_arc_create(lv_obj_get_parent(page));
    lv_obj_remove_style_all(arc);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(arc, SW_SCREEN - 12, SW_SCREEN - 12);
    lv_obj_center(arc);
    lv_arc_set_bg_angles(arc, 328, 32);
    lv_obj_set_style_arc_width(arc, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, sw_color(SW_TONE_RAISED), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, sw_color(SW_TONE_TEXT), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_add_event_cb(page, follow_scroll, LV_EVENT_SCROLL, arc);
    lv_obj_add_event_cb(page, follow_scroll, LV_EVENT_SIZE_CHANGED, arc);
    lv_arc_set_angles(arc, 328, 346);
    return arc;
}
