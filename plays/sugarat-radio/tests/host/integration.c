/* Modified for Sugarat Radio by Deotaland, 2026. */
/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 深圳希德创新网络有限公司 (SDGOODS)
 * Actual application/page/settings code with deterministic SDK/service mocks.
 * Tests command routing and lifecycle, not audio, network or physical touch.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ui_radio.h"
#include "ui_radio_pages.h"
#include "ui_radio_skin.h"
#include "radio_settings.h"
#include "radio_player.h"
#include "radio_catalog.h"
#include "wifi_prov.h"
#include "nvs.h"
#include "sdgoods_app_shell.h"
#include "sdgoods_i18n.h"

static bool exists, fail_commit;
static char skin[24], pending[24];
static struct { char key[16]; int32_t value; } ints[8];
static int int_count;
esp_err_t nvs_open(const char *ns, int mode, nvs_handle_t *h)
{ (void)ns; if (!exists && mode == NVS_READONLY) return ESP_FAIL; exists = true; *h = 1; return ESP_OK; }
void nvs_close(nvs_handle_t h) { (void)h; pending[0] = 0; }
esp_err_t nvs_get_i32(nvs_handle_t h, const char *key, int32_t *value)
{ (void)h; for (int i = 0; i < int_count; ++i) if (!strcmp(key, ints[i].key)) { *value = ints[i].value; return ESP_OK; } return ESP_FAIL; }
esp_err_t nvs_set_i32(nvs_handle_t h, const char *key, int32_t value)
{ (void)h; int i; for (i = 0; i < int_count; ++i) if (!strcmp(key, ints[i].key)) break; assert(i < 8); if (i == int_count) ++int_count; snprintf(ints[i].key, 16, "%s", key); ints[i].value = value; return ESP_OK; }
esp_err_t nvs_get_str(nvs_handle_t h, const char *key, char *out, size_t *size)
{ (void)h; if (strcmp(key, "skin_id") || !skin[0] || *size < strlen(skin) + 1) return ESP_FAIL; strcpy(out, skin); *size = strlen(skin) + 1; return ESP_OK; }
esp_err_t nvs_set_str(nvs_handle_t h, const char *key, const char *s)
{ (void)h; if (!strcmp(key, "skin_id")) snprintf(pending, sizeof(pending), "%s", s); return ESP_OK; }
esp_err_t nvs_erase_key(nvs_handle_t h, const char *key) { (void)h; (void)key; return ESP_OK; }
esp_err_t nvs_commit(nvs_handle_t h)
{ (void)h; if (fail_commit) return ESP_FAIL; if (pending[0]) strcpy(skin, pending); pending[0] = 0; return ESP_OK; }

static radio_player_state_t state;
static int plays, stops;
static bool fail_play;
static char last_play_url[200];
esp_err_t radio_player_init(void) { return ESP_OK; }
esp_err_t radio_player_play(const char *url, const char *codec)
{ snprintf(last_play_url,sizeof(last_play_url),"%s",url); (void)codec; ++plays; if(fail_play)return ESP_FAIL; state = RP_PLAYING; return ESP_OK; }
void radio_player_stop(void) { ++stops; state = RP_IDLE; }
void radio_player_set_paused(bool paused) { state = paused ? RP_PAUSED : RP_PLAYING; }
radio_player_state_t radio_player_state(void) { return state; }
bool radio_player_is_busy(void) { return state == RP_PLAYING || state == RP_CONNECTING || state == RP_BUFFERING; }
int radio_player_levels(uint8_t *out, int n) { memset(out, 80, (size_t)n); return n; }
void mock_wifi_outcome(bool saved, bool portal);
void mock_wifi_fail(void);
static radio_station_t stations[64];
static int catalog_generation;
static char loaded_city[24];
int radio_host_hold_catalog;
void (*radio_host_pending_task)(void *);
void *radio_host_pending_arg;
int radio_catalog_load(const char *city)
{
    snprintf(loaded_city,sizeof(loaded_city),"%s",city?city:"");
    ++catalog_generation;
    for (int i = 0; i < 64; ++i) {
        snprintf(stations[i].name, sizeof(stations[i].name), SDG_T("测试电台 %02d","Test station %02d"), i);
        snprintf(stations[i].url, sizeof(stations[i].url), "https://example.invalid/%d/%d", catalog_generation, i);
        if (i == 0) snprintf(stations[i].name, sizeof(stations[i].name), "KEXP 90.3");
        stations[i].freq_mhz = i == 0 ? 90.3f : 103.7f;
    }
    return 64;
}
int radio_catalog_count(void) { return 64; }
const radio_station_t *radio_catalog_at(int idx) { return idx >= 0 && idx < 64 ? &stations[idx] : NULL; }
static void (*exit_cb)(void);
void sdgoods_cc_bind(lv_obj_t *scr)
{
    lv_obj_t *capture=lv_obj_create(scr);lv_obj_remove_style_all(capture);
    lv_obj_set_pos(capture,0,0);lv_obj_set_size(capture,360,70);
    lv_obj_clear_flag(capture,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(capture,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_PRESS_LOCK);
}
void sdgoods_app_shell_bind(lv_obj_t *scr) { sdgoods_cc_bind(scr); }
void sdgoods_app_shell_set_exit_cb(void (*cb)(void)) { exit_cb = cb; }
static void (*pause_cb)(void), (*resume_cb)(void);
void sdgoods_app_shell_set_pause_cb(void (*cb)(void)) { pause_cb = cb; }
void sdgoods_app_shell_set_resume_cb(void (*cb)(void)) { resume_cb = cb; }
void sdgoods_app_shell_leave(bool home)
{ (void)home; lv_scr_load(lv_obj_create(NULL)); if (exit_cb) exit_cb(); }
uint32_t sdg_i18n_seq(void) { return 0; }

static lv_obj_t *at(lv_obj_t *o, int x, int y)
{
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return NULL;
    lv_area_t a; lv_obj_get_coords(o, &a);
    if (x < a.x1 || x > a.x2 || y < a.y1 || y > a.y2) return NULL;
    for (int i = (int)lv_obj_get_child_cnt(o) - 1; i >= 0; --i) {
        lv_obj_t *hit = at(lv_obj_get_child(o, i), x, y);
        if (hit) return hit;
    }
    return lv_obj_has_flag(o, LV_OBJ_FLAG_CLICKABLE) ? o : NULL;
}

static void tick(void) { lv_tick_inc(130); lv_timer_handler(); lv_refr_now(NULL); }
static void tap(int x, int y)
{
    lv_obj_update_layout(lv_scr_act());
    lv_obj_t *o = at(lv_scr_act(), x, y);
    assert(o && !lv_obj_has_state(o, LV_STATE_DISABLED));
    lv_event_send(o,LV_EVENT_PRESSED,NULL);
    lv_event_send(o,LV_EVENT_RELEASED,NULL);
    lv_event_send(o, LV_EVENT_CLICKED, NULL);
    tick();
}

static bool has_text(lv_obj_t *o, const char *text)
{
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), text)) return true;
    for (unsigned i = 0; i < lv_obj_get_child_cnt(o); ++i)
        if (has_text(lv_obj_get_child(o, (int)i), text)) return true;
    return false;
}

static void open_settings(void) { tap(180,332); tap(274,94); }
void radio_host_snapshot(const char *name);

static void gesture(lv_dir_t direction)
{
    lv_indev_t mock = {0};
    mock.proc.types.pointer.gesture_dir = direction;
    lv_event_send(lv_scr_act(),LV_EVENT_PRESSED,NULL);
    lv_event_send(lv_scr_act(),LV_EVENT_GESTURE,&mock);
    lv_event_send(lv_scr_act(),LV_EVENT_RELEASED,NULL);
    tick();
}

static lv_point_t pointer_point;
static lv_indev_state_t pointer_state;
static void pointer_read(lv_indev_drv_t *driver,lv_indev_data_t *data)
{(void)driver;data->point=pointer_point;data->state=pointer_state;}
static void pointer_sample(int x,int y,lv_indev_state_t state)
{
    pointer_point=(lv_point_t){x,y};pointer_state=state;
    lv_tick_inc(40);lv_timer_handler();lv_refr_now(NULL);
}
static void drag(int x,int y,int end_x,int end_y)
{
    pointer_sample(x,y,LV_INDEV_STATE_PRESSED);
    for(int i=1;i<=8;++i)pointer_sample(x+(end_x-x)*i/8,y+(end_y-y)*i/8,LV_INDEV_STATE_PRESSED);
    pointer_sample(end_x,end_y,LV_INDEV_STATE_RELEASED);tick();
}
static lv_obj_t *find_text(lv_obj_t *o,const char *text)
{
    if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))return NULL;
    if(lv_obj_check_type(o,&lv_label_class) && !strcmp(lv_label_get_text(o),text))return o;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(o);++i){lv_obj_t *found=find_text(lv_obj_get_child(o,(int)i),text);if(found)return found;}
    return NULL;
}
static lv_obj_t *find_type(lv_obj_t *o,const lv_obj_class_t *type)
{
    if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))return NULL;
    if(lv_obj_check_type(o,type))return o;
    for(uint32_t i=0;i<lv_obj_get_child_cnt(o);++i){lv_obj_t *found=find_type(lv_obj_get_child(o,(int)i),type);if(found)return found;}
    return NULL;
}
static void navigation_and_network(void)
{
    static lv_indev_drv_t driver;
    lv_indev_drv_init(&driver);driver.type=LV_INDEV_TYPE_POINTER;driver.read_cb=pointer_read;
    lv_indev_drv_register(&driver);
    int old_plays=plays,old_stops=stops;
    drag(180,332,180,212);
    assert(radio_pages_any_open());
    drag(270,148,90,148);
    assert(has_text(lv_scr_act(),SDG_T("测试电台 03","Test station 03")));
    drag(90,148,270,148);
    assert(has_text(lv_scr_act(),"KEXP 90.3"));
    drag(90,148,270,148);
    assert(has_text(lv_scr_act(),"KEXP 90.3"));
    for(int i=0;i<21;++i)drag(270,148,90,148);
    assert(has_text(lv_scr_act(),SDG_T("测试电台 63","Test station 63")));
    drag(180,240,180,120);
    assert(!radio_pages_any_open() && plays==old_plays && stops==old_stops);
    assert(!strcmp(radio_settings_get_skin(),"space-signal"));
    puts("PASS: real LVGL pointer input: main up opens list, row swipes page both ways without playback, all 64 stations reachable, up closes list.");
    open_settings();
    assert(has_text(lv_scr_act(), "Connected"));
    mock_wifi_fail(); tick();
    assert(has_text(lv_scr_act(), "Failed"));
    mock_wifi_outcome(true, false); tick();
    tap(180,178); radio_host_snapshot("us-region-picker");
    tap(180,196); tick(); tick();
    assert(!strcmp(loaded_city, "nyc") && !radio_pages_any_open());
    assert(plays == old_plays && stops == old_stops);
    open_settings(); tap(180,142); tick();
    assert(lv_scr_act() != NULL && !has_text(lv_scr_act(), "Sugarat Radio"));
    assert(has_text(lv_scr_act(), "On device"));
    radio_host_snapshot("wifi-settings");
    /* A new, committed connection must return to the existing radio screen. */
    mock_wifi_outcome(false, false); tick(); tick();
    assert(has_text(lv_scr_act(), "On device"));
    mock_wifi_outcome(true, false); tick(); tick();
    assert(ui_radio_is_open() && !has_text(lv_scr_act(), "On device"));
    assert(plays == old_plays && stops == old_stops);
    puts("PASS: US region selection preserves audio; setup ignores save failures and returns to the existing radio after committed connection.");
}

void radio_host_integration(void)
{
    assert(!strcmp(radio_settings_get_skin(), "dot-matrix"));
    radio_settings_set_night(false);
    assert(!strcmp(radio_settings_get_skin(), "paper-radio"));
    radio_settings_set_night(true);
    assert(!strcmp(radio_settings_get_skin(), "dot-matrix"));
    assert(radio_skin_find("editorial")==0);
    assert(radio_settings_set_skin("vinyl-club"));
    assert(!strcmp(radio_settings_get_skin(),"vinyl-pixel"));
    fail_commit=true;
    assert(!radio_settings_set_skin("editorial"));
    assert(!strcmp(radio_settings_get_skin(),"vinyl-pixel"));
    fail_commit=false;
    assert(radio_settings_set_skin("dot-matrix"));
    radio_settings_set_sleep_min(30);
    radio_settings_set_fav(5, true);
    assert(!radio_settings_set_skin(""));
    assert(!radio_settings_set_skin("this-id-is-too-long-to-be-a-valid-skin-id"));
    lv_obj_t *previous = lv_scr_act();
    assert(wifi_prov_start() == ESP_OK);
    ui_radio_start();
    lv_obj_del(previous);
    tick(); tick();
    assert(!ui_radio_is_open() && has_text(lv_scr_act(), "On device"));
    radio_host_snapshot("first-setup");
    mock_wifi_outcome(false, false); tick(); tick();
    assert(!ui_radio_is_open());
    mock_wifi_outcome(true, true); tick(); tick();
    assert(!ui_radio_is_open());
    mock_wifi_outcome(true, false);
    for (int i=0; i<6; ++i) tick();
    assert(ui_radio_is_open());
    radio_host_snapshot("radio-home");
    assert(plays == 1 && state == RP_PLAYING);
    open_settings();radio_host_snapshot("settings"); tap(180, 214);
    tap(262, 298); tap(262, 298); tap(262, 298);
    tap(180, 298);
    assert(!strcmp(radio_settings_get_skin(), "cat-dj"));
    assert(plays == 1 && stops == 0 && state == RP_PLAYING);
    tap(180, 294);
    assert(state == RP_PAUSED);
    open_settings(); tap(180, 214); tap(262, 298); tap(180, 298);
    assert(!strcmp(radio_settings_get_skin(), "space-signal"));
    assert(plays == 1 && stops == 0 && state == RP_PAUSED);
    open_settings(); tap(180, 214); tap(262, 298); tap(80, 94);
    assert(!strcmp(radio_settings_get_skin(), "space-signal"));
    fail_commit = true;
    radio_pages_detach();open_settings(); tap(180, 214); tap(262, 298); tap(180, 298);
    assert(!strcmp(radio_settings_get_skin(), "space-signal"));
    assert(plays == 1 && stops == 0 && state == RP_PAUSED);
    fail_commit = false;
    radio_pages_detach();
    gesture(LV_DIR_LEFT);
    assert(!strcmp(radio_settings_get_skin(),"seaside-sunset"));
    assert(state==RP_PAUSED && plays==1 && stops==0);
    gesture(LV_DIR_RIGHT);
    assert(!strcmp(radio_settings_get_skin(),"space-signal"));
    gesture(LV_DIR_TOP);
    assert(radio_pages_any_open());radio_pages_detach();gesture(LV_DIR_BOTTOM);
    assert(!strcmp(radio_settings_get_skin(),"space-signal"));
    for(int i=0;i<RADIO_SKIN_COUNT;i++)gesture(LV_DIR_LEFT);
    for(int i=0;i<RADIO_SKIN_COUNT;i++)gesture(LV_DIR_RIGHT);
    assert(!strcmp(radio_settings_get_skin(),"space-signal"));
    assert(state==RP_PAUSED && plays==1 && stops==0);
    int original_skin=radio_skin_find(radio_settings_get_skin());
    open_settings();tap(180,214);
    for(int i=original_skin;i<RADIO_SKIN_COUNT-1;++i)tap(262,298);
    assert(has_text(lv_scr_act(),SDG_T("紫夜唱片","Night Vinyl")));
    assert(has_text(lv_scr_act(),SDG_T("外观 13 / 13","Style 13 / 13")));
    radio_host_snapshot("vinyl-picker");tap(180,298);
    assert(!strcmp(radio_settings_get_skin(),"vinyl-night"));
    assert(state==RP_PAUSED && plays==1 && stops==0);
    gesture(LV_DIR_LEFT);
    assert(!strcmp(radio_settings_get_skin(),"paper-radio"));
    gesture(LV_DIR_RIGHT);
    assert(!strcmp(radio_settings_get_skin(),"vinyl-night"));
    for(int i=0;i<=original_skin;++i)gesture(LV_DIR_LEFT);
    assert(!strcmp(radio_settings_get_skin(),"space-signal"));
    assert(state==RP_PAUSED && plays==1 && stops==0);
    pause_cb(); gesture(LV_DIR_LEFT); resume_cb();
    assert(!strcmp(radio_settings_get_skin(),"space-signal"));
    tap(180,332);
    gesture(LV_DIR_LEFT);
    assert(!strcmp(radio_settings_get_skin(),"space-signal"));
    radio_host_snapshot("station-list");
    for (int i = 0; i < 21; ++i) tap(262, 298);
    assert(has_text(lv_scr_act(), SDG_T("测试电台 63","Test station 63")));
    tap(180, 145);
    assert(plays == 2 && state == RP_PLAYING);
    assert(radio_settings_get_last_index() == 63);
    assert(radio_settings_get_sleep_min() == 30 && radio_settings_is_fav(5));
    char previous_url[200];
    snprintf(previous_url,sizeof(previous_url),"%s",last_play_url);
    open_settings(); tap(180,286); tick();
    state=RP_ERROR;
    tap(180,294);
    assert(plays==3 && !strcmp(last_play_url,previous_url));
    fail_play=true;
    tap(262,294);
    assert(plays==4 && state==RP_IDLE && has_text(lv_scr_act(),SDG_T("连接失败，点播放重试","Failed - press play to retry")));
    fail_play=false;
    tap(180,294);
    assert(plays==5 && state==RP_PLAYING);
    navigation_and_network();
    ui_radio_close();
    assert(!ui_radio_is_open() && stops == 2);
    puts("PASS: actual application/page flow: legacy migration, playing/paused skin switches without audio restart, cancel, failed-save rollback, all 64 stations reachable, preferences preserved, horizontal gesture cycles, vertical/overlay/page isolation, exit cleanup. SDK/services mocked.");
}
