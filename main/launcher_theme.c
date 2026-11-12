/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 */
#include "launcher_theme.h"

lv_obj_t *th_screen(void) {
    lv_obj_t *screen = sw_screen();
    lv_obj_set_style_bg_color(screen, lv_color_hex(TH_BG), 0);
    return screen;
}

lv_obj_t *th_plain(lv_obj_t *parent) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
    return obj;
}

lv_obj_t *th_box(lv_obj_t *parent, uint32_t color, lv_coord_t radius) {
    lv_obj_t *box = th_plain(parent);
    lv_obj_set_style_radius(box, radius, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    return box;
}

void th_tap(lv_obj_t *obj, int action) {
    lv_color_t color = lv_obj_get_style_bg_color(obj, LV_PART_MAIN);
    lv_color_t pressed = lv_color_brightness(color) > 140 ? lv_color_darken(color, LV_OPA_20) : lv_color_lighten(color, LV_OPA_20);
    lv_obj_set_style_bg_color(obj, pressed, LV_STATE_PRESSED);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    sw_set_action(obj, action);
}

lv_obj_t *th_text(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color) {
    lv_obj_t *label = sw_label(parent, text, SW_FONT_BODY, SW_TONE_TEXT);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

lv_obj_t *th_line(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color, lv_coord_t width) {
    lv_obj_t *label = th_text(parent, text, font, color);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_size(label, width, lv_font_get_line_height(font));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

lv_obj_t *th_wrap(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color, lv_coord_t width) {
    lv_obj_t *label = th_text(parent, text, font, color);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

lv_obj_t *th_icon(lv_obj_t *parent, sw_icon_t icon, sw_size_t size, uint32_t color) {
    lv_obj_t *label = sw_icon(parent, icon, size, SW_TONE_TEXT);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

lv_obj_t *th_badge(lv_obj_t *parent, sw_icon_t icon, uint32_t color, lv_coord_t size) {
    lv_obj_t *badge = th_box(parent, color, size * 3 / 10);
    lv_obj_set_size(badge, size, size);
    sw_size_t icon_size = size >= 60 ? SW_SIZE_XL : size >= 44 ? SW_SIZE_L : SW_SIZE_M;
    lv_obj_center(th_icon(badge, icon, icon_size, TH_TEXT));
    return badge;
}

lv_obj_t *th_button(lv_obj_t *parent, const char *text, th_button_t kind, int action) {
    lv_obj_t *button = th_box(parent, kind == TH_PRIMARY ? TH_ACCENT : TH_KEY, 24);
    lv_obj_set_size(button, 220, 48);
    th_tap(button, action);
    uint32_t color = kind == TH_PRIMARY ? TH_ON_ACCENT : kind == TH_DANGER ? TH_RED : TH_TEXT;
    lv_obj_center(th_text(button, text, sw_font(SW_FONT_BODY), color));
    return button;
}

lv_obj_t *th_image_badge(lv_obj_t *parent, const lv_img_dsc_t *image) {
    lv_obj_t *badge = th_box(parent, TH_KEY, image->header.w * 3 / 10);
    lv_obj_set_size(badge, image->header.w, image->header.h);
    lv_obj_set_style_clip_corner(badge, true, 0);
    lv_obj_t *picture = lv_img_create(badge);
    lv_obj_remove_style_all(picture);
    lv_obj_clear_flag(picture, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(picture, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_img_set_src(picture, image);
    lv_obj_center(picture);
    return badge;
}

lv_obj_t *th_tile(lv_obj_t *parent, sw_icon_t icon, uint32_t icon_color, const lv_img_dsc_t *image, const char *title,
                  const char *subtitle, lv_coord_t width, lv_coord_t height, int action) {
    lv_obj_t *tile = th_box(parent, TH_CARD, 20);
    lv_obj_set_size(tile, width, height);
    th_tap(tile, action);
    bool large = height >= 130;   /* App tiles: a two-column grid with full-size covers. */
    lv_coord_t badge = image ? image->header.h : large ? 84 : width >= 100 ? 40 : 44;
    lv_obj_t *mark = image ? th_image_badge(tile, image) : th_badge(tile, icon, icon_color, badge);
    lv_obj_align(mark, LV_ALIGN_TOP_MID, 0, subtitle ? 12 : (height - badge - 26) / 2);
    lv_obj_t *label = th_line(tile, title, sw_font(subtitle || large ? SW_FONT_BODY : SW_FONT_CAPTION), TH_TEXT, width - 12);
    lv_obj_align_to(label, mark, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);
    if (subtitle) {
        lv_obj_t *second = th_line(tile, subtitle, sw_font(SW_FONT_CAPTION), TH_MUTED, width - 12);
        lv_obj_align_to(second, label, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    }
    return tile;
}

static void round_button(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_coord_t x, int action) {
    lv_obj_t *button = th_box(parent, TH_KEY, LV_RADIUS_CIRCLE);
    lv_obj_set_size(button, 40, 40);
    lv_obj_set_pos(button, x, 38);
    th_tap(button, action);
    lv_obj_center(th_text(button, text, font, TH_TEXT));
}

void th_header(lv_obj_t *screen, const char *title, int help_action) {
    lv_obj_t *band = th_box(screen, TH_BG, 0);
    lv_obj_set_size(band, 360, 84);
    round_button(band, LV_SYMBOL_LEFT, &lv_font_montserrat_14, 76, SW_ACTION_BACK);
    if (help_action != SW_ACTION_NONE) round_button(band, "?", sw_font(SW_FONT_HEADLINE), 244, help_action);
    lv_obj_t *label = th_line(band, title, sw_font(SW_FONT_HEADLINE), TH_TEXT, 150);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 45);
}

lv_obj_t *th_grid(lv_obj_t *screen) {
    lv_obj_t *grid = lv_obj_create(screen);
    lv_obj_remove_style_all(grid);
    lv_obj_add_flag(grid, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_size(grid, 360, 360);
    lv_obj_set_scroll_dir(grid, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_top(grid, 92, 0);
    lv_obj_set_style_pad_bottom(grid, 76, 0);
    lv_obj_set_style_pad_row(grid, 10, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return grid;
}

lv_obj_t *th_grid_row(lv_obj_t *grid) {
    lv_obj_t *row = th_plain(grid);
    lv_obj_set_size(row, 280, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_style_pad_row(row, 8, 0);
    return row;
}

void th_arc_colors(lv_obj_t *arc, uint32_t indicator, uint32_t track) {
    lv_obj_set_style_arc_color(arc, lv_color_hex(indicator), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(track), LV_PART_MAIN);
}
