/* SPDX-License-Identifier: Apache-2.0 */
/** Sugarat Game RPG components: stat bars, hearts, dialog, menu, counters, inventory, achievements, title screen. */
#include "sg_internal.h"
#include <string.h>

/* ---------------------------------------------------------------- HP bar */

typedef struct {
    int32_t value, max;
    sg_tone_t tone;
} meter_t;

static void meter_draw(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    const meter_t *m = lv_obj_get_user_data(lv_obj_get_parent(obj));
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    sg_draw_box(ctx, &a, SG_BOX_INSET, SG_TONE_SURFACE);
    const lv_coord_t u = SG_UNIT, seg = 4 * u, gap = u;
    lv_coord_t inner = lv_area_get_width(&a) - 4 * u;
    int segments = (inner + gap) / (seg + gap);
    int filled = m->max > 0 ? (int)((LV_CLAMP(0, m->value, m->max) * segments + m->max - 1) / m->max) : 0;
    sg_tone_t tone = m->tone;
    if (m->max > 0 && m->value * 4 <= m->max) tone = SG_TONE_RED;
    lv_color_t on = sg_color(tone);
    for (int i = 0; i < filled; ++i) {
        lv_coord_t x = a.x1 + 2 * u + i * (seg + gap);
        sg_rect(ctx, x, a.y1 + 2 * u, x + seg - 1, a.y2 - 2 * u, on);
        sg_rect(ctx, x, a.y1 + 2 * u, x + seg - 1, a.y1 + 3 * u - 1, sg_shade(on, 120));
    }
}

lv_obj_t *sg_hp_bar(lv_obj_t *parent, sg_icon_t icon, int32_t value, int32_t max, sg_tone_t tone)
{
    lv_obj_t *bar = sg_row(parent);
    lv_obj_set_style_pad_column(bar, 8, 0);
    meter_t *m = sg_state(bar, sizeof(meter_t));
    *m = (meter_t){value, max, tone};
    sg_icon(bar, icon, SG_SIZE_M, tone);
    lv_obj_t *track = sg_plain(bar);
    lv_obj_set_size(track, 150, 22);
    lv_obj_add_event_cb(track, meter_draw, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_t *label = sg_label(bar, "", SG_FONT_CAPTION, SG_TONE_TEXT);
    lv_obj_set_style_min_width(label, 56, 0);
    sg_hp_bar_set(bar, value);
    return bar;
}

void sg_hp_bar_set(lv_obj_t *bar, int32_t value)
{
    meter_t *m = lv_obj_get_user_data(bar);
    m->value = LV_CLAMP(0, value, m->max);
    lv_label_set_text_fmt(lv_obj_get_child(bar, 2), "%" LV_PRId32 "/%" LV_PRId32, m->value, m->max);
    lv_obj_invalidate(lv_obj_get_child(bar, 1));
}

/* ---------------------------------------------------------------- Hearts */

lv_obj_t *sg_hearts(lv_obj_t *parent, uint8_t max, uint8_t value)
{
    lv_obj_t *hearts = sg_row(parent);
    lv_obj_set_style_pad_column(hearts, 4, 0);
    for (uint8_t i = 0; i < max; ++i) sg_icon(hearts, SG_ICON_HEART, SG_SIZE_M, SG_TONE_RED);
    sg_hearts_set(hearts, value);
    return hearts;
}

void sg_hearts_set(lv_obj_t *hearts, uint8_t value)
{
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(hearts); ++i)
        sg_icon_set(lv_obj_get_child(hearts, i), SG_ICON_HEART, i < value ? SG_TONE_RED : SG_TONE_SURFACE);
}

/* ---------------------------------------------------------------- Dialog */

typedef struct {
    char *text;
    uint32_t shown, length;
    lv_timer_t *typer, *blink;
    lv_obj_t *body, *label, *arrow;
} dialog_t;

static void dialog_render(dialog_t *d)
{
    char saved = d->text[d->shown];
    d->text[d->shown] = '\0';
    lv_label_set_text(d->label, d->text);
    d->text[d->shown] = saved;
}

static void dialog_type(lv_timer_t *timer)
{
    dialog_t *d = timer->user_data;
    if (d->shown >= d->length) return;
    uint32_t i = d->shown;
    _lv_txt_encoded_next(d->text, &i);
    d->shown = i;
    dialog_render(d);
}

static void dialog_blink(lv_timer_t *timer)
{
    dialog_t *d = timer->user_data;
    bool done = d->shown >= d->length;
    if (!done || lv_obj_has_flag(d->arrow, LV_OBJ_FLAG_HIDDEN) == false) lv_obj_add_flag(d->arrow, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(d->arrow, LV_OBJ_FLAG_HIDDEN);
}

static void dialog_delete(lv_event_t *e)
{
    dialog_t *d = lv_obj_get_user_data(lv_event_get_target(e));
    lv_timer_del(d->typer);
    lv_timer_del(d->blink);
    lv_mem_free(d->text);
}

static void dialog_click(lv_event_t *e)
{
    sg_dialog_skip(lv_event_get_current_target(e));
}

lv_obj_t *sg_dialog(lv_obj_t *parent, const char *name, const char *text)
{
    lv_obj_t *dialog = sg_plain(parent);
    lv_obj_set_width(dialog, 300);
    lv_obj_set_flex_flow(dialog, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(dialog, 0, 0);
    sg_tappable(dialog);
    lv_obj_add_event_cb(dialog, dialog_delete, LV_EVENT_DELETE, NULL); /* Must run before sg_state() frees d. */
    dialog_t *d = sg_state(dialog, sizeof(dialog_t));
    lv_obj_add_event_cb(dialog, dialog_click, LV_EVENT_CLICKED, NULL);
    lv_obj_t *tag = sg_plain(dialog);
    sg_box(tag, SG_BOX_BUTTON, SG_TONE_ACCENT, false);
    lv_obj_set_style_pad_hor(tag, 12, 0);
    lv_obj_set_style_pad_ver(tag, 4, 0);
    lv_obj_set_style_translate_x(tag, 14, 0);
    sg_label(tag, name ? name : "", SG_FONT_CAPTION, SG_TONE_BG);
    if (!name) lv_obj_add_flag(tag, LV_OBJ_FLAG_HIDDEN);
    d->body = sg_plain(dialog);
    lv_obj_set_width(d->body, LV_PCT(100));
    sg_box(d->body, SG_BOX_FRAME, SG_TONE_TEXT, true);
    lv_obj_set_style_pad_all(d->body, 16, 0);
    lv_obj_set_style_pad_right(d->body, 16 + SG_UNIT, 0);
    lv_obj_set_style_pad_bottom(d->body, 18 + SG_UNIT, 0);
    lv_obj_set_style_min_height(d->body, 108, 0);
    lv_obj_set_flex_flow(d->body, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(d->body, 12, 0);
    d->label = sg_label(d->body, "", SG_FONT_BODY, SG_TONE_TEXT);
    lv_obj_set_flex_grow(d->label, 1);
    lv_label_set_long_mode(d->label, LV_LABEL_LONG_WRAP);
    d->arrow = sg_icon(d->body, SG_ICON_DOWN, SG_SIZE_S, SG_TONE_ACCENT);
    lv_obj_add_flag(d->arrow, LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_FLOATING);
    lv_obj_align(d->arrow, LV_ALIGN_BOTTOM_RIGHT, 2, 14);
    d->typer = lv_timer_create(dialog_type, 45, d);
    d->blink = lv_timer_create(dialog_blink, 400, d);
    sg_dialog_set_text(dialog, text);
    return dialog;
}

void sg_dialog_set_text(lv_obj_t *dialog, const char *text)
{
    dialog_t *d = lv_obj_get_user_data(dialog);
    size_t length = strlen(text ? text : "");
    char *copy = lv_mem_alloc(length + 1);
    LV_ASSERT_MALLOC(copy);
    memcpy(copy, text ? text : "", length + 1);
    lv_mem_free(d->text);
    d->text = copy;
    d->length = (uint32_t)length;
    d->shown = 0;
    dialog_render(d);
    lv_obj_add_flag(d->arrow, LV_OBJ_FLAG_HIDDEN);
}

bool sg_dialog_skip(lv_obj_t *dialog)
{
    dialog_t *d = lv_obj_get_user_data(dialog);
    bool typing = d->shown < d->length;
    d->shown = d->length;
    dialog_render(d);
    lv_obj_clear_flag(d->arrow, LV_OBJ_FLAG_HIDDEN);
    return typing;
}

lv_obj_t *sg_dialog_portrait(lv_obj_t *dialog)
{
    dialog_t *d = lv_obj_get_user_data(dialog);
    lv_obj_t *first = lv_obj_get_child(d->body, 0);
    if (first != d->label && first != d->arrow) return first;
    lv_obj_t *slot = sg_plain(d->body);
    lv_obj_set_size(slot, 72, 72);
    sg_box(slot, SG_BOX_INSET, SG_TONE_SURFACE, false);
    sg_flex(slot, LV_FLEX_FLOW_COLUMN, 0);
    lv_obj_move_to_index(slot, 0);
    return slot;
}

/* ---------------------------------------------------------------- Menu */

static void menu_click(lv_event_t *e)
{
    int action = sg_event_action(e);
    if (action >= 0 && action < 1000) sg_menu_set_selected(lv_event_get_current_target(e), (uint8_t)action);
}

lv_obj_t *sg_menu(lv_obj_t *parent, const char *const *options, uint8_t count, uint8_t selected)
{
    lv_obj_t *menu = sg_plain(parent);
    sg_box(menu, SG_BOX_FRAME, SG_TONE_TEXT, true);
    lv_obj_set_width(menu, 240);
    lv_obj_set_style_pad_all(menu, 12, 0);
    lv_obj_set_style_pad_right(menu, 12 + SG_UNIT, 0);
    lv_obj_set_style_pad_bottom(menu, 12 + SG_UNIT, 0);
    lv_obj_set_flex_flow(menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(menu, 2, 0);
    for (uint8_t i = 0; i < count; ++i) {
        lv_obj_t *row = sg_row(menu);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_ver(row, 4, 0);
        sg_tappable(row);
        sg_set_action(row, i);
        sg_icon(row, SG_ICON_PLAY, SG_SIZE_S, SG_TONE_ACCENT);
        sg_label(row, options[i], SG_FONT_BODY, SG_TONE_TEXT);
    }
    lv_obj_add_event_cb(menu, menu_click, LV_EVENT_CLICKED, NULL);
    sg_menu_set_selected(menu, selected);
    return menu;
}

void sg_menu_set_selected(lv_obj_t *menu, uint8_t selected)
{
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(menu); ++i) {
        lv_obj_t *row = lv_obj_get_child(menu, i);
        bool on = i == selected;
        lv_obj_set_style_opa(lv_obj_get_child(row, 0), on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(row, 1), sg_color(on ? SG_TONE_ACCENT : SG_TONE_TEXT), 0);
        if (on) lv_obj_add_state(row, LV_STATE_CHECKED);
        else lv_obj_clear_state(row, LV_STATE_CHECKED);
    }
}

uint8_t sg_menu_get_selected(lv_obj_t *menu)
{
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(menu); ++i)
        if (lv_obj_has_state(lv_obj_get_child(menu, i), LV_STATE_CHECKED)) return (uint8_t)i;
    return 0;
}

/* ---------------------------------------------------------------- Counter and level */

lv_obj_t *sg_counter(lv_obj_t *parent, sg_icon_t icon, int32_t value, sg_tone_t tone)
{
    lv_obj_t *counter = sg_row(parent);
    lv_obj_set_style_pad_column(counter, 6, 0);
    sg_icon(counter, icon, SG_SIZE_M, tone);
    sg_label(counter, "", SG_FONT_NUM_S, SG_TONE_TEXT);
    sg_counter_set(counter, value);
    return counter;
}

void sg_counter_set(lv_obj_t *counter, int32_t value)
{
    lv_label_set_text_fmt(lv_obj_get_child(counter, 1), "x%" LV_PRId32, value);
}

lv_obj_t *sg_level(lv_obj_t *parent, uint16_t level, uint8_t exp_percent)
{
    lv_obj_t *box = sg_row(parent);
    lv_obj_set_style_pad_column(box, 8, 0);
    lv_obj_t *tag = sg_plain(box);
    sg_box(tag, SG_BOX_BUTTON, SG_TONE_PURPLE, false);
    lv_obj_set_style_pad_hor(tag, 8, 0);
    lv_obj_set_style_pad_ver(tag, 4, 0);
    sg_label(tag, "", SG_FONT_CAPTION, SG_TONE_TEXT);
    lv_obj_t *column = sg_plain(box);
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(column, 2, 0);
    sg_label(column, "EXP", SG_FONT_CAPTION, SG_TONE_MUTED);
    lv_obj_t *bar = sg_progress(column, exp_percent, SG_TONE_PURPLE);
    lv_obj_set_size(bar, 150, 16);
    sg_level_set(box, level, exp_percent);
    return box;
}

void sg_level_set(lv_obj_t *level, uint16_t value, uint8_t exp_percent)
{
    lv_label_set_text_fmt(lv_obj_get_child(lv_obj_get_child(level, 0), 0), "LV%u", value);
    sg_progress_set(lv_obj_get_child(lv_obj_get_child(level, 1), 1), exp_percent);
}

/* ---------------------------------------------------------------- Inventory */

static void inventory_click(lv_event_t *e)
{
    int action = sg_event_action(e);
    if (action >= 0 && action < 1000) sg_inventory_select(lv_event_get_current_target(e), (uint8_t)action);
}

lv_obj_t *sg_inventory(lv_obj_t *parent, uint8_t cols, uint8_t rows)
{
    const lv_coord_t slot = 54, gap = 6;
    uint8_t count = (uint8_t)LV_MIN(cols * rows, 32);
    lv_obj_t *grid = sg_plain(parent);
    lv_obj_set_width(grid, cols * slot + (cols - 1) * gap);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(grid, gap, 0);
    lv_obj_set_style_pad_column(grid, gap, 0);
    for (uint8_t i = 0; i < count; ++i) {
        lv_obj_t *cell = sg_plain(grid);
        lv_obj_set_size(cell, slot, slot);
        sg_box(cell, SG_BOX_INSET, SG_TONE_SURFACE, false);
        sg_tappable(cell);
        sg_set_action(cell, i);
        lv_obj_set_style_outline_color(cell, sg_color(SG_TONE_ACCENT), LV_STATE_CHECKED);
        lv_obj_set_style_outline_width(cell, SG_UNIT * 2, LV_STATE_CHECKED);
        lv_obj_set_style_outline_pad(cell, 0, LV_STATE_CHECKED);
        lv_obj_center(sg_icon(cell, SG_ICON_NONE, SG_SIZE_M, SG_TONE_TEXT));
        lv_obj_t *number = sg_label(cell, "", SG_FONT_CAPTION, SG_TONE_TEXT);
        lv_obj_align(number, LV_ALIGN_BOTTOM_RIGHT, -3, 1);
    }
    lv_obj_add_event_cb(grid, inventory_click, LV_EVENT_CLICKED, NULL);
    return grid;
}

void sg_inventory_set(lv_obj_t *inventory, uint8_t slot, sg_icon_t icon, sg_tone_t tone, uint16_t count)
{
    lv_obj_t *cell = lv_obj_get_child(inventory, slot);
    if (!cell) return;
    sg_icon_set(lv_obj_get_child(cell, 0), icon, tone);
    lv_obj_t *number = lv_obj_get_child(cell, 1);
    if (count && icon != SG_ICON_NONE) lv_label_set_text_fmt(number, "%u", count);
    else lv_label_set_text_static(number, "");
}

void sg_inventory_select(lv_obj_t *inventory, uint8_t slot)
{
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(inventory); ++i) {
        lv_obj_t *cell = lv_obj_get_child(inventory, i);
        if (i == slot) lv_obj_add_state(cell, LV_STATE_CHECKED);
        else lv_obj_clear_state(cell, LV_STATE_CHECKED);
    }
}

/* ---------------------------------------------------------------- Achievement and title */

static void slide(void *obj, int32_t v)
{
    lv_obj_set_style_translate_y(obj, v - v % SG_UNIT, 0);
}

lv_obj_t *sg_achievement(lv_obj_t *parent, sg_icon_t icon, const char *title, uint32_t duration_ms)
{
    lv_obj_t *banner = sg_plain(parent ? parent : lv_layer_top());
    sg_box(banner, SG_BOX_FRAME, SG_TONE_ACCENT, true);
    lv_obj_set_style_pad_all(banner, 12, 0);
    lv_obj_set_style_pad_right(banner, 16 + SG_UNIT, 0);
    lv_obj_set_style_pad_bottom(banner, 12 + SG_UNIT, 0);
    sg_flex(banner, LV_FLEX_FLOW_ROW, 12);
    lv_obj_t *slot = sg_plain(banner);
    sg_box(slot, SG_BOX_BUTTON, SG_TONE_ACCENT, false);
    lv_obj_set_style_pad_all(slot, 6, 0);
    sg_icon(slot, icon, SG_SIZE_M, SG_TONE_BG);
    lv_obj_t *text = sg_plain(banner);
    lv_obj_set_flex_flow(text, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(text, 2, 0);
    sg_label(text, "成就解锁!", SG_FONT_CAPTION, SG_TONE_ACCENT);
    sg_label(text, title, SG_FONT_BODY, SG_TONE_TEXT);
    lv_obj_align(banner, LV_ALIGN_TOP_MID, 0, 40);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, banner);
    lv_anim_set_exec_cb(&a, slide);
    lv_anim_set_values(&a, -120, 0);
    lv_anim_set_time(&a, 300);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_start(&a);
    if (duration_ms) lv_obj_del_delayed(banner, duration_ms);
    return banner;
}

static void prompt_blink(lv_timer_t *timer)
{
    lv_obj_t *prompt = timer->user_data;
    lv_obj_set_style_opa(prompt, lv_obj_get_style_opa(prompt, 0) ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
}

static void prompt_stop(lv_event_t *e)
{
    lv_timer_del(lv_event_get_user_data(e));
}

lv_obj_t *sg_title_screen(lv_obj_t *parent, const char *title, const char *subtitle, const char *prompt)
{
    lv_obj_t *block = sg_column(parent);
    lv_obj_set_style_pad_row(block, 10, 0);
    lv_obj_t *stack = sg_plain(block);
    lv_obj_t *shadow = sg_label(stack, title, SG_FONT_TITLE, SG_TONE_PURPLE);
    lv_obj_set_pos(shadow, 3 * SG_UNIT, 3 * SG_UNIT);
    lv_obj_t *front = sg_label(stack, title, SG_FONT_TITLE, SG_TONE_ACCENT);
    lv_obj_set_pos(front, 0, 0);
    lv_obj_set_style_pad_right(front, 3 * SG_UNIT, 0);
    lv_obj_set_style_pad_bottom(front, 3 * SG_UNIT, 0);
    if (subtitle) sg_label(block, subtitle, SG_FONT_CAPTION, SG_TONE_TEAL);
    lv_obj_t *label = sg_label(block, prompt, SG_FONT_BODY, SG_TONE_TEXT);
    lv_obj_set_style_pad_top(label, 26, 0);
    lv_timer_t *timer = lv_timer_create(prompt_blink, 500, label);
    lv_obj_add_event_cb(label, prompt_stop, LV_EVENT_DELETE, timer);
    return block;
}
