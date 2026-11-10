/* SPDX-License-Identifier: MIT
 * Application-owned integration example; replace the placeholder home with the project's own screen. */
#include "setup_flow.h"
LV_FONT_DECLARE(sugarat_wifi_font_20);
LV_FONT_DECLARE(sugarat_wifi_font_16);

/* Navigation handles startup, a new connection and an explicit return from settings. */
static void connected(lv_obj_t *setup, const sugarat_wifi_event_t *event, void *user_data);
static void open_setup(lv_event_t *event) {
    sugarat_wifi_t *wifi = lv_event_get_user_data(event);
    sugarat_wifi_ui_config_t config = sugarat_wifi_ui_default_config();
    config.on_connected = connected; config.user_data = wifi; config.notify_current_connection = false;
    lv_obj_t *screen = sugarat_wifi_ui_create_with_config(wifi, &config);
    if (!screen) return;
    lv_obj_t *home = lv_scr_act(); lv_scr_load(screen); lv_obj_del(home);
}
static lv_obj_t *caption(lv_obj_t *parent, const char *text, int y, bool large) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_width(label, 220); lv_obj_set_pos(label, 10, y);
    lv_obj_set_style_text_font(label, large ? &sugarat_wifi_font_20 : &sugarat_wifi_font_16, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT); lv_label_set_text(label, text);
    return label;
}
static void connected(lv_obj_t *setup, const sugarat_wifi_event_t *event, void *user_data) {
    lv_obj_t *home = lv_obj_create(NULL);
    if (!home) return;
    lv_obj_remove_style_all(home); lv_obj_clear_flag(home, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(home, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(home, lv_color_hex(0xf2f2f7), 0);
    lv_obj_t *safe = lv_obj_create(home); lv_obj_remove_style_all(safe);
    lv_obj_set_pos(safe, 60, 60); lv_obj_set_size(safe, 240, 240); lv_obj_clear_flag(safe, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *badge = lv_obj_create(safe); lv_obj_remove_style_all(badge);
    lv_obj_set_pos(badge, 88, 8); lv_obj_set_size(badge, 64, 64); lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(badge, lv_color_hex(0x007aff), 0);
    static const lv_point_t check[] = {{17,33},{28,44},{48,23}};
    lv_obj_t *line = lv_line_create(badge); lv_line_set_points(line, check, 3);
    lv_obj_set_style_line_width(line, 4, 0); lv_obj_set_style_line_color(line, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    caption(safe, "App ready", 86, true);
    caption(safe, event->ssid, 121, false); caption(safe, event->ip, 147, false);
    lv_obj_t *settings = lv_btn_create(safe); lv_obj_set_pos(settings, 24, 184); lv_obj_set_size(settings, 192, 48);
    lv_obj_set_style_radius(settings, 18, 0); lv_obj_set_style_shadow_width(settings, 0, 0);
    lv_obj_t *label = lv_label_create(settings); lv_label_set_text(label, "Wi-Fi settings"); lv_obj_center(label);
    lv_obj_add_event_cb(settings, open_setup, LV_EVENT_CLICKED, user_data);
    lv_scr_load(home); lv_obj_del(setup);
}
lv_obj_t *sugarat_wifi_example_create(sugarat_wifi_t *wifi) {
    sugarat_wifi_ui_config_t config = sugarat_wifi_ui_default_config();
    config.on_connected = connected; config.user_data = wifi;
    return sugarat_wifi_ui_create_with_config(wifi, &config);
}
