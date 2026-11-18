/* SPDX-License-Identifier: Apache-2.0 */
/** Sugarat Game foundation: palette, pixel fonts, pixel drawing, icons, actions and layout containers. */
#include "sg_internal.h"
#include <string.h>

extern const lv_font_t sg_font_s, sg_font_m, sg_font_l, sg_font_num_m, sg_font_num_l;
extern const lv_font_t sg_font_icon_16, sg_font_icon_24, sg_font_icon_32, sg_font_icon_40;

static const uint32_t palette[SG_TONE_COUNT] = {
    [SG_TONE_BG] = 0x1A1C2C,     [SG_TONE_SURFACE] = 0x333C57, [SG_TONE_RAISED] = 0x566C86,
    [SG_TONE_TEXT] = 0xF4F4F4,   [SG_TONE_MUTED] = 0x94B0C2,   [SG_TONE_ACCENT] = 0xFFCD75,
    [SG_TONE_RED] = 0xE8475B,    [SG_TONE_ORANGE] = 0xEF7D57,  [SG_TONE_YELLOW] = 0xFFCD75,
    [SG_TONE_GREEN] = 0x38B764,  [SG_TONE_TEAL] = 0x73EFF7,    [SG_TONE_BLUE] = 0x41A6F6,
    [SG_TONE_PURPLE] = 0x9D65E8, [SG_TONE_PINK] = 0xFF77A8,
};

/* Shared label styles: one per text role and icon size, one per tone. Static, never freed. */
static struct {
    lv_style_t fonts[SG_FONT_COUNT + 4];
    lv_style_t tones[SG_TONE_COUNT];
    bool ready;
} shared;

void sg_init(void)
{
    if (shared.ready) return;
    for (int i = 0; i < SG_FONT_COUNT + 4; ++i) {
        lv_style_init(&shared.fonts[i]);
        lv_style_set_text_font(&shared.fonts[i], i < SG_FONT_COUNT ? sg_font((sg_font_t)i) : sg_icon_font((sg_size_t)(i - SG_FONT_COUNT)));
        lv_style_set_text_line_space(&shared.fonts[i], 4);
    }
    for (int i = 0; i < SG_TONE_COUNT; ++i) {
        lv_style_init(&shared.tones[i]);
        lv_style_set_text_color(&shared.tones[i], sg_color((sg_tone_t)i));
    }
    shared.ready = true;
}

/** Create a label that shares the font style at font_index and the tone style. */
static lv_obj_t *shared_label(lv_obj_t *parent, int font_index, sg_tone_t tone)
{
    sg_init();
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_remove_style_all(label);
    lv_obj_add_style(label, &shared.fonts[font_index], 0);
    lv_obj_add_style(label, &shared.tones[tone < SG_TONE_COUNT ? tone : SG_TONE_TEXT], 0);
    return label;
}

lv_color_t sg_color(sg_tone_t tone)
{
    return lv_color_hex(palette[tone < SG_TONE_COUNT ? tone : SG_TONE_TEXT]);
}

const lv_font_t *sg_font(sg_font_t font)
{
    switch (font) {
    case SG_FONT_CAPTION: return &sg_font_s;
    case SG_FONT_TITLE:
    case SG_FONT_NUM_S: return &sg_font_l;
    case SG_FONT_NUM_M: return &sg_font_num_m;
    case SG_FONT_NUM_L: return &sg_font_num_l;
    case SG_FONT_BODY:
    case SG_FONT_HEADLINE:
    default: return &sg_font_m;
    }
}

const lv_font_t *sg_icon_font(sg_size_t size)
{
    static const lv_font_t *const fonts[] = {&sg_font_icon_16, &sg_font_icon_24, &sg_font_icon_32, &sg_font_icon_40};
    return fonts[size <= SG_SIZE_XL ? size : SG_SIZE_M];
}

sg_tone_t sg_on_tone(sg_tone_t tone)
{
    switch (tone) {
    case SG_TONE_YELLOW:
    case SG_TONE_ACCENT:
    case SG_TONE_TEAL:
    case SG_TONE_TEXT:
    case SG_TONE_MUTED:
    case SG_TONE_PINK: return SG_TONE_BG;
    default: return SG_TONE_TEXT;
    }
}

static void free_state(lv_event_t *e)
{
    lv_mem_free(lv_event_get_user_data(e));
}

void *sg_state(lv_obj_t *obj, size_t size)
{
    void *state = lv_mem_alloc(size);
    LV_ASSERT_MALLOC(state);
    memset(state, 0, size);
    lv_obj_set_user_data(obj, state);
    lv_obj_add_event_cb(obj, free_state, LV_EVENT_DELETE, state);
    return state;
}

lv_obj_t *sg_plain(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    return obj;
}

void sg_flex(lv_obj_t *obj, lv_flex_flow_t flow, lv_coord_t gap)
{
    lv_obj_set_flex_flow(obj, flow);
    lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(obj, gap, 0);
    lv_obj_set_style_pad_column(obj, gap, 0);
}

void sg_tappable(lv_obj_t *obj)
{
    static lv_style_t disabled;
    if (!disabled.prop_cnt) {
        lv_style_init(&disabled);
        lv_style_set_opa(&disabled, LV_OPA_50);
    }
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_style(obj, &disabled, LV_STATE_DISABLED);
}

/* ---------------------------------------------------------------- Pixel drawing */

lv_color_t sg_shade(lv_color_t c, int amount)
{
    return amount >= 0 ? lv_color_mix(lv_color_white(), c, (lv_opa_t)amount)
                       : lv_color_mix(lv_color_hex(SG_INK), c, (lv_opa_t)-amount);
}

void sg_rect(lv_draw_ctx_t *ctx, lv_coord_t x1, lv_coord_t y1, lv_coord_t x2, lv_coord_t y2, lv_color_t color)
{
    if (x2 < x1 || y2 < y1) return;
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.bg_opa = LV_OPA_COVER;
    lv_area_t area = {x1, y1, x2, y2};
    lv_draw_rect(ctx, &dsc, &area);
}

static void chamfer(lv_draw_ctx_t *ctx, const lv_area_t *a, lv_color_t color)
{
    sg_rect(ctx, a->x1 + SG_UNIT, a->y1, a->x2 - SG_UNIT, a->y2, color);
    sg_rect(ctx, a->x1, a->y1 + SG_UNIT, a->x2, a->y2 - SG_UNIT, color);
}

void sg_draw_box(lv_draw_ctx_t *ctx, const lv_area_t *area, sg_box_style_t style, sg_tone_t tone)
{
    const lv_coord_t u = SG_UNIT;
    lv_area_t in = {area->x1 + u, area->y1 + u, area->x2 - u, area->y2 - u};
    chamfer(ctx, area, lv_color_hex(SG_INK));
    switch (style) {
    case SG_BOX_FRAME: {
        chamfer(ctx, &in, sg_color(tone));
        sg_rect(ctx, in.x1 + u, in.y1 + u, in.x2 - u, in.y2 - u, lv_color_hex(SG_DEEP));
        break;
    }
    case SG_BOX_INSET: {
        lv_color_t fill = sg_shade(sg_color(SG_TONE_BG), -90);
        sg_rect(ctx, in.x1, in.y1, in.x2, in.y2, fill);
        break;
    }
    case SG_BOX_BUTTON:
    case SG_BOX_PANEL:
    default: {
        lv_color_t fill = sg_color(style == SG_BOX_BUTTON ? tone : SG_TONE_SURFACE);
        sg_rect(ctx, in.x1, in.y1, in.x2, in.y2, fill);
        sg_rect(ctx, in.x1, in.y1, in.x2, in.y1 + u - 1, style == SG_BOX_PANEL ? sg_color(SG_TONE_RAISED) : sg_shade(fill, 110));
        sg_rect(ctx, in.x1, in.y2 - u + 1, in.x2, in.y2, sg_shade(fill, -110));
        break;
    }
    }
}

#define BOX_PACK(style, tone, shadow) ((void *)(uintptr_t)((style) | ((tone) << 8) | ((shadow) ? 0x10000 : 0)))

static void box_draw(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_current_target(e);
    uintptr_t box = (uintptr_t)lv_event_get_user_data(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    if (box & 0x10000) {
        a.x2 -= SG_UNIT;
        a.y2 -= SG_UNIT;
        if (!lv_obj_has_state(obj, LV_STATE_PRESSED)) {
            lv_area_t s = {a.x1 + SG_UNIT, a.y1 + SG_UNIT, a.x2 + SG_UNIT, a.y2 + SG_UNIT};
            chamfer(ctx, &s, lv_color_hex(SG_INK));
        }
    }
    sg_draw_box(ctx, &a, (sg_box_style_t)(box & 0xFF), (sg_tone_t)((box >> 8) & 0xFF));
}

static const lv_style_t *sunk_style(void)
{
    static lv_style_t sunk;
    if (!sunk.prop_cnt) {
        lv_style_init(&sunk);
        lv_style_set_translate_x(&sunk, SG_UNIT);
        lv_style_set_translate_y(&sunk, SG_UNIT);
    }
    return &sunk;
}

void sg_box(lv_obj_t *obj, sg_box_style_t style, sg_tone_t tone, bool shadow)
{
    lv_obj_add_event_cb(obj, box_draw, LV_EVENT_DRAW_MAIN_BEGIN, BOX_PACK(style, tone, shadow));
    if (shadow) lv_obj_add_style(obj, (lv_style_t *)sunk_style(), LV_STATE_PRESSED);
}

void sg_box_set_tone(lv_obj_t *obj, sg_tone_t tone)
{
    uintptr_t box = (uintptr_t)lv_obj_get_event_user_data(obj, box_draw);
    if (!box) return;
    lv_obj_remove_event_cb(obj, box_draw);
    lv_obj_add_event_cb(obj, box_draw, LV_EVENT_DRAW_MAIN_BEGIN, BOX_PACK(box & 0xFF, tone, box & 0x10000));
    lv_obj_invalidate(obj);
}

/* ---------------------------------------------------------------- Foundation */

lv_obj_t *sg_screen(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen, SG_SCREEN, SG_SCREEN);
    lv_obj_set_style_bg_color(screen, sg_color(SG_TONE_BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    return screen;
}

lv_obj_t *sg_label(lv_obj_t *parent, const char *text, sg_font_t font, sg_tone_t tone)
{
    lv_obj_t *label = shared_label(parent, font < SG_FONT_COUNT ? font : SG_FONT_BODY, tone);
    lv_label_set_text(label, text ? text : "");
    return label;
}

lv_obj_t *sg_icon(lv_obj_t *parent, sg_icon_t icon, sg_size_t size, sg_tone_t tone)
{
    lv_obj_t *label = shared_label(parent, SG_FONT_COUNT + (size <= SG_SIZE_XL ? size : SG_SIZE_M), tone);
    lv_label_set_text_static(label, sg_icon_glyphs[icon < SG_ICON_COUNT ? icon : SG_ICON_NONE]);
    return label;
}

void sg_icon_set(lv_obj_t *icon_obj, sg_icon_t icon, sg_tone_t tone)
{
    lv_label_set_text_static(icon_obj, sg_icon_glyphs[icon < SG_ICON_COUNT ? icon : SG_ICON_NONE]);
    lv_obj_set_style_text_color(icon_obj, sg_color(tone), 0);
}

void sg_set_action(lv_obj_t *obj, int action)
{
    lv_obj_add_flag(obj, LV_OBJ_FLAG_USER_1);
    lv_obj_set_user_data(obj, (void *)(intptr_t)action);
}

int sg_event_action(lv_event_t *e)
{
    lv_obj_t *stop = lv_event_get_current_target(e);
    for (lv_obj_t *obj = lv_event_get_target(e); obj; obj = lv_obj_get_parent(obj)) {
        if (lv_obj_has_flag(obj, LV_OBJ_FLAG_USER_1)) return (int)(intptr_t)lv_obj_get_user_data(obj);
        if (obj == stop) break;
    }
    return SG_ACTION_NONE;
}

/* ---------------------------------------------------------------- Block rings */

static void ring_draw(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    const sg_blocks_t *b = lv_obj_get_user_data(obj);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    lv_coord_t cx = (a.x1 + a.x2 + 1) / 2, cy = (a.y1 + a.y2 + 1) / 2;
    lv_coord_t r = lv_area_get_width(&a) / 2 - b->block / 2 - SG_UNIT;
    lv_color_t on = sg_color(b->tone), off = sg_color(SG_TONE_SURFACE);
    bool full = b->sweep >= 360;
    for (int i = 0; i < b->count; ++i) {
        int32_t angle = b->start + (full ? b->sweep * i / b->count : (b->count > 1 ? b->sweep * i / (b->count - 1) : 0));
        lv_coord_t x = cx + (lv_coord_t)((int32_t)r * lv_trigo_cos((int16_t)angle) >> LV_TRIGO_SHIFT);
        lv_coord_t y = cy + (lv_coord_t)((int32_t)r * lv_trigo_sin((int16_t)angle) >> LV_TRIGO_SHIFT);
        x -= ((x - a.x1) % SG_UNIT) + b->block / 2;
        y -= ((y - a.y1) % SG_UNIT) + b->block / 2;
        lv_color_t color = off;
        if (b->head >= 0) {
            int lag = (b->head - i + b->count) % b->count;
            if (lag < 4) color = lv_color_mix(on, off, (lv_opa_t)(255 - lag * 60));
        } else if (i < b->filled) {
            color = on;
        }
        sg_rect(ctx, x - SG_UNIT / 2, y - SG_UNIT / 2, x + b->block + SG_UNIT / 2 - 1, y + b->block + SG_UNIT / 2 - 1, lv_color_hex(SG_INK));
        sg_rect(ctx, x, y, x + b->block - 1, y + b->block - 1, color);
    }
}

lv_obj_t *sg_block_ring(lv_obj_t *parent, lv_coord_t size, const sg_blocks_t *blocks)
{
    lv_obj_t *obj = sg_plain(parent);
    lv_obj_set_size(obj, size, size);
    sg_blocks_t *b = sg_state(obj, sizeof(sg_blocks_t));
    *b = *blocks;
    lv_obj_add_event_cb(obj, ring_draw, LV_EVENT_DRAW_MAIN, NULL);
    return obj;
}

/* ---------------------------------------------------------------- Layout */

lv_obj_t *sg_page(lv_obj_t *screen, const char *title)
{
    lv_obj_t *page = lv_obj_create(screen);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, SG_SCREEN, SG_SCREEN);
    lv_obj_set_scroll_dir(page, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_top(page, title ? 28 : 60, 0);
    lv_obj_set_style_pad_bottom(page, 80, 0);
    sg_flex(page, LV_FLEX_FLOW_COLUMN, 8);
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (title) {
        lv_obj_t *ribbon = sg_plain(page);
        sg_box(ribbon, SG_BOX_BUTTON, SG_TONE_ACCENT, true);
        lv_obj_set_style_pad_hor(ribbon, 16, 0);
        lv_obj_set_style_pad_top(ribbon, 6, 0);
        lv_obj_set_style_pad_bottom(ribbon, 6 + SG_UNIT, 0);
        sg_label(ribbon, title, SG_FONT_BODY, SG_TONE_BG);
    }
    return page;
}

lv_obj_t *sg_row(lv_obj_t *parent)
{
    lv_obj_t *row = sg_plain(parent);
    sg_flex(row, LV_FLEX_FLOW_ROW, 10);
    return row;
}

lv_obj_t *sg_column(lv_obj_t *parent)
{
    lv_obj_t *column = sg_plain(parent);
    sg_flex(column, LV_FLEX_FLOW_COLUMN, 6);
    return column;
}

lv_obj_t *sg_card(lv_obj_t *parent)
{
    lv_obj_t *card = sg_plain(parent);
    lv_obj_set_width(card, 280);
    sg_box(card, SG_BOX_PANEL, SG_TONE_SURFACE, true);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_style_pad_right(card, 14 + SG_UNIT, 0);
    lv_obj_set_style_pad_bottom(card, 14 + SG_UNIT, 0);
    sg_flex(card, LV_FLEX_FLOW_COLUMN, 8);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    return card;
}

lv_obj_t *sg_section(lv_obj_t *parent, const char *title)
{
    lv_obj_t *row = sg_row(parent);
    lv_obj_set_width(row, 260);
    lv_obj_set_style_pad_top(row, 8, 0);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);
    sg_icon(row, SG_ICON_RIGHT, SG_SIZE_S, SG_TONE_ACCENT);
    sg_label(row, title, SG_FONT_CAPTION, SG_TONE_MUTED);
    return row;
}

lv_obj_t *sg_status_bar(lv_obj_t *screen, const char *time)
{
    lv_obj_t *bar = sg_row(screen);
    lv_obj_set_style_pad_column(bar, 10, 0);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 16);
    sg_icon(bar, SG_ICON_WIFI, SG_SIZE_S, SG_TONE_TEXT);
    sg_label(bar, time, SG_FONT_CAPTION, SG_TONE_TEXT);
    sg_icon(bar, SG_ICON_BATTERY, SG_SIZE_S, SG_TONE_GREEN);
    return bar;
}

void sg_status_bar_set_time(lv_obj_t *bar, const char *time)
{
    lv_label_set_text(lv_obj_get_child(bar, 1), time);
}

void sg_status_bar_set_battery(lv_obj_t *bar, uint8_t percent, bool charging)
{
    sg_tone_t tone = charging ? SG_TONE_GREEN : percent < 20 ? SG_TONE_RED : SG_TONE_TEXT;
    sg_icon_set(lv_obj_get_child(bar, 2), charging ? SG_ICON_BOLT : SG_ICON_BATTERY, tone);
}

void sg_status_bar_set_connected(lv_obj_t *bar, bool connected)
{
    sg_icon_set(lv_obj_get_child(bar, 0), SG_ICON_WIFI, connected ? SG_TONE_TEXT : SG_TONE_RAISED);
}

lv_obj_t *sg_page_dots(lv_obj_t *parent, uint8_t count, uint8_t active)
{
    lv_obj_t *dots = sg_row(parent);
    lv_obj_set_style_pad_column(dots, 6, 0);
    for (uint8_t i = 0; i < count; ++i) {
        lv_obj_t *dot = sg_plain(dots);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    }
    lv_obj_align(dots, LV_ALIGN_BOTTOM_MID, 0, -18);
    sg_page_dots_set(dots, active);
    return dots;
}

void sg_page_dots_set(lv_obj_t *dots, uint8_t active)
{
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(dots); ++i) {
        lv_obj_t *dot = lv_obj_get_child(dots, i);
        lv_obj_set_style_bg_color(dot, sg_color(i == active ? SG_TONE_ACCENT : SG_TONE_RAISED), 0);
        lv_obj_set_width(dot, i == active ? 16 : 8);
    }
}

static void follow_scroll(lv_event_t *e)
{
    lv_obj_t *page = lv_event_get_target(e), *track = lv_event_get_user_data(e);
    lv_obj_t *thumb = lv_obj_get_child(track, 0);
    lv_coord_t y = lv_obj_get_scroll_y(page), max = y + lv_obj_get_scroll_bottom(page);
    lv_coord_t room = lv_obj_get_content_height(track) - lv_obj_get_height(thumb);
    lv_coord_t offset = max > 0 ? (lv_coord_t)((int32_t)LV_CLAMP(0, y, max) * room / max) : 0;
    lv_obj_set_y(thumb, offset - offset % SG_UNIT);
}

lv_obj_t *sg_scroll_indicator(lv_obj_t *page)
{
    lv_obj_t *track = sg_plain(lv_obj_get_parent(page));
    lv_obj_set_size(track, 14, 112);
    sg_box(track, SG_BOX_INSET, SG_TONE_SURFACE, false);
    lv_obj_set_style_pad_all(track, 2 * SG_UNIT, 0);
    lv_obj_align(track, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_t *thumb = sg_plain(track);
    lv_obj_set_size(thumb, LV_PCT(100), 28);
    lv_obj_set_style_bg_color(thumb, sg_color(SG_TONE_ACCENT), 0);
    lv_obj_set_style_bg_opa(thumb, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(page, follow_scroll, LV_EVENT_SCROLL, track);
    return track;
}
