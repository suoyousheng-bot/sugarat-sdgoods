/* SPDX-License-Identifier: Apache-2.0 */
/** Sugarat Game buttons, value controls and list rows. */
#include "sg_internal.h"

static void set_ink(lv_obj_t *button, sg_tone_t ink)
{
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(button); ++i)
        lv_obj_set_style_text_color(lv_obj_get_child(button, i), sg_color(ink), 0);
}

lv_obj_t *sg_button(lv_obj_t *parent, sg_icon_t icon, const char *text, sg_button_variant_t variant, sg_tone_t tone)
{
    lv_obj_t *button = sg_plain(parent);
    sg_tappable(button);
    lv_obj_set_height(button, 54);
    lv_obj_set_style_min_width(button, 54, 0);
    lv_obj_set_style_pad_left(button, text ? 20 : 0, 0);
    lv_obj_set_style_pad_right(button, (text ? 20 : 0) + SG_UNIT, 0);
    lv_obj_set_style_pad_bottom(button, SG_UNIT, 0);
    sg_flex(button, LV_FLEX_FLOW_ROW, 8);
    sg_tone_t ink = tone;
    if (variant == SG_BUTTON_FILLED) {
        sg_box(button, SG_BOX_BUTTON, tone, true);
        ink = sg_on_tone(tone);
    } else if (variant == SG_BUTTON_TINTED) {
        sg_box(button, SG_BOX_FRAME, tone, true);
    } else {
        sg_box(button, SG_BOX_PANEL, SG_TONE_SURFACE, true);
        if (tone == SG_TONE_SURFACE || tone == SG_TONE_RAISED) ink = SG_TONE_TEXT;
    }
    if (icon != SG_ICON_NONE) sg_icon(button, icon, SG_SIZE_M, ink);
    if (text) sg_label(button, text, SG_FONT_BODY, ink);
    return button;
}

lv_obj_t *sg_icon_button(lv_obj_t *parent, sg_icon_t icon, sg_tone_t tone)
{
    lv_obj_t *button = sg_button(parent, icon, NULL, tone == SG_TONE_TEXT ? SG_BUTTON_PLAIN : SG_BUTTON_TINTED, tone);
    lv_obj_set_size(button, 56, 56);
    return button;
}

static void toggle_button_ink(lv_event_t *e)
{
    lv_obj_t *button = lv_event_get_current_target(e);
    sg_tone_t tone = (sg_tone_t)(intptr_t)lv_event_get_user_data(e);
    bool on = lv_obj_has_state(button, LV_STATE_CHECKED);
    sg_box_set_tone(button, on ? tone : SG_TONE_SURFACE);
    set_ink(button, on ? sg_on_tone(tone) : SG_TONE_MUTED);
}

lv_obj_t *sg_toggle_button(lv_obj_t *parent, sg_icon_t icon, sg_tone_t tone, bool on)
{
    lv_obj_t *button = sg_plain(parent);
    sg_tappable(button);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_size(button, 64, 64);
    lv_obj_set_style_pad_right(button, SG_UNIT, 0);
    lv_obj_set_style_pad_bottom(button, SG_UNIT, 0);
    sg_box(button, SG_BOX_BUTTON, tone, true);
    lv_obj_center(sg_icon(button, icon, SG_SIZE_L, SG_TONE_TEXT));
    if (on) lv_obj_add_state(button, LV_STATE_CHECKED);
    lv_obj_add_event_cb(button, toggle_button_ink, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)tone);
    lv_event_send(button, LV_EVENT_VALUE_CHANGED, NULL);
    return button;
}

static void switch_draw(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    bool on = lv_obj_has_state(obj, LV_STATE_CHECKED);
    sg_draw_box(ctx, &a, SG_BOX_INSET, SG_TONE_SURFACE);
    const lv_coord_t u = SG_UNIT;
    if (on) sg_rect(ctx, a.x1 + 2 * u, a.y1 + 2 * u, a.x2 - 2 * u, a.y2 - 2 * u, sg_color(SG_TONE_GREEN));
    lv_coord_t knob = lv_area_get_height(&a) - 2 * u;
    lv_area_t k = {on ? a.x2 - u - knob + 1 : a.x1 + u, a.y1 + u, 0, a.y2 - u};
    k.x2 = k.x1 + knob - 1;
    sg_draw_box(ctx, &k, SG_BOX_BUTTON, SG_TONE_TEXT);
}

lv_obj_t *sg_toggle(lv_obj_t *parent, bool on)
{
    lv_obj_t *sw = sg_plain(parent);
    sg_tappable(sw);
    lv_obj_add_flag(sw, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_size(sw, 64, 34);
    lv_obj_add_event_cb(sw, switch_draw, LV_EVENT_DRAW_MAIN, NULL);
    if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
    return sw;
}

static void checkbox_sync(lv_event_t *e)
{
    lv_obj_t *box = lv_event_get_current_target(e);
    lv_obj_t *mark = lv_obj_get_child(lv_obj_get_child(box, 0), 0);
    if (lv_obj_has_state(box, LV_STATE_CHECKED)) lv_obj_clear_flag(mark, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(mark, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *sg_checkbox(lv_obj_t *parent, const char *text, bool checked)
{
    lv_obj_t *box = sg_row(parent);
    sg_tappable(box);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_style_pad_column(box, 12, 0);
    lv_obj_t *square = sg_plain(box);
    lv_obj_set_size(square, 28, 28);
    sg_box(square, SG_BOX_INSET, SG_TONE_SURFACE, false);
    lv_obj_center(sg_icon(square, SG_ICON_CHECK, SG_SIZE_S, SG_TONE_GREEN));
    sg_label(box, text, SG_FONT_BODY, SG_TONE_TEXT);
    if (checked) lv_obj_add_state(box, LV_STATE_CHECKED);
    lv_obj_add_event_cb(box, checkbox_sync, LV_EVENT_VALUE_CHANGED, NULL);
    lv_event_send(box, LV_EVENT_VALUE_CHANGED, NULL);
    return box;
}

static void slider_track(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    sg_draw_box(lv_event_get_draw_ctx(e), &a, SG_BOX_INSET, SG_TONE_SURFACE);
}

lv_obj_t *sg_slider(lv_obj_t *parent, int32_t min, int32_t max, int32_t value, sg_tone_t tone)
{
    lv_obj_t *slider = lv_slider_create(parent);
    lv_obj_remove_style_all(slider);
    lv_obj_add_flag(slider, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_size(slider, 220, 18);
    lv_obj_set_ext_click_area(slider, 14);
    lv_slider_set_range(slider, min, max);
    lv_slider_set_value(slider, value, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, slider_track, LV_EVENT_DRAW_MAIN_BEGIN, NULL);
    lv_obj_set_style_pad_all(slider, 2 * SG_UNIT, 0);
    lv_obj_set_style_bg_color(slider, sg_color(tone), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, sg_color(SG_TONE_TEXT), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_border_width(slider, SG_UNIT, LV_PART_KNOB);
    lv_obj_set_style_border_color(slider, lv_color_hex(SG_INK), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 6, LV_PART_KNOB);
    return slider;
}

typedef struct {
    int32_t min, max, value, step;
    lv_obj_t *label;
} stepper_t;

static void stepper_render(lv_obj_t *stepper)
{
    stepper_t *s = lv_obj_get_user_data(stepper);
    lv_label_set_text_fmt(s->label, "%" LV_PRId32, s->value);
}

static void stepper_click(lv_event_t *e)
{
    lv_obj_t *stepper = lv_event_get_current_target(e);
    int action = sg_event_action(e);
    if (action != 0 && action != 1) return;
    stepper_t *s = lv_obj_get_user_data(stepper);
    int32_t next = LV_CLAMP(s->min, s->value + (action ? s->step : -s->step), s->max);
    if (next == s->value) return;
    s->value = next;
    stepper_render(stepper);
    lv_event_send(stepper, LV_EVENT_VALUE_CHANGED, NULL);
}

lv_obj_t *sg_stepper(lv_obj_t *parent, int32_t min, int32_t max, int32_t value, int32_t step)
{
    lv_obj_t *stepper = sg_row(parent);
    lv_obj_set_style_pad_column(stepper, 14, 0);
    stepper_t *s = sg_state(stepper, sizeof(*s));
    *s = (stepper_t){min, max, LV_CLAMP(min, value, max), step, NULL};
    sg_set_action(sg_icon_button(stepper, SG_ICON_MINUS, SG_TONE_TEXT), 0);
    s->label = sg_label(stepper, "", SG_FONT_NUM_M, SG_TONE_ACCENT);
    lv_obj_set_style_min_width(s->label, 96, 0);
    lv_obj_set_style_text_align(s->label, LV_TEXT_ALIGN_CENTER, 0);
    sg_set_action(sg_icon_button(stepper, SG_ICON_PLUS, SG_TONE_TEXT), 1);
    lv_obj_add_event_cb(stepper, stepper_click, LV_EVENT_CLICKED, NULL);
    stepper_render(stepper);
    return stepper;
}

int32_t sg_stepper_get(lv_obj_t *stepper)
{
    return ((stepper_t *)lv_obj_get_user_data(stepper))->value;
}

void sg_stepper_set(lv_obj_t *stepper, int32_t value)
{
    stepper_t *s = lv_obj_get_user_data(stepper);
    s->value = LV_CLAMP(s->min, value, s->max);
    stepper_render(stepper);
}

lv_obj_t *sg_segmented(lv_obj_t *parent, const char *const *options, uint8_t count, uint8_t selected)
{
    lv_obj_t *matrix = lv_btnmatrix_create(parent);
    lv_obj_remove_style_all(matrix);
    lv_obj_add_flag(matrix, LV_OBJ_FLAG_EVENT_BUBBLE);
    const char **map = sg_state(matrix, sizeof(char *) * (count + 1u));
    for (uint8_t i = 0; i < count; ++i) map[i] = options[i];
    map[count] = "";
    lv_btnmatrix_set_map(matrix, map);
    lv_btnmatrix_set_btn_ctrl_all(matrix, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_one_checked(matrix, true);
    lv_btnmatrix_set_btn_ctrl(matrix, selected, LV_BTNMATRIX_CTRL_CHECKED);
    lv_obj_set_size(matrix, 260, 48);
    sg_box(matrix, SG_BOX_INSET, SG_TONE_SURFACE, false);
    lv_obj_set_style_pad_all(matrix, 2 * SG_UNIT, 0);
    lv_obj_set_style_pad_column(matrix, SG_UNIT, 0);
    lv_obj_set_style_text_font(matrix, sg_font(SG_FONT_BODY), LV_PART_ITEMS);
    lv_obj_set_style_text_color(matrix, sg_color(SG_TONE_MUTED), LV_PART_ITEMS);
    lv_obj_set_style_text_color(matrix, sg_color(SG_TONE_BG), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(matrix, sg_color(SG_TONE_ACCENT), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);
    return matrix;
}

uint8_t sg_segmented_get(lv_obj_t *segmented)
{
    for (uint16_t i = 0; i < 255; ++i) {
        if (!lv_btnmatrix_get_btn_text(segmented, i)) break;
        if (lv_btnmatrix_has_btn_ctrl(segmented, i, LV_BTNMATRIX_CTRL_CHECKED)) return (uint8_t)i;
    }
    return 0;
}

#define PROGRESS_PACK(percent, tone) ((void *)(uintptr_t)((percent) | ((tone) << 8)))

static void progress_draw(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    uintptr_t packed = (uintptr_t)lv_obj_get_user_data(obj);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    sg_draw_box(ctx, &a, SG_BOX_INSET, SG_TONE_SURFACE);
    const int segments = 10, gap = SG_UNIT, u = SG_UNIT;
    lv_coord_t inner = lv_area_get_width(&a) - 4 * u, w = (inner - gap * (segments - 1)) / segments;
    lv_coord_t x = a.x1 + 2 * u + (inner - (w * segments + gap * (segments - 1))) / 2;
    int filled = (int)((packed & 0xFF) + 5) / 10;
    lv_color_t on = sg_color((sg_tone_t)(packed >> 8));
    for (int i = 0; i < segments; ++i, x += w + gap) {
        lv_color_t c = i < filled ? on : sg_color(SG_TONE_SURFACE);
        sg_rect(ctx, x, a.y1 + 2 * u, x + w - 1, a.y2 - 2 * u, c);
        if (i < filled) sg_rect(ctx, x, a.y1 + 2 * u, x + w - 1, a.y1 + 3 * u - 1, sg_shade(on, 120));
    }
}

lv_obj_t *sg_progress(lv_obj_t *parent, uint8_t percent, sg_tone_t tone)
{
    lv_obj_t *bar = sg_plain(parent);
    lv_obj_set_size(bar, 224, 22);
    lv_obj_set_user_data(bar, PROGRESS_PACK(LV_MIN(percent, 100), tone));
    lv_obj_add_event_cb(bar, progress_draw, LV_EVENT_DRAW_MAIN, NULL);
    return bar;
}

void sg_progress_set(lv_obj_t *progress, uint8_t percent)
{
    uintptr_t packed = (uintptr_t)lv_obj_get_user_data(progress);
    lv_obj_set_user_data(progress, PROGRESS_PACK(LV_MIN(percent, 100), packed >> 8));
    lv_obj_invalidate(progress);
}

static uint8_t ring_count(lv_coord_t size)
{
    return (uint8_t)LV_CLAMP(12, size / 8, 40);
}

lv_obj_t *sg_ring(lv_obj_t *parent, lv_coord_t size, uint8_t percent, sg_tone_t tone)
{
    uint8_t count = ring_count(size);
    sg_blocks_t b = {270, 360, count, 0, (uint8_t)LV_CLAMP(6, size / 9 / SG_UNIT * SG_UNIT, 12), -1, tone};
    lv_obj_t *ring = sg_block_ring(parent, size, &b);
    sg_ring_set(ring, percent);
    return ring;
}

void sg_ring_set(lv_obj_t *ring, uint8_t percent)
{
    sg_blocks_t *b = lv_obj_get_user_data(ring);
    b->filled = (uint8_t)((LV_MIN(percent, 100) * b->count + 50) / 100);
    lv_obj_invalidate(ring);
}

static void spin(lv_timer_t *timer)
{
    sg_blocks_t *b = lv_obj_get_user_data(timer->user_data);
    b->head = (int8_t)((b->head + 1) % b->count);
    lv_obj_invalidate(timer->user_data);
}

static void spin_stop(lv_event_t *e)
{
    lv_timer_del(lv_event_get_user_data(e));
}

lv_obj_t *sg_spinner(lv_obj_t *parent, lv_coord_t size, sg_tone_t tone)
{
    sg_blocks_t b = {270, 360, 8, 0, (uint8_t)LV_CLAMP(6, size / 6 / SG_UNIT * SG_UNIT, 14), 0, tone};
    lv_obj_t *spinner = sg_block_ring(parent, size, &b);
    lv_timer_t *timer = lv_timer_create(spin, 110, spinner);
    lv_obj_add_event_cb(spinner, spin_stop, LV_EVENT_DELETE, timer);
    return spinner;
}

static void roller_frame(lv_event_t *e)
{
    lv_obj_t *roller = lv_event_get_target(e);
    lv_area_t a;
    lv_obj_get_coords(roller, &a);
    lv_coord_t h = lv_font_get_line_height(sg_font(SG_FONT_TITLE)) + 16, mid = (a.y1 + a.y2) / 2;
    lv_area_t f = {a.x1, mid - h / 2, a.x2, mid + h / 2};
    sg_draw_box(lv_event_get_draw_ctx(e), &f, SG_BOX_FRAME, SG_TONE_ACCENT);
}

lv_obj_t *sg_roller(lv_obj_t *parent, const char *options, uint16_t selected, uint8_t visible_rows)
{
    lv_obj_t *roller = lv_roller_create(parent);
    lv_obj_remove_style_all(roller);
    lv_obj_add_flag(roller, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_roller_set_options(roller, options, LV_ROLLER_MODE_NORMAL);
    lv_obj_set_width(roller, 200);
    lv_obj_set_style_text_font(roller, sg_font(SG_FONT_BODY), 0);
    lv_obj_set_style_text_color(roller, sg_color(SG_TONE_MUTED), 0);
    lv_obj_set_style_text_align(roller, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(roller, 26, 0);
    lv_obj_set_style_anim_time(roller, 160, 0);
    lv_obj_set_style_text_color(roller, sg_color(SG_TONE_ACCENT), LV_PART_SELECTED);
    lv_obj_set_style_text_font(roller, sg_font(SG_FONT_TITLE), LV_PART_SELECTED);
    lv_obj_add_event_cb(roller, roller_frame, LV_EVENT_DRAW_MAIN_BEGIN, NULL);
    lv_roller_set_visible_row_count(roller, visible_rows);
    lv_roller_set_selected(roller, selected, LV_ANIM_OFF);
    return roller;
}

/* ---------------------------------------------------------------- Lists */

lv_obj_t *sg_list_item(lv_obj_t *parent, sg_icon_t icon, sg_tone_t tone, const char *title, const char *subtitle)
{
    lv_obj_t *item = sg_plain(parent);
    sg_tappable(item);
    lv_obj_set_width(item, 280);
    lv_obj_set_style_min_height(item, 64, 0);
    sg_box(item, SG_BOX_PANEL, SG_TONE_SURFACE, true);
    lv_obj_set_style_pad_left(item, 12, 0);
    lv_obj_set_style_pad_right(item, 12 + SG_UNIT, 0);
    lv_obj_set_style_pad_top(item, 8, 0);
    lv_obj_set_style_pad_bottom(item, 8 + SG_UNIT, 0);
    sg_flex(item, LV_FLEX_FLOW_ROW, 10);
    lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (icon != SG_ICON_NONE) {
        lv_obj_t *slot = sg_plain(item);
        lv_obj_set_size(slot, 36, 36);
        sg_box(slot, SG_BOX_INSET, SG_TONE_SURFACE, false);
        lv_obj_center(sg_icon(slot, icon, SG_SIZE_S, tone));
    }
    lv_obj_t *text = sg_plain(item);
    lv_obj_set_flex_grow(text, 1);
    lv_obj_set_flex_flow(text, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(text, 2, 0);
    lv_obj_t *label = sg_label(text, title, SG_FONT_BODY, SG_TONE_TEXT);
    lv_obj_set_size(label, LV_PCT(100), lv_font_get_line_height(sg_font(SG_FONT_BODY)));
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    if (subtitle) {
        label = sg_label(text, subtitle, SG_FONT_CAPTION, SG_TONE_MUTED);
        lv_obj_set_size(label, LV_PCT(100), lv_font_get_line_height(sg_font(SG_FONT_CAPTION)));
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    }
    return item;
}

lv_obj_t *sg_list_item_value(lv_obj_t *item, const char *text)
{
    return sg_label(item, text, SG_FONT_CAPTION, SG_TONE_ACCENT);
}

lv_obj_t *sg_list_item_toggle(lv_obj_t *item, bool on)
{
    return sg_toggle(item, on);
}

void sg_list_item_chevron(lv_obj_t *item)
{
    sg_icon(item, SG_ICON_RIGHT, SG_SIZE_S, SG_TONE_MUTED);
}
