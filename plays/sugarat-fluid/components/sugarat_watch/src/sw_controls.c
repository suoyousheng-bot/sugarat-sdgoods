/* SPDX-License-Identifier: Apache-2.0 */
/** Sugarat Watch buttons, value controls and list rows. */
#include "sw_internal.h"
#include <stdio.h>

static void fill(lv_obj_t *obj, sw_tone_t tone, lv_opa_t opa, lv_style_selector_t selector)
{
    lv_obj_set_style_bg_color(obj, sw_color(tone), selector);
    lv_obj_set_style_bg_opa(obj, opa, selector);
}

lv_obj_t *sw_button(lv_obj_t *parent, sw_icon_t icon, const char *text, sw_button_variant_t variant, sw_tone_t tone)
{
    lv_obj_t *button = sw_plain(parent);
    sw_tappable(button);
    lv_obj_set_height(button, 54);
    lv_obj_set_style_min_width(button, 54, 0);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_hor(button, text ? 26 : 0, 0);
    sw_flex(button, LV_FLEX_FLOW_ROW, 8);
    sw_tone_t ink = tone;
    if (variant == SW_BUTTON_FILLED) {
        fill(button, tone, LV_OPA_COVER, 0);
        ink = sw_on_tone(tone);
    } else if (variant == SW_BUTTON_TINTED) {
        fill(button, tone, LV_OPA_30, 0);
    } else {
        fill(button, SW_TONE_RAISED, LV_OPA_COVER, 0);
        if (tone == SW_TONE_RAISED || tone == SW_TONE_SURFACE) ink = SW_TONE_TEXT;
    }
    if (icon != SW_ICON_NONE) sw_icon(button, icon, SW_SIZE_M, ink);
    if (text) sw_label(button, text, SW_FONT_HEADLINE, ink);
    return button;
}

lv_obj_t *sw_icon_button(lv_obj_t *parent, sw_icon_t icon, sw_tone_t tone)
{
    lv_obj_t *button = sw_button(parent, icon, NULL, tone == SW_TONE_TEXT ? SW_BUTTON_PLAIN : SW_BUTTON_TINTED, tone);
    lv_obj_set_size(button, 56, 56);
    return button;
}

static void toggle_button_ink(lv_event_t *e)
{
    lv_obj_t *button = lv_event_get_current_target(e);
    sw_tone_t tone = (sw_tone_t)(intptr_t)lv_event_get_user_data(e);
    bool on = lv_obj_has_state(button, LV_STATE_CHECKED);
    lv_obj_set_style_text_color(lv_obj_get_child(button, 0), sw_color(on ? sw_on_tone(tone) : SW_TONE_TEXT), 0);
}

lv_obj_t *sw_toggle_button(lv_obj_t *parent, sw_icon_t icon, sw_tone_t tone, bool on)
{
    lv_obj_t *button = sw_plain(parent);
    sw_tappable(button);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_size(button, 64, 64);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    fill(button, SW_TONE_RAISED, LV_OPA_COVER, 0);
    fill(button, tone, LV_OPA_COVER, LV_STATE_CHECKED);
    lv_obj_t *glyph = sw_icon(button, icon, SW_SIZE_L, SW_TONE_TEXT);
    lv_obj_center(glyph);
    if (on) lv_obj_add_state(button, LV_STATE_CHECKED);
    lv_obj_add_event_cb(button, toggle_button_ink, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)tone);
    lv_event_send(button, LV_EVENT_VALUE_CHANGED, NULL);
    return button;
}

lv_obj_t *sw_toggle(lv_obj_t *parent, bool on)
{
    lv_obj_t *sw = lv_switch_create(parent);
    lv_obj_remove_style_all(sw);
    lv_obj_add_flag(sw, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_size(sw, 56, 34);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    fill(sw, SW_TONE_RAISED, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    fill(sw, SW_TONE_GREEN, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    fill(sw, SW_TONE_TEXT, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_pad_all(sw, -3, LV_PART_KNOB);
    if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
    return sw;
}

lv_obj_t *sw_checkbox(lv_obj_t *parent, const char *text, bool checked)
{
    lv_obj_t *box = lv_checkbox_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_add_flag(box, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_checkbox_set_text(box, text);
    lv_obj_set_style_text_font(box, sw_font(SW_FONT_BODY), 0);
    lv_obj_set_style_text_color(box, sw_color(SW_TONE_TEXT), 0);
    lv_obj_set_style_pad_column(box, 12, 0);
    lv_obj_set_style_radius(box, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(box, 2, LV_PART_INDICATOR);
    lv_obj_set_style_border_color(box, sw_color(SW_TONE_MUTED), LV_PART_INDICATOR);
    lv_obj_set_style_border_color(box, sw_color(SW_TONE_GREEN), LV_PART_INDICATOR | LV_STATE_CHECKED);
    fill(box, SW_TONE_GREEN, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_pad_all(box, 5, LV_PART_INDICATOR);
    lv_obj_set_style_text_font(box, sw_icon_font(SW_SIZE_S), LV_PART_INDICATOR);
    lv_obj_set_style_text_color(box, sw_color(SW_TONE_BG), LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (checked) lv_obj_add_state(box, LV_STATE_CHECKED);
    return box;
}

lv_obj_t *sw_slider(lv_obj_t *parent, int32_t min, int32_t max, int32_t value, sw_tone_t tone)
{
    lv_obj_t *slider = lv_slider_create(parent);
    lv_obj_remove_style_all(slider);
    lv_obj_add_flag(slider, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_size(slider, 220, 10);
    lv_obj_set_ext_click_area(slider, 16);
    lv_slider_set_range(slider, min, max);
    lv_slider_set_value(slider, value, LV_ANIM_OFF);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    fill(slider, SW_TONE_RAISED, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    fill(slider, tone, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    fill(slider, SW_TONE_TEXT, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 7, LV_PART_KNOB);
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
    int action = sw_event_action(e);
    if (action != 0 && action != 1) return;
    stepper_t *s = lv_obj_get_user_data(stepper);
    int32_t next = LV_CLAMP(s->min, s->value + (action ? s->step : -s->step), s->max);
    if (next == s->value) return;
    s->value = next;
    stepper_render(stepper);
    lv_event_send(stepper, LV_EVENT_VALUE_CHANGED, NULL);
}

lv_obj_t *sw_stepper(lv_obj_t *parent, int32_t min, int32_t max, int32_t value, int32_t step)
{
    lv_obj_t *stepper = sw_row(parent);
    lv_obj_set_style_pad_column(stepper, 18, 0);
    stepper_t *s = sw_state(stepper, sizeof(*s));
    *s = (stepper_t){min, max, LV_CLAMP(min, value, max), step, NULL};
    sw_set_action(sw_icon_button(stepper, SW_ICON_MINUS, SW_TONE_TEXT), 0);
    s->label = sw_label(stepper, "", SW_FONT_NUM_M, SW_TONE_TEXT);
    lv_obj_set_style_min_width(s->label, 76, 0);
    lv_obj_set_style_text_align(s->label, LV_TEXT_ALIGN_CENTER, 0);
    sw_set_action(sw_icon_button(stepper, SW_ICON_PLUS, SW_TONE_TEXT), 1);
    lv_obj_add_event_cb(stepper, stepper_click, LV_EVENT_CLICKED, NULL);
    stepper_render(stepper);
    return stepper;
}

int32_t sw_stepper_get(lv_obj_t *stepper)
{
    return ((stepper_t *)lv_obj_get_user_data(stepper))->value;
}

void sw_stepper_set(lv_obj_t *stepper, int32_t value)
{
    stepper_t *s = lv_obj_get_user_data(stepper);
    s->value = LV_CLAMP(s->min, value, s->max);
    stepper_render(stepper);
}

lv_obj_t *sw_segmented(lv_obj_t *parent, const char *const *options, uint8_t count, uint8_t selected)
{
    lv_obj_t *matrix = lv_btnmatrix_create(parent);
    lv_obj_remove_style_all(matrix);
    lv_obj_add_flag(matrix, LV_OBJ_FLAG_EVENT_BUBBLE);
    const char **map = sw_state(matrix, sizeof(char *) * (count + 1u));
    for (uint8_t i = 0; i < count; ++i) map[i] = options[i];
    map[count] = "";
    lv_btnmatrix_set_map(matrix, map);
    lv_btnmatrix_set_btn_ctrl_all(matrix, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_one_checked(matrix, true);
    lv_btnmatrix_set_btn_ctrl(matrix, selected, LV_BTNMATRIX_CTRL_CHECKED);
    lv_obj_set_size(matrix, 260, 46);
    lv_obj_set_style_radius(matrix, LV_RADIUS_CIRCLE, 0);
    fill(matrix, SW_TONE_RAISED, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(matrix, 4, 0);
    lv_obj_set_style_pad_column(matrix, 2, 0);
    lv_obj_set_style_radius(matrix, LV_RADIUS_CIRCLE, LV_PART_ITEMS);
    lv_obj_set_style_text_font(matrix, sw_font(SW_FONT_BODY), LV_PART_ITEMS);
    lv_obj_set_style_text_color(matrix, sw_color(SW_TONE_MUTED), LV_PART_ITEMS);
    lv_obj_set_style_text_color(matrix, sw_color(SW_TONE_TEXT), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(matrix, lv_color_hex(0x636366), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);
    return matrix;
}

uint8_t sw_segmented_get(lv_obj_t *segmented)
{
    for (uint16_t i = 0; i < 255; ++i) {
        if (!lv_btnmatrix_get_btn_text(segmented, i)) break;
        if (lv_btnmatrix_has_btn_ctrl(segmented, i, LV_BTNMATRIX_CTRL_CHECKED)) return (uint8_t)i;
    }
    return 0;
}

lv_obj_t *sw_progress(lv_obj_t *parent, uint8_t percent, sw_tone_t tone)
{
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, 220, 10);
    lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    fill(bar, SW_TONE_RAISED, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    fill(bar, tone, LV_OPA_COVER, LV_PART_INDICATOR);
    sw_progress_set(bar, percent);
    return bar;
}

void sw_progress_set(lv_obj_t *progress, uint8_t percent)
{
    lv_bar_set_value(progress, LV_MIN(percent, 100), LV_ANIM_OFF);
}

static lv_obj_t *arc_base(lv_obj_t *parent, lv_coord_t size, lv_coord_t width, sw_tone_t tone)
{
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_remove_style_all(arc);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(arc, size, size);
    lv_obj_set_style_arc_width(arc, width, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, sw_color(tone), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, sw_color(tone), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    return arc;
}

lv_obj_t *sw_ring(lv_obj_t *parent, lv_coord_t size, uint8_t percent, sw_tone_t tone)
{
    lv_obj_t *ring = arc_base(parent, size, LV_MAX(6, size / 10), tone);
    lv_arc_set_rotation(ring, 270);
    lv_arc_set_bg_angles(ring, 0, 360);
    lv_arc_set_range(ring, 0, 100);
    sw_ring_set(ring, percent);
    return ring;
}

void sw_ring_set(lv_obj_t *ring, uint8_t percent)
{
    lv_arc_set_value(ring, LV_MIN(percent, 100));
}

lv_obj_t *sw_spinner(lv_obj_t *parent, lv_coord_t size, sw_tone_t tone)
{
    lv_obj_t *spinner = lv_spinner_create(parent, 1000, 70);
    lv_obj_remove_style_all(spinner);
    lv_obj_clear_flag(spinner, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(spinner, size, size);
    lv_coord_t width = LV_MAX(4, size / 9);
    lv_obj_set_style_arc_width(spinner, width, LV_PART_MAIN);
    lv_obj_set_style_arc_color(spinner, sw_color(SW_TONE_RAISED), LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner, sw_color(tone), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(spinner, true, LV_PART_INDICATOR);
    return spinner;
}

lv_obj_t *sw_roller(lv_obj_t *parent, const char *options, uint16_t selected, uint8_t visible_rows)
{
    lv_obj_t *roller = lv_roller_create(parent);
    lv_obj_remove_style_all(roller);
    lv_obj_add_flag(roller, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_roller_set_options(roller, options, LV_ROLLER_MODE_NORMAL);
    lv_obj_set_width(roller, 200);
    lv_obj_set_style_text_font(roller, sw_font(SW_FONT_HEADLINE), 0);
    lv_obj_set_style_text_color(roller, sw_color(SW_TONE_MUTED), 0);
    lv_obj_set_style_text_align(roller, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(roller, 22, 0);
    lv_obj_set_style_anim_time(roller, 200, 0);
    lv_obj_set_style_bg_color(roller, sw_color(SW_TONE_RAISED), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_radius(roller, 16, LV_PART_SELECTED);
    lv_obj_set_style_border_width(roller, 2, LV_PART_SELECTED);
    lv_obj_set_style_border_color(roller, sw_color(SW_TONE_ACCENT), LV_PART_SELECTED);
    lv_obj_set_style_text_color(roller, sw_color(SW_TONE_TEXT), LV_PART_SELECTED);
    lv_obj_set_style_text_font(roller, sw_font(SW_FONT_TITLE), LV_PART_SELECTED);
    lv_roller_set_visible_row_count(roller, visible_rows);
    lv_roller_set_selected(roller, selected, LV_ANIM_OFF);
    return roller;
}

/* ---------------------------------------------------------------- Lists */

lv_obj_t *sw_list_item(lv_obj_t *parent, sw_icon_t icon, sw_tone_t tone, const char *title, const char *subtitle)
{
    lv_obj_t *item = sw_plain(parent);
    sw_tappable(item);
    lv_obj_set_width(item, 280);
    lv_obj_set_style_min_height(item, 64, 0);
    lv_obj_set_style_radius(item, 22, 0);
    fill(item, SW_TONE_SURFACE, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(item, 14, 0);
    lv_obj_set_style_pad_ver(item, 10, 0);
    sw_flex(item, LV_FLEX_FLOW_ROW, 12);
    lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (icon != SW_ICON_NONE) {
        lv_obj_t *badge = sw_plain(item);
        lv_obj_set_size(badge, 36, 36);
        lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
        fill(badge, tone, LV_OPA_COVER, 0);
        lv_obj_center(sw_icon(badge, icon, SW_SIZE_S, sw_on_tone(tone)));
    }
    lv_obj_t *text = sw_plain(item);
    lv_obj_set_flex_grow(text, 1);
    lv_obj_set_flex_flow(text, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(text, 2, 0);
    lv_obj_t *label = sw_label(text, title, SW_FONT_BODY, SW_TONE_TEXT);
    lv_obj_set_size(label, LV_PCT(100), lv_font_get_line_height(sw_font(SW_FONT_BODY)));
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    if (subtitle) {
        label = sw_label(text, subtitle, SW_FONT_CAPTION, SW_TONE_MUTED);
        lv_obj_set_size(label, LV_PCT(100), lv_font_get_line_height(sw_font(SW_FONT_CAPTION)));
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    }
    return item;
}

lv_obj_t *sw_list_item_value(lv_obj_t *item, const char *text)
{
    return sw_label(item, text, SW_FONT_BODY, SW_TONE_MUTED);
}

lv_obj_t *sw_list_item_toggle(lv_obj_t *item, bool on)
{
    return sw_toggle(item, on);
}

void sw_list_item_chevron(lv_obj_t *item)
{
    sw_icon(item, SW_ICON_RIGHT, SW_SIZE_S, SW_TONE_MUTED);
}
