/* Modified for Sugarat Radio by Deotaland, 2026. */
/*
 * sdgoods-radio · 页面层实现
 *
 * 一条硬规则：**任何会让当前页面被销毁的动作都走 lv_async_call**。
 * 直接在点按回调里删掉自己所在的整棵树，LVGL 的事件派发还在用这些对象，
 * 属于「能跑但迟早出鬼」的写法（旧版设置浮层只是隐藏，所以没暴露这个问题）。
 */

#include "ui_radio_pages.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "radio_catalog.h"
#include "sdgoods_lcd.h"
#include "ui_radio_kit.h"
#include "ui_radio_skin.h"
#include "sdgoods_i18n.h"
#include "sdgoods_cc.h"
#include "wifi_prov.h"
#include "radio_region.h"


#define LIST_SHOW_MAX   3
#define WIFI_MAX_NETS   12

static const radio_pages_ctl_t *s_ctl;

static rk_page_t s_list;
static rk_page_t s_set;
static rk_page_t s_pick;
static lv_obj_t *s_skin_page, *s_skin_art, *s_skin_title, *s_skin_name, *s_skin_note;
static int s_skin_candidate;
static sdgoods_tap_ctx_t s_skin_ctx[4];
static sdgoods_tap_ctx_t s_list_nav_ctx[2];
static int s_list_offset;
static bool s_list_owned, s_list_queued;
static int s_list_direction;
static bool s_list_loading;
static lv_obj_t *s_wifi_value;
static wifi_prov_state_t s_wifi_displayed;

/* 位移守卫上下文：由调用方静态存储，生命周期覆盖对应对象 */
static sdgoods_tap_ctx_t s_list_ctx[LIST_SHOW_MAX];
static sdgoods_tap_ctx_t s_set_ctx[8];
static sdgoods_tap_ctx_t s_pick_ctx[20];
static sdgoods_tap_ctx_t s_head_ctx[4];

/* Selection callback stays valid until the picker is detached. */
static void (*s_pick_cb)(int idx);

static void list_open_at(lv_obj_t *parent, int offset);

void radio_pages_init(const radio_pages_ctl_t *ctl)
{
    s_ctl = ctl;
}

void radio_pages_detach(void)
{
    if (s_skin_page) lv_obj_del(s_skin_page);
    s_skin_page = s_skin_art = s_skin_title = s_skin_name = s_skin_note = NULL;
    rk_page_close(&s_pick);
    rk_page_close(&s_set);
    s_wifi_value=NULL;
    rk_page_close(&s_list);
    s_pick_cb = NULL;
}

void radio_pages_close_all(void)
{
    radio_pages_detach();
}

bool radio_pages_any_open(void)
{
    return s_skin_page || rk_page_is_open(&s_list) || rk_page_is_open(&s_set) ||
           rk_page_is_open(&s_pick);
}

/* ------------------------------------------------------------------ 异步 */
static void list_apply(void *arg)
{
    int idx = (int)(intptr_t)arg;
    rk_page_close(&s_list);
    if (s_ctl && s_ctl->play_index) {
        s_ctl->play_index(idx);
    }
}

static void list_row_cb(void *ud)
{
    lv_async_call(list_apply, ud);
}

/* ------------------------------------------------------------------ 列表 */
static void list_settings_async(void *ud)
{
    (void)ud;
    radio_pages_close_all();
    if (s_ctl) {
        lv_obj_t *parent = lv_scr_act();
        radio_pages_open_settings(parent);
    }
}

static void on_list_settings(void *ud)
{
    (void)ud;
    lv_async_call(list_settings_async, NULL);
}

static void list_move_async(void *ud)
{
    if (!rk_page_is_open(&s_list)) return;
    int count = s_ctl && s_ctl->count ? s_ctl->count() : 0;
    int offset = s_list_offset + (int)(intptr_t)ud * LIST_SHOW_MAX;
    if (offset >= 0 && offset < count) list_open_at(lv_scr_act(), offset);
}

static void list_move(void *ud) { lv_async_call(list_move_async, ud); }
static void back_to_list_async(void *ud){(void)ud;list_open_at(lv_scr_act(),s_list_offset);}
static void back_to_list(void *ud){(void)ud;lv_async_call(back_to_list_async,NULL);}

static void list_gesture_apply(void *arg)
{
    s_list_queued=false;
    if(!rk_page_is_open(&s_list))return;
    if((int)(intptr_t)arg==2)rk_page_close(&s_list);
    else list_move_async(arg);
}

static void list_gesture(lv_event_t *e)
{
    lv_event_code_t code=lv_event_get_code(e);
    if(code==LV_EVENT_PRESSED){s_list_owned=true;s_list_direction=0;}
    else if(code==LV_EVENT_GESTURE && s_list_owned){
        lv_indev_t *input=lv_event_get_param(e);
        if(!input)return;
        lv_dir_t dir=lv_indev_get_gesture_dir(input);
        s_list_direction=dir==LV_DIR_LEFT?1:dir==LV_DIR_RIGHT?-1:dir==LV_DIR_TOP?2:0;
    }else if(code==LV_EVENT_RELEASED){
        if(s_list_owned && s_list_direction && !s_list_queued){
            s_list_queued=true;
            lv_async_call(list_gesture_apply,(void *)(intptr_t)s_list_direction);
        }
        s_list_owned=false;
    }else if(code==LV_EVENT_PRESS_LOST)s_list_owned=false;
}

static void list_bubble(lv_obj_t *o, lv_obj_t *root)
{
    /* Leave the platform's top capture strip in sole ownership of its press. */
    if(o!=root && lv_obj_get_y(o)==0 && lv_obj_get_height(o)==70 && lv_obj_get_width(o)==360)return;
    if(o!=root)lv_obj_add_flag(o,LV_OBJ_FLAG_EVENT_BUBBLE|LV_OBJ_FLAG_GESTURE_BUBBLE);
    for(uint32_t i=0;i<lv_obj_get_child_cnt(o);++i)list_bubble(lv_obj_get_child(o,(int)i),root);
}

void radio_pages_open_list(lv_obj_t *parent)
{
    int current = s_ctl && s_ctl->cur_index ? s_ctl->cur_index() : 0;
    list_open_at(parent, current > 0 ? current / LIST_SHOW_MAX * LIST_SHOW_MAX : 0);
}

static void list_open_at(lv_obj_t *parent, int offset)
{
    if (!parent) {
        return;
    }
    radio_pages_detach();
    rk_page_open(&s_list, parent, SDG_T("电台列表","Stations"));
    rk_page_action(&s_list, SDG_T("设置","Settings"), &s_head_ctx[0], on_list_settings, NULL);
    lv_obj_clear_flag(s_list.root,LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_clear_flag(s_list.body,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_list.root,list_gesture,LV_EVENT_ALL,NULL);
    s_list_owned=s_list_queued=false;

    int n = s_ctl && s_ctl->count ? s_ctl->count() : 0;
    s_list_loading=s_ctl && s_ctl->loading && s_ctl->loading();
    if (n <= 0) {
        rk_page_hint(&s_list,s_ctl && s_ctl->loading && s_ctl->loading()?SDG_T("正在获取电台","Loading stations"):
                     wifi_prov_get_state()==WIFI_STATE_CONNECTED?SDG_T("还没有电台\n请刷新电台","No stations. Refresh to retry."):
                     SDG_T("连上 WiFi 后会自动获取","Connect Wi-Fi to load stations"));
        list_bubble(s_list.root,s_list.root);
        return;
    }
    s_list_offset = offset >= 0 && offset < n ? offset : 0;
    lv_obj_set_height(s_list.body, 144);

    static char names[LIST_SHOW_MAX][RADIO_NAME_MAX + 4];
    static char metas[LIST_SHOW_MAX][32];
    for (int row = 0; row < LIST_SHOW_MAX && s_list_offset + row < n; row++) {
        int i = s_list_offset + row;
        const radio_station_t *st = radio_catalog_at(i);
        if (!st) {
            break;
        }
        rk_text_safe(st->name, names[row], sizeof(names[row]), RK_FONT_TITLE);
        if (names[row][0] == '\0') {
            snprintf(names[row], sizeof(names[row]), SDG_T("电台 %d", "Station %d"), i + 1);
        }
        if(st->freq_mhz>0.0f){
            snprintf(metas[row],sizeof(metas[row]),"FM %.1f MHz",st->freq_mhz);
        }else if (st->bitrate > 0) {
            snprintf(metas[row], sizeof(metas[row]), "%uk", (unsigned)st->bitrate);
        } else {
            metas[row][0] = '\0';
        }
        rk_page_row_net(&s_list, names[row], metas[row], &s_list_ctx[row], list_row_cb,
                        (void *)(intptr_t)i);
    }
    const rk_theme_t *th = rk_theme();
    lv_obj_t *prev = rk_round_btn(s_list.root, 76, 276, 44, RK_ICON_BACK, th->card, th->ink);
    sdgoods_tap_bind(prev, &s_list_nav_ctx[0], list_move, (void *)(intptr_t)-1);
    if (s_list_offset == 0) lv_obj_add_state(prev, LV_STATE_DISABLED);
    lv_obj_t *next = rk_round_btn(s_list.root, 240, 276, 44, RK_ICON_CHEVRON, th->card, th->ink);
    sdgoods_tap_bind(next, &s_list_nav_ctx[1], list_move, (void *)(intptr_t)1);
    if (s_list_offset + LIST_SHOW_MAX >= n) lv_obj_add_state(next, LV_STATE_DISABLED);
    lv_obj_t *page = lv_label_create(s_list.root);
    char page_text[24];
    snprintf(page_text, sizeof(page_text), "%d / %d", s_list_offset / LIST_SHOW_MAX + 1, (n + LIST_SHOW_MAX - 1) / LIST_SHOW_MAX);
    lv_label_set_text(page, page_text);
    lv_obj_set_style_text_font(page, RK_FONT_SMALL, 0);
    lv_obj_set_style_text_color(page, th->muted, 0);
    lv_obj_set_style_text_align(page, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(page, 120, 288);
    lv_obj_set_size(page, 120, 22);
    lv_obj_t *handle=rk_box(s_list.root,166,336,28,3);
    lv_obj_set_style_bg_color(handle,th->muted,0);
    lv_obj_set_style_bg_opa(handle,LV_OPA_50,0);
    lv_obj_set_style_radius(handle,2,0);
    list_bubble(s_list.root,s_list.root);
}

/* ------------------------------------------------------------------ 选择器 */
static void pick_apply(void *arg)
{
    int idx = (int)(intptr_t)arg;
    void (*cb)(int) = s_pick_cb;
    s_pick_cb = NULL;
    rk_page_close(&s_pick);
    if (cb) {
        cb(idx);
    }
}

static void pick_row_cb(void *ud)
{
    lv_async_call(pick_apply, ud);
}

static void picker_open(lv_obj_t *parent, const char *title, const char *const *labels,
                        int n, int selected, void (*cb)(int))
{
    if (!parent || n <= 0 || n > 20) {
        return;
    }
    rk_page_open(&s_pick, parent, title);
    s_pick_cb = cb;
    for (int i = 0; i < n; i++) {
        static char marks[20][16];
        if (i == selected) {
            snprintf(marks[i], sizeof(marks[i]), "Selected");
        } else {
            marks[i][0] = '\0';
        }
        rk_page_row(&s_pick, labels[i], marks[i], &s_pick_ctx[i], pick_row_cb,
                    (void *)(intptr_t)i);
    }

}

/* Explicit US markets; no IP geolocation request is made. */
static const char *CITY_LABELS[20];
static void city_picked(int idx) {
    const radio_region_t *region = idx >= 0 ? radio_region_at((size_t)idx) : NULL;
    if (region && s_ctl && s_ctl->set_city) s_ctl->set_city(region->id);
}
static void open_city_picker(lv_obj_t *parent) {
    int selected = 0;
    for (size_t i = 0; i < radio_region_count(); ++i) {
        CITY_LABELS[i] = radio_region_at(i)->label;
        if (s_ctl && s_ctl->city && !strcmp(s_ctl->city(), CITY_LABELS[i])) selected = (int)i;
    }
    picker_open(parent, SDG_T("地区", "US region"), CITY_LABELS,
                (int)radio_region_count(), selected, city_picked);
}

/* 定时 */
static const char *const SLEEP_LABELS[] = {
    "Off", "15 min", "30 min", "45 min", "60 min", "90 min",
};
static const int SLEEP_STEPS[] = {0, 15, 30, 45, 60, 90};
#define SLEEP_N ((int)(sizeof(SLEEP_STEPS) / sizeof(SLEEP_STEPS[0])))

static int sleep_index(void)
{
    int cur = (s_ctl && s_ctl->sleep_min) ? s_ctl->sleep_min() : 0;
    for (int i = 0; i < SLEEP_N; i++) {
        if (SLEEP_STEPS[i] == cur) {
            return i;
        }
    }
    return 0;
}

static void sleep_picked(int idx)
{
    if (!s_ctl || !s_ctl->set_sleep || idx < 0 || idx >= SLEEP_N) {
        return;
    }
    s_ctl->set_sleep(SLEEP_STEPS[idx]);
}

static void open_sleep_picker(lv_obj_t *parent)
{
    picker_open(parent, SDG_T("定时休眠","Sleep timer"), SLEEP_LABELS, SLEEP_N, sleep_index(), sleep_picked);
}

/* ------------------------------------------------------------------ 设置 */
static void set_async_open_city(void *a)
{
    (void)a;
    open_city_picker(lv_scr_act());
}

static void set_async_open_sleep(void *a)
{
    (void)a;
    open_sleep_picker(lv_scr_act());
}

static lv_obj_t *skin_label(lv_obj_t *parent, int x, int y, int w, const char *text)
{
    lv_obj_t *o = lv_label_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, 24);
    lv_obj_set_style_text_font(o, RK_FONT_BODY, 0);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(o, LV_LABEL_LONG_DOT);
    lv_label_set_text(o, text);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

static void skin_render(void)
{
    if (!s_skin_page) return;
    const radio_skin_t *s = radio_skin_get(s_skin_candidate);
    lv_obj_set_style_bg_color(s_skin_page, lv_color_hex(s->bg), 0);
    lv_obj_clean(s_skin_art);
    radio_skin_preview(s_skin_art, s_skin_candidate);
    char title[40];
    snprintf(title, sizeof(title), SDG_T("外观 %d / %d", "Style %d / %d"), s_skin_candidate + 1, RADIO_SKIN_COUNT);
    lv_label_set_text(s_skin_title, title);
    lv_label_set_text(s_skin_name, radio_skin_name(s_skin_candidate));
    lv_obj_set_style_text_color(s_skin_title, lv_color_hex(s->ink), 0);
    lv_obj_set_style_text_color(s_skin_name, lv_color_hex(s->ink), 0);
    lv_obj_set_style_text_color(s_skin_note, lv_color_hex(s->muted), 0);
    lv_label_set_text(s_skin_note, s_ctl && s_ctl->skin && s_ctl->skin() == s_skin_candidate
                      ? SDG_T("当前风格", "Current style") : SDG_T("点应用后保存", "Apply to save"));
}

static void skin_move_async(void *arg)
{
    if (!s_skin_page) return;
    s_skin_candidate = (s_skin_candidate + (int)(intptr_t)arg + RADIO_SKIN_COUNT) % RADIO_SKIN_COUNT;
    skin_render();
}

static void skin_move(void *ud) { lv_async_call(skin_move_async, ud); }
static void skin_cancel_async(void *ud) { (void)ud; radio_pages_open_settings(lv_scr_act()); }
static void skin_cancel(void *ud) { (void)ud; lv_async_call(skin_cancel_async, NULL); }

static void skin_apply_async(void *ud)
{
    (void)ud;
    if (!s_skin_page || !s_ctl || !s_ctl->set_skin) return;
    int candidate = s_skin_candidate;
    if (!s_ctl->set_skin(candidate)) {
        radio_pages_detach();
        rk_page_open(&s_set, lv_scr_act(), SDG_T("保存失败", "Save failed"));
        rk_page_hint(&s_set, SDG_T("保存失败，请重试", "Save failed. Try again."));
    }
}

static void skin_apply(void *ud) { (void)ud; lv_async_call(skin_apply_async, NULL); }

static void set_async_theme(void *a)
{
    (void)a;
    radio_pages_detach();
    s_skin_candidate = s_ctl && s_ctl->skin ? s_ctl->skin() : RADIO_SKIN_DEFAULT;
    s_skin_page = rk_box(lv_scr_act(), 0, 0, 360, 360);
    lv_obj_set_style_bg_opa(s_skin_page, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_skin_page, LV_OBJ_FLAG_CLICKABLE);
    s_skin_art = rk_box(s_skin_page, 0, 0, 360, 360);
    lv_obj_set_y(s_skin_art,16);
    s_skin_title = skin_label(s_skin_page, 108, 86, 136, "");
    lv_obj_t *back = rk_round_btn(s_skin_page,60,74,40,RK_ICON_BACK,
                                lv_color_hex(0x30343B),lv_color_white());
    sdgoods_tap_bind(back, &s_skin_ctx[0], skin_cancel, NULL);
    s_skin_name = skin_label(s_skin_page, 60, 242, 240, "");
    lv_obj_set_style_text_font(s_skin_name, RK_FONT_TITLE, 0);
    s_skin_note = skin_label(s_skin_page, 60, 230, 240, "");
    lv_obj_set_height(s_skin_note, 18);
    lv_obj_add_flag(s_skin_note, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(s_skin_note, RK_FONT_SMALL, 0);
    lv_obj_t *prev = rk_round_btn(s_skin_page, 76, 276, 44, RK_ICON_BACK,
                                 lv_color_hex(0x30343B), lv_color_white());
    sdgoods_tap_bind(prev, &s_skin_ctx[1], skin_move, (void *)(intptr_t)-1);
    lv_obj_t *apply = rk_pill(s_skin_page, 132, 276, 96, 44, SDG_T("应用", "Apply"),
                              lv_color_hex(0x30343B), lv_color_white());
    sdgoods_tap_bind(apply, &s_skin_ctx[2], skin_apply, NULL);
    lv_obj_t *next = rk_round_btn(s_skin_page, 240, 276, 44, RK_ICON_CHEVRON,
                                 lv_color_hex(0x30343B), lv_color_white());
    sdgoods_tap_bind(next, &s_skin_ctx[3], skin_move, (void *)(intptr_t)1);
    skin_render();
    sdgoods_cc_bind(s_skin_page);
}

static void set_async_refresh(void *a)
{
    (void)a;
    radio_pages_detach();
    if (s_ctl && s_ctl->refresh) {
        s_ctl->refresh();
    }
}

static void set_async_wifi(void *a)
{
    (void)a;
    radio_pages_detach();
    radio_pages_open_wifi_page(lv_scr_act());
}

static void set_city_cb(void *ud)    { (void)ud; lv_async_call(set_async_open_city, NULL); }
static void set_sleep_cb(void *ud)   { (void)ud; lv_async_call(set_async_open_sleep, NULL); }
static void set_theme_cb(void *ud)   { (void)ud; lv_async_call(set_async_theme, NULL); }
static void set_refresh_cb(void *ud) { (void)ud; lv_async_call(set_async_refresh, NULL); }
static void set_wifi_cb(void *ud)    { (void)ud; lv_async_call(set_async_wifi, NULL); }

static const char *wifi_state_label(wifi_prov_state_t state)
{
    switch(state){
    case WIFI_STATE_CONNECTED:return SDG_T("已连接","Connected");
    case WIFI_STATE_CONNECTING:return SDG_T("连接中","Connecting");
    case WIFI_STATE_SETUP:return SDG_T("待配网","Setup");
    default:return SDG_T("连接失败","Failed");
    }
}

void radio_pages_open_settings(lv_obj_t *parent)
{
    if (!parent) {
        return;
    }
    radio_pages_detach();
    rk_page_open(&s_set, parent, "Sugarat Radio");
    s_set.back_cb=back_to_list;

    char sleep_buf[24];
    int sm = (s_ctl && s_ctl->sleep_min) ? s_ctl->sleep_min() : 0;
    if (sm <= 0) {
        snprintf(sleep_buf, sizeof(sleep_buf), "%s",SDG_T("Off","Off"));
    } else {
        snprintf(sleep_buf, sizeof(sleep_buf), SDG_T("%d 分钟","%d min"), sm);
    }

    s_wifi_displayed=wifi_prov_get_state();
    const char *wifi=wifi_state_label(s_wifi_displayed);

    /* Five visible rows; the header back arrow returns to the station list. */
    rk_page_row_compact(&s_set, SDG_T("WiFi 配网","Wi-Fi"), wifi, &s_set_ctx[4], set_wifi_cb, NULL);
    s_wifi_value=lv_obj_get_child(lv_obj_get_child(s_set.body,0),1);
    rk_page_row_compact(&s_set, SDG_T("地区","Region"), s_ctl && s_ctl->city ? s_ctl->city() : "United States",
                        &s_set_ctx[0], set_city_cb, NULL);
    rk_page_row_compact(&s_set, SDG_T("外观", "Appearance"), radio_skin_name(s_ctl && s_ctl->skin ? s_ctl->skin() : RADIO_SKIN_DEFAULT),
                        &s_set_ctx[1], set_theme_cb, NULL);
    rk_page_row_compact(&s_set, SDG_T("定时休眠","Sleep"), sleep_buf, &s_set_ctx[2], set_sleep_cb, NULL);
    rk_page_row_compact(&s_set, SDG_T("刷新电台","Refresh"), NULL, &s_set_ctx[3], set_refresh_cb, NULL);
}

/* ------------------------------------------------------------------ WiFi */
void radio_pages_open_wifi_page(lv_obj_t *parent) {
    radio_pages_detach();
    wifi_prov_show(parent);
}

void radio_pages_tick(void) {
    if (rk_page_is_open(&s_list)) {
        bool loading = s_ctl && s_ctl->loading && s_ctl->loading();
        if (s_list_loading && !loading) list_open_at(lv_scr_act(), s_list_offset);
        else s_list_loading = loading;
    }
    if (rk_page_is_open(&s_set) && s_wifi_value) {
        wifi_prov_state_t state = wifi_prov_get_state();
        if (state != s_wifi_displayed) {
            s_wifi_displayed = state;
            lv_label_set_text(s_wifi_value, wifi_state_label(state));
        }
    }
}
