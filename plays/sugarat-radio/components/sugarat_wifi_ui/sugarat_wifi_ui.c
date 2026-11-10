/* SPDX-License-Identifier: MIT
 * LVGL-thread-owned provisioning screens for the 360x360 round display. */
#include "sugarat_wifi_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
LV_FONT_DECLARE(sugarat_wifi_font_16);
LV_FONT_DECLARE(sugarat_wifi_font_20);

typedef enum { PAGE_HOME, PAGE_LIST, PAGE_PASSWORD, PAGE_PORTAL, PAGE_CONNECTION } page_t;
typedef enum { TITLE, DEVICE, PHONE, SCAN, BACK, PASSWORD, CONNECT, SHOW, HIDE, CHOOSE,
    IDLE, SCANNING, CONNECTING, CONNECTED, FAILED, SAVED, NOT_SAVED, BUSY, INVALID,
    OPEN, UNSUPPORTED, HOTSPOT, KEY, URL, WAIT, EMPTY, FORGET, FORGOTTEN, CLOSED,
    DEVICE_HINT, PHONE_HINT, SECURED, KEYS, NEXT, SPACE, DELETE_KEY, CHANGE, CLOSE_FAILED, TEXT_COUNT } text_t;
static const char *words[2][TEXT_COUNT] = {
    {"Wi-Fi setup","On device","Use phone","Scan","Back","Password","Connect","Show","Hide","Choose a network",
     "Choose how to connect","Scanning...","Connecting...","Connected","Connection failed","Saved for restart","Connected; save failed","Please wait...","Check password / selection",
     "Open","Unsupported","Join this hotspot","Password","Open in browser","No hotspot password","No networks found","Forget Wi-Fi","Settings cleared","Hotspot closed",
     "Scan & choose","Connect to hotspot","Secured","Keys","Next","Space","Del","Change network","Hotspot close failed"},
    {"网络设置","设备选网","手机配网","扫描","返回","密码","连接","显示","隐藏","请选择网络",
     "请选择配网方式","正在扫描","正在连接","连接成功","连接失败","重启后自动连接","已连接但保存失败","请稍候","请检查密码或网络",
     "开放网络","暂不支持","请连接以下热点","连接密码","打开网页","热点无需密码","未发现网络","清除网络","已清除配置","热点已关闭",
     "搜索附近网络","连接设备热点","加密网络","键盘","下一组","空格","删除","更换网络","热点关闭失败"}
};
static const char *const keysets[] = { "", "abcdefghijklmnopqrstuvwxyz", "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "0123456789", "!\"#$%&'()*+,-./:;<=>?@[\\]^_\x60{|}~" };
typedef struct {
    sugarat_wifi_t *wifi;
    lv_obj_t *screen;
    lv_timer_t *timer;
    sugarat_wifi_snapshot_t snapshot;
    sugarat_wifi_ui_config_t config;
    uint32_t notified_connection, return_after_sequence;
    uint32_t revision, generation;
    unsigned selected, network_page, keyset, key_page;
    page_t page;
    bool zh, reveal, return_requested;
    char draft[64], selected_ssid[33], error[80];
} ui_t;
static const char *tr(ui_t *u, text_t key) { return words[u->zh ? 1 : 0][key]; }
static void render(ui_t *u);
static bool app_connection_ready(const ui_t *u) {
    const sugarat_wifi_event_t *event = &u->snapshot.last_connected;
    return u->config.on_connected && u->snapshot.phase == SW_CONNECTED && u->snapshot.saved
        && event->id == SUGARAT_WIFI_EVENT_CONNECTED && event->sequence && event->saved && !event->portal;
}
/* The navigation callback may delete u; the event copy stays valid until the callback returns. */
static void enter_app(ui_t *u) {
    sugarat_wifi_event_t event = u->snapshot.last_connected;
    u->notified_connection = event.sequence; u->return_requested = false;
    u->config.on_connected(u->screen, &event, u->config.user_data);
}
static lv_color_t ink(void) { return lv_color_hex(0x1c1c1e); }
static lv_color_t muted(void) { return lv_color_hex(0x6d6d72); }
static void place(lv_obj_t *obj, int x, int y, int width, int height) {
    lv_obj_set_pos(obj, x, y); lv_obj_set_size(obj, width, height);
}
static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int width, bool large, lv_color_t color) {
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_style_text_font(obj, large ? &sugarat_wifi_font_20 : &sugarat_wifi_font_16, 0);
    lv_obj_set_style_text_color(obj, color, 0);
    lv_label_set_text(obj, text); place(obj, x, y, width, large ? sugarat_wifi_font_20.line_height : sugarat_wifi_font_16.line_height);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
    return obj;
}
static void centered_label(lv_obj_t *parent, const char *text, int y, bool large, lv_color_t color) {
    lv_obj_t *obj = label(parent, text, 0, y, 240, large, color);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
}
static lv_obj_t *button_base(lv_obj_t *parent, int x, int y, int width, int height, bool primary) {
    lv_obj_t *obj = lv_btn_create(parent); lv_obj_remove_style_all(obj);
    place(obj, x, y, width, height); lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj, primary ? 24 : 14, 0); lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(primary ? 0x007aff : 0xffffff), 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(primary ? 0x0066d6 : 0xe5e5ea), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(primary ? 0x007aff : 0xffffff), 0);
    lv_obj_set_style_opa(obj, LV_OPA_50, LV_STATE_DISABLED);
    return obj;
}
static void report(ui_t *u, esp_err_t err) {
    snprintf(u->error, sizeof(u->error), "%s", err == ESP_OK ? "" : tr(u, err == ESP_ERR_INVALID_STATE ? BUSY : INVALID));
}
static void command(lv_event_t *event) {
    ui_t *u = lv_event_get_user_data(event);
    const char *name = lv_obj_get_user_data(lv_event_get_target(event));
    u->error[0] = 0;
    if (!strcmp(name, "return_app")) {
        sugarat_wifi_snapshot(u->wifi, &u->snapshot);
        if (!app_connection_ready(u)) { render(u); return; }
        if (u->snapshot.portal || u->snapshot.last_event.id == SUGARAT_WIFI_EVENT_PORTAL_OPENED
            || u->snapshot.last_event.id == SUGARAT_WIFI_EVENT_PORTAL_FAILED) {
            esp_err_t err = sugarat_wifi_portal(u->wifi, false); report(u, err);
            u->return_requested = err == ESP_OK; u->return_after_sequence = u->snapshot.last_event.sequence;
            render(u); return;
        }
        enter_app(u); return;
    }
    if (!strcmp(name, "language")) u->zh = !u->zh;
    else if (!strcmp(name, "device")) { u->page = PAGE_LIST; u->network_page = 0; report(u, sugarat_wifi_scan(u->wifi)); }
    else if (!strcmp(name, "phone")) { u->page = PAGE_PORTAL; report(u, sugarat_wifi_portal(u->wifi, true)); }
    else if (!strcmp(name, "scan")) { u->network_page = 0; report(u, sugarat_wifi_scan(u->wifi)); }
    else if (!strcmp(name, "previous")) { if (u->network_page) u->network_page--; }
    else if (!strcmp(name, "following")) { if ((u->network_page + 1) * 2 < u->snapshot.count) u->network_page++; }
    else if (!strcmp(name, "back")) {
        if (u->page == PAGE_PASSWORD) { u->page = PAGE_LIST; memset(u->draft, 0, sizeof(u->draft)); }
        else u->page = PAGE_HOME;
    }
    else if (!strcmp(name, "show")) u->reveal = !u->reveal;
    else if (!strcmp(name, "delete")) { size_t length = strlen(u->draft); if (length) u->draft[length - 1] = 0; }
    else if (!strcmp(name, "keys")) { u->keyset = 0; u->key_page = 0; }
    else if (!strcmp(name, "next")) { if (u->keyset) u->key_page = (u->key_page + 1) % ((strlen(keysets[u->keyset]) + 5) / 6); }
    else if (!strcmp(name, "space")) { size_t length = strlen(u->draft); if (length < 63) { u->draft[length] = ' '; u->draft[length + 1] = 0; } }
    else if (!strcmp(name, "connect")) {
        esp_err_t err = sugarat_wifi_connect(u->wifi, u->generation, u->selected, u->draft);
        report(u, err);
        if (err == ESP_OK) { memset(u->draft, 0, sizeof(u->draft)); u->page = PAGE_LIST; }
    }
    else if (!strcmp(name, "connection")) u->page = PAGE_CONNECTION;
    else if (!strcmp(name, "change")) u->page = PAGE_HOME;
    else if (!strcmp(name, "forget")) { report(u, sugarat_wifi_forget(u->wifi)); u->page = PAGE_HOME; }
    render(u);
}
typedef enum { ICON_NONE, ICON_BACK, ICON_FORWARD, ICON_DELETE, ICON_KEYBOARD, ICON_EYE, ICON_EYE_OFF, ICON_SPACE, ICON_SCAN, ICON_WIFI, ICON_PHONE_HOTSPOT } icon_t;
static lv_obj_t *stroke(lv_obj_t *parent, const lv_point_t *points, unsigned count, lv_color_t color) {
    lv_obj_t *line = lv_line_create(parent); lv_line_set_points(line, points, count);
    lv_obj_set_style_line_color(line, color, 0); lv_obj_set_style_line_width(line, 2, 0);
    lv_obj_set_style_line_rounded(line, true, 0); lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
    return line;
}
static lv_obj_t *outline(lv_obj_t *parent, int x, int y, int width, int height, int radius, lv_color_t color) {
    lv_obj_t *obj = lv_obj_create(parent); lv_obj_remove_style_all(obj); place(obj, x, y, width, height);
    lv_obj_set_style_radius(obj, radius, 0); lv_obj_set_style_border_width(obj, 2, 0);
    lv_obj_set_style_border_color(obj, color, 0); lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}
static lv_obj_t *icon(lv_obj_t *parent, icon_t kind, lv_color_t color) {
    /* LVGL retains line-point arrays for the lifetime of their objects. */
    static const lv_point_t left[] = {{15,5},{8,12},{15,19}}, right[] = {{9,5},{16,12},{9,19}};
    static const lv_point_t erase[] = {{9,5},{22,5},{22,19},{9,19},{2,12},{9,5}};
    static const lv_point_t cross_a[] = {{12,9},{18,15}}, cross_b[] = {{12,15},{18,9}};
    static const lv_point_t eye[] = {{2,12},{5,7},{9,5},{15,5},{19,7},{22,12},{19,17},{15,19},{9,19},{5,17},{2,12}};
    static const lv_point_t slash[] = {{3,3},{21,21}}, space[] = {{3,9},{3,15},{21,15},{21,9}};
    static const lv_point_t scan[] = {{21,8},{18,4},{12,2},{6,4},{3,9},{3,15},{7,20},{14,22},{20,18}};
    static const lv_point_t arrow[] = {{15,8},{21,8},{21,2}};
    static const lv_point_t wifi_outer[] = {{2,8},{5,5},{9,3},{15,3},{19,5},{22,8}};
    static const lv_point_t wifi_middle[] = {{6,12},{9,10},{15,10},{18,12}}, wifi_inner[] = {{10,16},{12,15},{14,16}};
    static const lv_point_t phone_outer[] = {{18,5},{21,8},{22,12},{21,16},{18,19}}, phone_inner[] = {{16,9},{18,12},{16,15}};
    lv_obj_t *root = lv_obj_create(parent); lv_obj_remove_style_all(root); lv_obj_set_size(root, 24, 24);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    switch (kind) {
        case ICON_BACK: stroke(root, left, 3, color); break;
        case ICON_FORWARD: stroke(root, right, 3, color); break;
        case ICON_DELETE: stroke(root, erase, 6, color); stroke(root, cross_a, 2, color); stroke(root, cross_b, 2, color); break;
        case ICON_KEYBOARD:
            outline(root, 1, 4, 22, 17, 3, color);
            for (unsigned row = 0; row < 2; ++row) for (unsigned col = 0; col < 4; ++col) {
                lv_obj_t *dot = lv_obj_create(root); lv_obj_remove_style_all(dot); place(dot, 4 + col * 4, 8 + row * 4, 2, 2);
                lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(dot, color, 0);
                lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
            }
            {
                lv_obj_t *bar = lv_obj_create(root); lv_obj_remove_style_all(bar); place(bar, 7, 16, 10, 2);
                lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(bar, color, 0);
                lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
            }
            break;
        case ICON_EYE: case ICON_EYE_OFF:
            stroke(root, eye, 11, color); outline(root, 9, 9, 6, 6, LV_RADIUS_CIRCLE, color);
            if (kind == ICON_EYE_OFF) stroke(root, slash, 2, color);
            break;
        case ICON_SPACE: stroke(root, space, 4, color); break;
        case ICON_SCAN: stroke(root, scan, 9, color); stroke(root, arrow, 3, color); break;
        case ICON_WIFI:
            stroke(root, wifi_outer, 6, color); stroke(root, wifi_middle, 4, color); stroke(root, wifi_inner, 3, color);
            {
                lv_obj_t *dot = lv_obj_create(root); lv_obj_remove_style_all(dot); place(dot, 10, 19, 4, 4);
                lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(dot, color, 0); lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
                lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
            }
            break;
        case ICON_PHONE_HOTSPOT:
            outline(root, 2, 2, 12, 20, 3, color); stroke(root, phone_outer, 5, color); stroke(root, phone_inner, 3, color);
            break;
        case ICON_NONE: break;
    }
    return root;
}
static icon_t action_icon(ui_t *u, const char *name) {
    if (!strcmp(name, "back") || !strcmp(name, "previous") || !strcmp(name, "return_app")) return ICON_BACK;
    if (!strcmp(name, "following") || !strcmp(name, "next")) return ICON_FORWARD;
    if (!strcmp(name, "delete")) return ICON_DELETE;
    if (!strcmp(name, "keys")) return ICON_KEYBOARD;
    if (!strcmp(name, "show")) return u->reveal ? ICON_EYE_OFF : ICON_EYE;
    if (!strcmp(name, "space")) return ICON_SPACE;
    if (!strcmp(name, "scan")) return ICON_SCAN;
    return ICON_NONE;
}
static lv_obj_t *button(ui_t *u, lv_obj_t *parent, const char *text, const char *name, int x, int y, int width, int height, bool primary) {
    lv_obj_t *obj = button_base(parent, x, y, width, height, primary);
    lv_obj_set_user_data(obj, (void *)name); lv_obj_add_event_cb(obj, command, LV_EVENT_CLICKED, u);
    icon_t kind = action_icon(u, name);
    if (kind != ICON_NONE) {
        lv_color_t color = kind == ICON_BACK || kind == ICON_FORWARD || kind == ICON_SCAN ? lv_color_hex(0x007aff) : ink();
        lv_obj_t *glyph = icon(obj, kind, color);
        lv_obj_align(glyph, LV_ALIGN_CENTER, kind == ICON_SCAN ? -28 : 0, 0);
        if (kind == ICON_SCAN) {
            lv_obj_t *caption = label(obj, text, 0, 0, 54, true, ink());
            lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0); lv_obj_align(caption, LV_ALIGN_CENTER, 16, 0);
        }
    } else {
        lv_obj_t *caption = label(obj, text, 4, 0, width - 8, true, primary ? lv_color_white() : ink());
        lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0); lv_obj_center(caption);
    }
    return obj;
}
static void header(ui_t *u, lv_obj_t *parent, const char *title, bool home, bool password) {
    if (!home) {
        lv_obj_t *back = button(u, parent, "<", "back", 0, 0, 48, 48, false);
        lv_obj_set_style_radius(back, 24, 0);
    }
    int title_x = home && app_connection_ready(u) ? 54 : 0;
    if (title_x) button(u, parent, "<", "return_app", 0, 0, 48, 48, false);
    if (home && u->snapshot.phase == SW_CONNECTED) button(u, parent, tr(u, CONNECTED), "connection", title_x, 0, 170 - title_x, 48, false);
    else {
        lv_obj_t *caption = label(parent, title, home ? title_x : 64, (48 - sugarat_wifi_font_20.line_height) / 2, home ? 170 - title_x : 112, true, ink());
        if (!home) lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
    }
    lv_obj_t *action = button(u, parent, password ? tr(u, DELETE_KEY) : u->zh ? "EN" : "中文", password ? "delete" : "language", 176, 0, 64, 48, false);
    lv_obj_set_style_radius(action, 24, 0);
    if (!password) lv_obj_set_style_text_color(lv_obj_get_child(action, 0), lv_color_hex(0x007aff), 0);
}
static const char *status_text(ui_t *u) {
    if (u->error[0]) return u->error;
    switch (u->snapshot.phase) {
        case SW_SCANNING: return tr(u, SCANNING);
        case SW_CONNECTING: return tr(u, CONNECTING);
        case SW_CONNECTED: return tr(u, u->snapshot.saved ? CONNECTED : NOT_SAVED);
        case SW_FAILED: return tr(u, u->snapshot.last_event.id == SUGARAT_WIFI_EVENT_PORTAL_FAILED ? CLOSE_FAILED : FAILED);
        default: return tr(u, IDLE);
    }
}
static lv_color_t status_color(ui_t *u) {
    return u->error[0] || u->snapshot.phase == SW_FAILED ? lv_color_hex(0xa05a00) : muted();
}
static void method(ui_t *u, lv_obj_t *parent, text_t title, text_t hint, const char *name, int y, bool primary) {
    lv_obj_t *obj = button_base(parent, 0, y, 240, 68, false);
    lv_obj_set_style_radius(obj, 18, 0);
    lv_obj_set_user_data(obj, (void *)name); lv_obj_add_event_cb(obj, command, LV_EVENT_CLICKED, u);
    lv_obj_t *tile = lv_obj_create(obj); lv_obj_remove_style_all(tile); place(tile, 10, 12, 44, 44);
    lv_obj_set_style_radius(tile, 13, 0); lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(tile, lv_color_hex(0xeaf2ff), 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *glyph = icon(tile, primary ? ICON_WIFI : ICON_PHONE_HOTSPOT, lv_color_hex(0x007aff)); lv_obj_center(glyph);
    int top = (68 - sugarat_wifi_font_20.line_height - sugarat_wifi_font_16.line_height - 2) / 2;
    label(obj, tr(u, title), 66, top, 162, true, ink());
    label(obj, tr(u, hint), 66, top + sugarat_wifi_font_20.line_height + 2, 162, false, muted());
}
static void choose(lv_event_t *event) {
    ui_t *u = lv_event_get_user_data(event);
    unsigned index = (unsigned)(uintptr_t)lv_obj_get_user_data(lv_event_get_target(event));
    if (index >= u->snapshot.count || !u->snapshot.networks[index].supported) return;
    u->error[0] = 0; u->selected = index; u->generation = u->snapshot.generation;
    snprintf(u->selected_ssid, sizeof(u->selected_ssid), "%s", u->snapshot.networks[index].ssid);
    if (!u->snapshot.networks[index].secured) { report(u, sugarat_wifi_connect(u->wifi, u->generation, index, "")); render(u); return; }
    memset(u->draft, 0, sizeof(u->draft)); u->keyset = 0; u->key_page = 0;
    u->page = PAGE_PASSWORD; u->reveal = false; render(u);
}
static void key(lv_event_t *event) {
    ui_t *u = lv_event_get_user_data(event);
    unsigned value = (unsigned)(uintptr_t)lv_obj_get_user_data(lv_event_get_target(event));
    if (value >= 256) { u->keyset = value - 255; u->key_page = 0; }
    else { size_t length = strlen(u->draft); if (length < 63) { u->draft[length] = (char)value; u->draft[length + 1] = 0; } }
    render(u);
}
static void keyboard(ui_t *u, lv_obj_t *parent) {
    header(u, parent, u->selected_ssid, false, true);
    lv_obj_t *field = lv_obj_create(parent); lv_obj_remove_style_all(field); place(field, 0, 50, 240, 30);
    lv_obj_set_style_radius(field, 10, 0); lv_obj_set_style_bg_opa(field, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(field, lv_color_white(), 0);
    char value[70], count[16]; size_t length = strlen(u->draft), shown = length > 14 ? 14 : length;
    if (!length) snprintf(value, sizeof(value), "%s", tr(u, PASSWORD));
    else if (u->reveal) snprintf(value, sizeof(value), "%s", u->draft + length - shown);
    else { memset(value, '*', shown); value[shown] = 0; }
    label(parent, u->error[0] ? u->error : value, 10, 50 + (30 - sugarat_wifi_font_20.line_height) / 2, u->error[0] ? 220 : 176, !u->error[0], u->error[0] ? status_color(u) : length ? ink() : muted());
    if (!u->error[0]) { snprintf(count, sizeof(count), "%u/63", (unsigned)length); label(parent, count, 190, 50 + (30 - sugarat_wifi_font_16.line_height) / 2, 46, false, muted()); }
    for (unsigned i = 0; i < 6; ++i) {
        int x = (i % 3) * 82, y = 84 + (i / 3) * 54;
        if (!u->keyset) {
            if (i < 4) {
                const char *modes[] = { "abc", "ABC", "123", "#+=" };
                lv_obj_t *obj = button_base(parent, x, y, 76, 48, false);
                lv_obj_t *text = label(obj, modes[i], 0, 0, 76, true, ink()); lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0); lv_obj_center(text);
                lv_obj_set_user_data(obj, (void *)(uintptr_t)(256 + i)); lv_obj_add_event_cb(obj, key, LV_EVENT_CLICKED, u);
            } else button(u, parent, i == 4 ? tr(u, SPACE) : tr(u, u->reveal ? HIDE : SHOW), i == 4 ? "space" : "show", x, y, 76, 48, false);
        } else {
            unsigned at = u->key_page * 6 + i;
            const char *set = keysets[u->keyset];
            char text[2] = { at < strlen(set) ? set[at] : 0, 0 };
            lv_obj_t *obj = button_base(parent, x, y, 76, 48, false);
            lv_obj_t *caption = label(obj, text, 0, 0, 76, true, ink()); lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0); lv_obj_center(caption);
            if (text[0]) { lv_obj_set_user_data(obj, (void *)(uintptr_t)(unsigned char)text[0]); lv_obj_add_event_cb(obj, key, LV_EVENT_CLICKED, u); }
            else lv_obj_add_state(obj, LV_STATE_DISABLED);
        }
    }
    button(u, parent, tr(u, KEYS), "keys", 0, 192, 72, 48, false);
    lv_obj_t *next = button(u, parent, tr(u, NEXT), "next", 76, 192, 72, 48, false);
    if (!u->keyset) lv_obj_add_state(next, LV_STATE_DISABLED);
    lv_obj_t *join = button(u, parent, tr(u, CONNECT), "connect", 152, 192, 88, 48, true);
    if (length < 8 || u->snapshot.phase == SW_SCANNING || u->snapshot.phase == SW_CONNECTING) lv_obj_add_state(join, LV_STATE_DISABLED);
}
static void render(ui_t *u) {
    lv_obj_clean(u->screen);
    lv_obj_t *safe = lv_obj_create(u->screen); lv_obj_remove_style_all(safe); place(safe, 60, 60, 240, 240); lv_obj_clear_flag(safe, LV_OBJ_FLAG_SCROLLABLE);
    if (u->page == PAGE_PASSWORD) { keyboard(u, safe); return; }
    header(u, safe, tr(u, TITLE), u->page == PAGE_HOME, false);
    if (u->page == PAGE_HOME) {
        label(safe, status_text(u), 2, 57, 236, false, status_color(u));
        method(u, safe, DEVICE, DEVICE_HINT, "device", 92, true);
        method(u, safe, PHONE, PHONE_HINT, "phone", 168, false);
    } else if (u->page == PAGE_LIST) {
        unsigned pages = (u->snapshot.count + 1) / 2;
        if (u->network_page >= pages) u->network_page = 0;
        label(safe, u->snapshot.phase == SW_IDLE ? tr(u, CHOOSE) : status_text(u), 2, 50, 236, false, status_color(u));
        if (!u->snapshot.count) label(safe, tr(u, EMPTY), 2, 108, 236, true, muted());
        for (unsigned row = 0; row < 2; ++row) {
            unsigned i = u->network_page * 2 + row;
            if (i >= u->snapshot.count) break;
            const sugarat_wifi_network_t *n = &u->snapshot.networks[i];
            lv_obj_t *obj = button_base(safe, 0, 72 + row * 60, 240, 54, false);
            label(obj, n->ssid, 12, 3, 216, true, ink());
            char detail[70]; snprintf(detail, sizeof(detail), "%s  %d dBm", tr(u, !n->supported ? UNSUPPORTED : n->secured ? SECURED : OPEN), n->rssi);
            label(obj, detail, 12, 31, 216, false, muted());
            lv_obj_set_user_data(obj, (void *)(uintptr_t)i); lv_obj_add_event_cb(obj, choose, LV_EVENT_CLICKED, u);
            if (!n->supported || u->snapshot.phase == SW_CONNECTING || u->snapshot.phase == SW_SCANNING) lv_obj_add_state(obj, LV_STATE_DISABLED);
        }
        lv_obj_t *previous = button(u, safe, "<", "previous", 0, 192, 52, 48, false);
        lv_obj_t *following = button(u, safe, ">", "following", 188, 192, 52, 48, false);
        char scan[40]; snprintf(scan, sizeof(scan), "%u/%u", u->network_page + 1, pages ? pages : 1);
        lv_obj_t *scan_button = button(u, safe, scan, "scan", 58, 192, 124, 48, false);
        if (!u->network_page) lv_obj_add_state(previous, LV_STATE_DISABLED);
        if (u->network_page + 1 >= pages) lv_obj_add_state(following, LV_STATE_DISABLED);
        if (u->snapshot.phase == SW_SCANNING || u->snapshot.phase == SW_CONNECTING) lv_obj_add_state(scan_button, LV_STATE_DISABLED);
    } else if (u->page == PAGE_PORTAL) {
        if (u->snapshot.portal) {
            centered_label(safe, tr(u, HOTSPOT), 64, false, muted());
            centered_label(safe, u->snapshot.ap_ssid, 92, true, ink());
            centered_label(safe, tr(u, WAIT), 126, false, muted());
            lv_obj_t *divider = lv_obj_create(safe); lv_obj_remove_style_all(divider); place(divider, 40, 162, 160, 1);
            lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(divider, lv_color_hex(0xe5e5ea), 0);
            lv_obj_clear_flag(divider, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
            centered_label(safe, tr(u, URL), 178, false, muted());
            centered_label(safe, "http://192.168.4.1", 206, true, lv_color_hex(0x007aff));
        } else {
            centered_label(safe, u->snapshot.phase == SW_CONNECTED ? status_text(u) : tr(u, CLOSED), 80, true, ink());
            button(u, safe, tr(u, PHONE), "phone", 0, 168, 240, 68, true);
        }
    } else {
        centered_label(safe, status_text(u), 57, false, status_color(u));
        centered_label(safe, u->snapshot.ssid, 82, true, ink());
        centered_label(safe, u->snapshot.ip, 112, false, muted());
        button(u, safe, tr(u, CHANGE), "change", 0, 136, 240, 48, true);
        button(u, safe, tr(u, FORGET), "forget", 0, 192, 240, 48, false);
    }
}
static void tick(lv_timer_t *timer) {
    ui_t *u = timer->user_data; sugarat_wifi_snapshot_t state; sugarat_wifi_snapshot(u->wifi, &state);
    bool changed = state.revision != u->revision;
    if (!changed && !u->return_requested) return;
    u->revision = state.revision; u->snapshot = state;
    bool close_result = state.last_event.sequence != u->return_after_sequence;
    if (u->return_requested && (!app_connection_ready(u)
        || (close_result && state.last_event.id == SUGARAT_WIFI_EVENT_PORTAL_FAILED))) u->return_requested = false;
    /* portal=false precedes synchronous HTTP/DNS teardown; only PORTAL_CLOSED confirms completion. */
    bool explicit_return = u->return_requested && close_result && state.last_event.id == SUGARAT_WIFI_EVENT_PORTAL_CLOSED;
    /* Keep the selected scan generation and in-progress password independent of radio updates. */
    if (changed && u->page != PAGE_PASSWORD) render(u);
    if (app_connection_ready(u) && !state.portal
        && (explicit_return || state.last_connected.sequence != u->notified_connection)) {
        enter_app(u);
    }
}
static void deleted(lv_event_t *event) {
    ui_t *u = lv_event_get_user_data(event); if (u->timer) lv_timer_del(u->timer);
    memset(u->draft, 0, sizeof(u->draft)); memset(&u->snapshot, 0, sizeof(u->snapshot)); free(u);
}
sugarat_wifi_ui_config_t sugarat_wifi_ui_default_config(void) {
    return (sugarat_wifi_ui_config_t){ .notify_current_connection = true };
}
lv_obj_t *sugarat_wifi_ui_create_with_config(sugarat_wifi_t *wifi, const sugarat_wifi_ui_config_t *config) {
    if (!wifi || !config) return NULL;
    ui_t *u = calloc(1, sizeof(*u)); if (!u) return NULL;
    u->wifi = wifi; u->revision = UINT32_MAX; u->config = *config;
    u->screen = lv_obj_create(NULL);
    if (!u->screen) { free(u); return NULL; }
    lv_obj_remove_style_all(u->screen); lv_obj_clear_flag(u->screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(u->screen, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(u->screen, lv_color_hex(0xf2f2f7), 0);
    sugarat_wifi_snapshot(wifi, &u->snapshot);
    if (!config->notify_current_connection) u->notified_connection = u->snapshot.last_connected.sequence;
    render(u);
    u->timer = lv_timer_create(tick, 250, u);
    if (!u->timer) { lv_obj_del(u->screen); free(u); return NULL; }
    lv_obj_add_event_cb(u->screen, deleted, LV_EVENT_DELETE, u);
    return u->screen;
}
lv_obj_t *sugarat_wifi_ui_create(sugarat_wifi_t *wifi) {
    sugarat_wifi_ui_config_t config = sugarat_wifi_ui_default_config();
    return sugarat_wifi_ui_create_with_config(wifi, &config);
}
