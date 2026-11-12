/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 */
#include "launcher_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "sdkconfig.h"
#include "sdgoods_cc.h"
#include "sdgoods_hw_info.h"
#include "app_icon.h"
#include "app_store.h"
#include "launcher_job.h"
#include "launcher_theme.h"
#include "launcher_wifi.h"

typedef enum {
    VIEW_HOME, VIEW_APPS, VIEW_DOWNLOAD, VIEW_SETTINGS, VIEW_WIFI,
    VIEW_BUSY, VIEW_PLAY, VIEW_PROGRESS, VIEW_RESULT, VIEW_APP, VIEW_LAUNCH,
} view_t;
typedef void (*build_fn)(lv_obj_t *screen);
typedef void (*alert_fn)(int action);

enum {   /* sw_set_action ids; app rows use their list index and keypad keys their digit */
    KEY_BACKSPACE = 10, KEY_OK = 11,
    ACT_OPEN = 900, ACT_UPDATE, ACT_UNINSTALL, ACT_INSTALL, ACT_REPLACE, ACT_MANAGE, ACT_DONE,
    ACT_WIFI, ACT_CONTROL, ACT_STORAGE, ACT_REMOVE_ALL, ACT_CANCEL_INSTALL, ACT_HELP,
    ACT_MY_APPS, ACT_DOWNLOAD, ACT_SETTINGS,   /* home tiles, in order */
};

static struct {
    view_t view;
    build_fn build;
    lv_obj_t *old;
    bool pending;
    lv_timer_t *timer;              /* Per-screen timer, deleted on navigation. */
    lv_obj_t *alert;
    alert_fn alert_cb;
    char code[7];
    lv_obj_t *digit[6], *box[6], *ok_label;
    lv_obj_t *wifi_label, *battery_label, *count_label, *free_label;
    lv_obj_t *ring, *percent, *status, *cancel;
    char busy_text[64];
    uint32_t job_sequence;
    community_play_t play;
    app_info_t *apps;               /* PSRAM list for the current screen. */
    int app_count;
    int detail;                     /* Index into apps. */
    int launch_slot;
    char launch_title[64];
    bool long_pressed;
    bool result_ok;
    char result_title[48], result_text[160];
    int result_slot;
    build_fn result_back;
    char wifi_seen[33];
    int home_wifi, home_battery, home_count;   /* Values shown on the home screen. */
} ui;

static void navigate(build_fn build);
static void build_home(lv_obj_t *screen);
static void build_apps(lv_obj_t *screen);
static void build_download(lv_obj_t *screen);
static void build_settings(lv_obj_t *screen);

/* ------------------------------------------------------------ text helpers */

static const lv_font_t *font(sw_font_t role) { return sw_font(role); }

static void format_mb(char *out, size_t cap, uint32_t bytes) {
    uint32_t tenths = (uint32_t)(((uint64_t)bytes * 10 + 524288) / 1048576);
    snprintf(out, cap, "%lu.%lu MB", (unsigned long)(tenths / 10), (unsigned long)(tenths % 10));
}

static const char *network_text(community_status_t status) {
    switch (status) {
    case COMMUNITY_NOT_FOUND: return "没有找到这个编号的公开应用。";
    case COMMUNITY_OFFLINE: return "网络连接失败，请检查 Wi-Fi 后重试。";
    case COMMUNITY_SERVER: return "社区服务暂时不可用，请稍后再试。";
    case COMMUNITY_FORMAT: return "这个应用发布的不是可安装的固件文件。";
    case COMMUNITY_CANCELLED: return "已取消安装，没有留下未完成的文件。";
    case COMMUNITY_OK: case COMMUNITY_SINK: break;
    }
    return "操作失败，请重试。";
}

static const char *problem_text(app_problem_t problem) {
    switch (problem) {
    case APP_ERR_LAYOUT: return "设备的分区不是启动器布局，无法管理应用。";
    case APP_ERR_SPACE: return "存储空间不足，请先卸载其他应用。";
    case APP_ERR_SLOTS: return "最多安装 16 个应用，请先卸载一个。";
    case APP_ERR_FLASH: return "写入存储失败，请重试。";
    case APP_ERR_IMAGE: return "下载的文件不是这台设备可以运行的固件。";
    case APP_ERR_HASH: return "文件校验失败，下载内容不完整，请重试。";
    case APP_ERR_CONFLICT: return "已有另一个应用使用相同的应用标识，无法安装。";
    case APP_ERR_SIZE: return "下载的文件大小与发布信息不一致。";
    case APP_ERR_NO_MEM: return "内存不足，请重启设备后再试。";
    case APP_OK: break;
    }
    return "操作失败，请重试。";
}

static sw_icon_t category_icon(const char *category) {
    if (!strcmp(category, "music")) return SW_ICON_MUSIC;
    if (!strcmp(category, "game")) return SW_ICON_GAME;
    if (!strcmp(category, "pixel")) return SW_ICON_IMAGE;
    if (!strcmp(category, "ai") || !strcmp(category, "companion")) return SW_ICON_ROBOT;
    if (!strcmp(category, "tool")) return SW_ICON_SETTINGS;
    return SW_ICON_STAR;
}

static uint32_t name_color(const char *name) {
    static const uint32_t colors[] = { TH_ACCENT, TH_VIOLET, TH_GREEN, TH_AMBER, TH_BLUE, 0x8A3B5A };
    uint32_t hash = 5381;
    for (const char *p = name; *p; p++) hash = hash * 33 + (uint8_t)*p;
    return colors[hash % (sizeof colors / sizeof *colors)];
}

/* Centered column for result, play and detail content. */
static lv_obj_t *column(lv_obj_t *parent, lv_coord_t gap) {
    lv_obj_t *col = th_plain(parent);
    lv_obj_set_size(col, 280, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, gap, 0);
    return col;
}

static void spinner(lv_obj_t *screen, const char *text) {
    lv_obj_t *col = column(screen, 16);
    th_arc_colors(sw_spinner(col, 56, SW_TONE_ACCENT), TH_ACCENT, TH_CARD);
    th_wrap(col, text, font(SW_FONT_BODY), TH_MUTED, 240);
    lv_obj_center(col);
}

static int load_apps(app_space_t *space) {
    if (!ui.apps) ui.apps = heap_caps_calloc(APP_STORE_MAX, sizeof(app_info_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ui.app_count = ui.apps ? app_store_snapshot(ui.apps, APP_STORE_MAX, space) : 0;
    return ui.app_count;
}

static const app_info_t *installed_play(uint32_t play_id) {
    for (int i = 0; i < ui.app_count; i++) if (ui.apps[i].play_id == play_id && !ui.apps[i].damaged) return &ui.apps[i];
    return NULL;
}

/* ------------------------------------------------------------ alerts */

static void close_alert(void) {
    if (ui.alert) lv_obj_del_async(ui.alert);
    ui.alert = NULL;
    ui.alert_cb = NULL;
}

static void alert_click(lv_event_t *e) {
    int action = sw_event_action(e);
    if (action == SW_ACTION_NONE) return;
    alert_fn callback = ui.alert_cb;
    close_alert();
    if (callback) callback(action);
}

/* Full-screen confirmation on the top layer. `on_action` receives SW_ACTION_CONFIRM or SW_ACTION_CANCEL. */
static void show_alert(const char *title, const char *message, const char *confirm, th_button_t kind,
                       const char *cancel, alert_fn on_action) {
    close_alert();
    lv_obj_t *modal = th_box(lv_layer_top(), TH_BG, 0);
    lv_obj_clear_flag(modal, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(modal, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(modal, 360, 360);
    lv_obj_t *col = column(modal, 10);
    th_wrap(col, title, font(SW_FONT_HEADLINE), TH_TEXT, 250);
    if (message) th_wrap(col, message, font(SW_FONT_BODY), TH_MUTED, 250);
    lv_obj_t *gap = th_plain(col);
    lv_obj_set_size(gap, 1, 6);
    th_button(col, confirm, kind, SW_ACTION_CONFIRM);
    if (cancel) th_button(col, cancel, TH_SECONDARY, SW_ACTION_CANCEL);
    lv_obj_center(col);
    lv_obj_add_event_cb(modal, alert_click, LV_EVENT_CLICKED, NULL);
    ui.alert = modal;
    ui.alert_cb = on_action;
}

/* ------------------------------------------------------------ navigation */

static void stop_timer(void) {
    if (ui.timer) { lv_timer_del(ui.timer); ui.timer = NULL; }
}

static void build_now(void *unused) {
    (void)unused;
    ui.pending = false;
    if (ui.old) { lv_obj_del(ui.old); ui.old = NULL; }
    ui.long_pressed = false;
    ui.ring = ui.percent = ui.status = ui.cancel = ui.ok_label = ui.wifi_label = ui.battery_label = ui.count_label = ui.free_label = NULL;
    memset(ui.digit, 0, sizeof ui.digit);
    memset(ui.box, 0, sizeof ui.box);
    ui.build(lv_scr_act());
}

/* Load an empty screen, then delete the old one and build the new one on the next tick, so that
 * only one populated screen exists at a time and no object is deleted inside its own event. */
static void navigate(build_fn build) {
    stop_timer();
    close_alert();
    ui.build = build;
    if (ui.pending) return;
    ui.pending = true;
    ui.old = lv_scr_act();
    lv_scr_load(th_screen());
    lv_async_call(build_now, NULL);
}

static void wifi_done(void) {
    if (ui.view == VIEW_WIFI) navigate(build_settings);
}

static void open_wifi(void) {
    lv_obj_t *setup = launcher_wifi_setup_create(wifi_done);
    if (!setup) {
        show_alert("无法打开联网设置", "请重启设备后再试。", "知道了", TH_PRIMARY, NULL, NULL);
        return;
    }
    stop_timer();
    close_alert();
    ui.view = VIEW_WIFI;
    lv_obj_t *old = lv_scr_act();
    lv_scr_load(setup);
    lv_obj_del_async(old);
}

static void need_wifi_action(int action) {
    if (action == SW_ACTION_CONFIRM) open_wifi();
}

static bool require_wifi(void) {
    if (launcher_wifi_connected()) return true;
    show_alert("还没有联网", "下载应用前需要先连接 Wi-Fi。", "去连接", TH_PRIMARY, "取消", need_wifi_action);
    return false;
}

/* ------------------------------------------------------------ results and busy screens */

static void result_click(lv_event_t *e);

static void build_result(lv_obj_t *screen) {
    ui.view = VIEW_RESULT;
    lv_obj_t *col = column(screen, 10);
    lv_obj_t *mark = th_box(col, ui.result_ok ? TH_ACCENT : TH_KEY, LV_RADIUS_CIRCLE);
    lv_obj_set_size(mark, 64, 64);
    if (ui.result_ok) lv_obj_center(th_text(mark, LV_SYMBOL_OK, &lv_font_montserrat_24, TH_ON_ACCENT));
    else lv_obj_center(th_icon(mark, SW_ICON_WARNING, SW_SIZE_L, TH_BRAND_TEXT));
    th_wrap(col, ui.result_title, font(SW_FONT_HEADLINE), TH_TEXT, 250);
    th_wrap(col, ui.result_text, font(SW_FONT_BODY), TH_MUTED, 250);
    if (ui.result_ok && ui.result_slot >= 0) {
        th_button(col, "打开", TH_PRIMARY, ACT_OPEN);
        th_button(col, "完成", TH_SECONDARY, ACT_DONE);
    } else {
        th_button(col, "返回", TH_SECONDARY, ACT_DONE);
    }
    lv_obj_center(col);
    lv_obj_add_event_cb(screen, result_click, LV_EVENT_CLICKED, NULL);
}

static void show_result(bool ok, const char *title, const char *text, int slot, build_fn back) {
    ui.result_ok = ok;
    strlcpy(ui.result_title, title, sizeof ui.result_title);
    strlcpy(ui.result_text, text, sizeof ui.result_text);
    ui.result_slot = slot;
    ui.result_back = back;
    navigate(build_result);
}

static void launch_now(lv_timer_t *timer) {
    (void)timer;
    ui.timer = NULL;   /* single-shot timers delete themselves */
    app_problem_t problem = app_store_select_boot(ui.launch_slot);
    if (problem == APP_OK) esp_restart();
    show_result(false, "无法打开应用", problem_text(problem), -1, build_apps);
}

static void build_launch(lv_obj_t *screen) {
    ui.view = VIEW_LAUNCH;
    char text[96];
    snprintf(text, sizeof text, "正在打开 %s", ui.launch_title);
    spinner(screen, text);
    ui.timer = lv_timer_create(launch_now, 350, NULL);
    lv_timer_set_repeat_count(ui.timer, 1);
}

static void launch(int slot, const char *title) {
    ui.launch_slot = slot;
    strlcpy(ui.launch_title, title, sizeof ui.launch_title);
    navigate(build_launch);
}

static void result_click(lv_event_t *e) {
    int action = sw_event_action(e);
    if (action == ACT_OPEN) launch(ui.result_slot, ui.launch_title);
    else if (action == ACT_DONE) navigate(ui.result_back);
}

static void build_play(lv_obj_t *screen);

static void job_finished(const launcher_job_status_t *status) {
    char text[160];
    switch (status->kind) {
    case JOB_LOOKUP:
        if (status->phase == JOB_SUCCEEDED) { ui.play = status->play; navigate(build_play); }
        else show_result(false, "查询失败", network_text(status->network), -1, build_download);
        break;
    case JOB_INSTALL:
        if (status->phase == JOB_SUCCEEDED) {
            snprintf(text, sizeof text, "%s %s 已安装。", status->installed.title, status->installed.version);
            strlcpy(ui.launch_title, status->installed.title, sizeof ui.launch_title);
            show_result(true, "安装完成", text, status->installed.slot, build_home);
        } else if (status->network == COMMUNITY_CANCELLED) {
            show_result(false, "已取消", network_text(status->network), -1, build_download);
        } else if (status->network != COMMUNITY_OK && status->network != COMMUNITY_SINK) {
            show_result(false, "安装失败", network_text(status->network), -1, build_download);
        } else {
            show_result(false, "安装失败", problem_text(status->problem), -1, build_download);
        }
        break;
    case JOB_UNINSTALL:
        if (status->phase == JOB_SUCCEEDED) navigate(build_apps);
        else show_result(false, "卸载失败", problem_text(status->problem), -1, build_apps);
        break;
    case JOB_NONE:
        navigate(build_home);
        break;
    }
}

static void busy_poll(lv_timer_t *timer) {
    (void)timer;
    launcher_job_status_t status;
    launcher_job_status(&status);
    if (status.sequence == ui.job_sequence && status.phase != JOB_RUNNING) job_finished(&status);
}

static void build_busy(lv_obj_t *screen) {
    ui.view = VIEW_BUSY;
    spinner(screen, ui.busy_text);
    ui.timer = lv_timer_create(busy_poll, 200, NULL);
}

static void track_job(const char *text) {
    launcher_job_status_t status;
    launcher_job_status(&status);
    ui.job_sequence = status.sequence;
    strlcpy(ui.busy_text, text, sizeof ui.busy_text);
    navigate(build_busy);
}

/* ------------------------------------------------------------ install progress */

static void progress_poll(lv_timer_t *timer) {
    (void)timer;
    launcher_job_status_t status;
    launcher_job_status(&status);
    if (status.sequence != ui.job_sequence) return;
    if (status.phase != JOB_RUNNING) { job_finished(&status); return; }
    uint32_t percent = status.total ? (uint32_t)((uint64_t)status.received * 100 / status.total) : 0;
    char text[64], done[16], total[16];
    sw_ring_set(ui.ring, (uint8_t)percent);
    snprintf(text, sizeof text, "%lu%%", (unsigned long)percent);
    lv_label_set_text(ui.percent, text);
    if (status.step == JOB_STEP_PREPARE) lv_label_set_text(ui.status, "正在准备存储空间");
    else if (status.step == JOB_STEP_VERIFY) lv_label_set_text(ui.status, "正在校验固件");
    else {
        format_mb(done, sizeof done, status.received);
        format_mb(total, sizeof total, status.total);
        snprintf(text, sizeof text, "已下载 %s / %s", done, total);
        lv_label_set_text(ui.status, text);
    }
    if (status.step == JOB_STEP_VERIFY && ui.cancel) lv_obj_add_flag(ui.cancel, LV_OBJ_FLAG_HIDDEN);
}

static void progress_click(lv_event_t *e) {
    if (sw_event_action(e) != ACT_CANCEL_INSTALL) return;
    launcher_job_cancel();
    lv_label_set_text(ui.status, "正在取消");
    lv_obj_clear_flag(ui.cancel, LV_OBJ_FLAG_CLICKABLE);
}

static void build_progress(lv_obj_t *screen) {
    ui.view = VIEW_PROGRESS;
    ui.ring = sw_ring(screen, 316, 0, SW_TONE_ACCENT);
    th_arc_colors(ui.ring, TH_ACCENT, TH_CARD);
    lv_obj_center(ui.ring);
    lv_obj_t *title = th_text(screen, ui.play.title, font(SW_FONT_BODY), TH_TEXT);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(title, 200);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -66);
    ui.percent = th_text(screen, "0%", font(SW_FONT_NUM_M), TH_TEXT);
    lv_obj_align(ui.percent, LV_ALIGN_CENTER, 0, -18);
    ui.status = th_text(screen, "正在准备存储空间", font(SW_FONT_CAPTION), TH_MUTED);
    lv_obj_align(ui.status, LV_ALIGN_CENTER, 0, 24);
    ui.cancel = th_box(screen, TH_KEY, 22);
    lv_obj_set_size(ui.cancel, 120, 44);
    th_tap(ui.cancel, ACT_CANCEL_INSTALL);
    lv_obj_center(th_text(ui.cancel, "取消", font(SW_FONT_BODY), TH_TEXT));
    lv_obj_align(ui.cancel, LV_ALIGN_CENTER, 0, 82);
    lv_obj_add_event_cb(screen, progress_click, LV_EVENT_CLICKED, NULL);
    ui.timer = lv_timer_create(progress_poll, 200, NULL);
}

static void start_install(int replaces_slot, bool release_first) {
    if (!require_wifi()) return;
    if (!launcher_job_install(&ui.play, replaces_slot, release_first)) return;
    launcher_job_status_t status;
    launcher_job_status(&status);
    ui.job_sequence = status.sequence;
    navigate(build_progress);
}

/* ------------------------------------------------------------ play confirmation */

static void play_click(lv_event_t *e) {
    const app_info_t *installed = installed_play(ui.play.play_id);
    switch (sw_event_action(e)) {
    case ACT_INSTALL: start_install(installed ? installed->slot : -1, false); break;
    case ACT_REPLACE: start_install(installed ? installed->slot : -1, true); break;
    case ACT_OPEN: if (installed) launch(installed->slot, installed->title); break;
    case ACT_MANAGE: navigate(build_apps); break;
    case SW_ACTION_BACK: navigate(build_download); break;
    default: break;
    }
}

static void build_play(lv_obj_t *screen) {
    ui.view = VIEW_PLAY;
    const community_play_t *play = &ui.play;
    app_space_t space;
    load_apps(&space);
    const app_info_t *installed = installed_play(play->play_id);
    bool compatible = play->product_id == CONFIG_LAUNCHER_PRODUCT_ID &&
                      play->hardware_revision_id == CONFIG_LAUNCHER_HARDWARE_REVISION_ID;
    uint32_t need = app_store_slot_bytes(play->size);
    char line[128], size[16], need_text[16], free_text[16];
    format_mb(size, sizeof size, play->size);
    format_mb(need_text, sizeof need_text, need);
    format_mb(free_text, sizeof free_text, space.largest);

    lv_obj_t *list = th_grid(screen);
    const app_icon_t *icon = app_icon_find(play->play_id);
    if (icon) th_image_badge(list, &icon->large);
    else th_badge(list, category_icon(play->category), name_color(play->title), 64);
    th_wrap(list, play->title, font(SW_FONT_HEADLINE), TH_TEXT, 250);
    snprintf(line, sizeof line, "%s · v%s", play->author[0] ? play->author : "社区作者", play->version);
    th_wrap(list, line, font(SW_FONT_CAPTION), TH_MUTED, 250);
    snprintf(line, sizeof line, "大小 %s · 需要 %s\n当前最大可用 %s", size, need_text, free_text);
    th_wrap(list, line, font(SW_FONT_CAPTION), TH_MUTED, 250);

    if (!compatible) {
        th_wrap(list, "这个应用不适配这台设备。", font(SW_FONT_BODY), TH_BRAND_TEXT, 250);
    } else if (installed && !strcmp(installed->version, play->version)) {
        th_wrap(list, "已安装最新版本。", font(SW_FONT_BODY), TH_TEXT, 250);
        th_button(list, "打开", TH_PRIMARY, ACT_OPEN);
    } else if (space.largest >= need) {
        if (installed) {
            snprintf(line, sizeof line, "已安装 v%s，更新会保留数据。", installed->version);
            th_wrap(list, line, font(SW_FONT_CAPTION), TH_MUTED, 250);
        }
        th_button(list, installed ? "更新" : "安装", TH_PRIMARY, ACT_INSTALL);
    } else if (installed) {
        th_wrap(list, "空间不足，需要先移除旧版本再更新。数据会保留；下载失败时需要重新安装。",
                font(SW_FONT_CAPTION), TH_BRAND_TEXT, 250);
        th_button(list, "移除旧版本并更新", TH_PRIMARY, ACT_REPLACE);
    } else {
        th_wrap(list, "存储空间不足，请先卸载其他应用。", font(SW_FONT_BODY), TH_BRAND_TEXT, 250);
        th_button(list, "管理应用", TH_SECONDARY, ACT_MANAGE);
    }
    snprintf(line, sizeof line, "#%06lu", (unsigned long)play->play_id);
    th_header(screen, line, SW_ACTION_NONE);
    lv_obj_add_event_cb(screen, play_click, LV_EVENT_CLICKED, NULL);
}

static void lookup(uint32_t play_id) {
    if (!require_wifi()) return;
    if (!launcher_job_lookup(play_id)) return;
    char text[48];
    snprintf(text, sizeof text, "正在查询应用 #%06lu", (unsigned long)play_id);
    track_job(text);
}

/* ------------------------------------------------------------ app detail */

static void uninstall_action(int action) {
    if (action == SW_ACTION_CONFIRM && launcher_job_uninstall(ui.apps[ui.detail].slot)) track_job("正在卸载");
}

static void detail_click(lv_event_t *e) {
    const app_info_t *app = &ui.apps[ui.detail];
    char title[96];
    switch (sw_event_action(e)) {
    case ACT_OPEN: launch(app->slot, app->title); break;
    case ACT_UPDATE: lookup(app->play_id); break;
    case ACT_UNINSTALL:
        snprintf(title, sizeof title, "卸载「%s」？", app->damaged ? "损坏的应用" : app->title);
        show_alert(title, "它保存的数据会一起删除。", "卸载", TH_DANGER, "取消", uninstall_action);
        break;
    case SW_ACTION_BACK: navigate(build_apps); break;
    default: break;
    }
}

static void build_app(lv_obj_t *screen) {
    ui.view = VIEW_APP;
    const app_info_t *app = &ui.apps[ui.detail];
    char line[96], size[16];
    lv_obj_t *grid = th_grid(screen);
    const app_icon_t *icon = app->damaged ? NULL : app_icon_find(app->play_id);
    if (icon) th_image_badge(grid, &icon->large);
    else th_badge(grid, app->damaged ? SW_ICON_WARNING : category_icon(app->category), app->damaged ? TH_KEY : name_color(app->project), 64);
    th_wrap(grid, app->damaged ? "损坏的应用" : app->title, font(SW_FONT_HEADLINE), TH_TEXT, 250);
    format_mb(size, sizeof size, app->size);
    if (app->damaged) snprintf(line, sizeof line, "无法校验这个位置上的固件，占用 %s。", size);
    else if (app->play_id) snprintf(line, sizeof line, "v%s · %s · #%06lu", app->version, size, (unsigned long)app->play_id);
    else snprintf(line, sizeof line, "v%s · %s", app->version, size);
    th_wrap(grid, line, font(SW_FONT_CAPTION), TH_MUTED, 250);
    lv_obj_t *row = th_grid_row(grid);
    if (!app->damaged) th_tile(row, SW_ICON_PLAY, TH_ACCENT, NULL, "打开", NULL, 80, 88, ACT_OPEN);
    if (!app->damaged && app->play_id) th_tile(row, SW_ICON_SYNC, TH_VIOLET, NULL, "检查更新", NULL, 80, 88, ACT_UPDATE);
    th_tile(row, SW_ICON_TRASH, TH_RED, NULL, "卸载", NULL, 80, 88, ACT_UNINSTALL);
    th_header(screen, "应用详情", SW_ACTION_NONE);
    lv_obj_add_event_cb(screen, detail_click, LV_EVENT_CLICKED, NULL);
}

/* ------------------------------------------------------------ My apps (page 2 of home) */

static void apps_click(lv_event_t *e) {
    int action = sw_event_action(e);
    if (action == SW_ACTION_BACK) { navigate(build_home); return; }
    if (action == ACT_DOWNLOAD) { navigate(build_download); return; }
    if (ui.long_pressed) { ui.long_pressed = false; return; }
    if (action < 0 || action >= ui.app_count) return;
    ui.detail = action;
    if (ui.apps[action].damaged) navigate(build_app);
    else launch(ui.apps[action].slot, ui.apps[action].title);
}

static void app_long(lv_event_t *e) {
    int index = sw_event_action(e);
    if (index < 0 || index >= ui.app_count) return;
    ui.long_pressed = true;
    ui.detail = index;
    navigate(build_app);
}

static void apps_gesture(lv_event_t *e) {
    (void)e;
    if (lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) navigate(build_home);
}

static void store_poll(lv_timer_t *timer) {
    (void)timer;
    if (app_store_state() != APP_STORE_LOADING) navigate(build_apps);
}

static void build_apps(lv_obj_t *screen) {
    ui.view = VIEW_APPS;
    lv_obj_add_event_cb(screen, apps_click, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screen, apps_gesture, LV_EVENT_GESTURE, NULL);
    sdgoods_cc_bind(screen);
    app_store_state_t state = app_store_state();
    if (state != APP_STORE_READY) {
        if (state == APP_STORE_LOADING) {
            spinner(screen, "正在检查已安装的应用");
            ui.timer = lv_timer_create(store_poll, 300, NULL);
        } else {
            lv_obj_t *col = column(screen, 8);
            th_wrap(col, "无法管理应用", font(SW_FONT_HEADLINE), TH_TEXT, 250);
            th_wrap(col, "设备的分区不是启动器布局。", font(SW_FONT_BODY), TH_MUTED, 250);
            lv_obj_center(col);
        }
        th_header(screen, "我的应用", SW_ACTION_NONE);
        return;
    }
    app_space_t space;
    int count = load_apps(&space);
    if (count == 0) {
        lv_obj_t *col = column(screen, 10);
        th_badge(col, SW_ICON_DOWN, TH_KEY, 56);
        th_wrap(col, "还没有应用", font(SW_FONT_HEADLINE), TH_TEXT, 250);
        th_wrap(col, "输入应用编号，下载到这台设备。", font(SW_FONT_BODY), TH_MUTED, 250);
        th_button(col, "去下载", TH_PRIMARY, ACT_DOWNLOAD);
        lv_obj_center(col);
    } else {
        lv_obj_t *grid = th_grid(screen);
        lv_obj_t *row = th_grid_row(grid);
        for (int i = 0; i < count; i++) {
            const app_info_t *app = &ui.apps[i];
            const app_icon_t *icon = app->damaged ? NULL : app_icon_find(app->play_id);
            lv_obj_t *tile = th_tile(row, app->damaged ? SW_ICON_WARNING : category_icon(app->category),
                                     app->damaged ? TH_KEY : name_color(app->project), icon ? &icon->large : NULL,
                                     app->damaged ? "已损坏" : app->title, NULL, 132, 136, i);
            lv_obj_add_event_cb(tile, app_long, LV_EVENT_LONG_PRESSED, NULL);
        }
        char text[96], used[16], pool[16];
        format_mb(used, sizeof used, space.pool - space.free);
        format_mb(pool, sizeof pool, space.pool);
        snprintf(text, sizeof text, "已用 %s / %s\n长按应用可卸载", used, pool);
        th_wrap(grid, text, font(SW_FONT_CAPTION), TH_MUTED, 240);
    }
    th_header(screen, "我的应用", SW_ACTION_NONE);
}

/* ------------------------------------------------------------ Download */

static void refresh_code(void) {
    size_t len = strlen(ui.code);
    for (int i = 0; i < 6; i++) {
        char text[2] = { i < (int)len ? ui.code[i] : 0, 0 };
        lv_label_set_text(ui.digit[i], text);
        bool cursor = i == (int)len;
        lv_obj_set_style_border_color(ui.box[i], lv_color_hex(cursor ? TH_ACCENT : TH_CARD), 0);
    }
    lv_obj_t *ok = lv_obj_get_parent(ui.ok_label);
    lv_obj_set_style_bg_color(ok, lv_color_hex(len ? TH_ACCENT : TH_CARD), 0);
    lv_obj_set_style_bg_color(ok, lv_color_hex(len ? 0xA83B22 : TH_KEY), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(ui.ok_label, lv_color_hex(len ? TH_ON_ACCENT : TH_DIM), 0);
}

static void download_click(lv_event_t *e) {
    int key = sw_event_action(e);
    size_t len = strlen(ui.code);
    if (key >= 0 && key <= 9) {
        if (len < 6) { ui.code[len] = (char)('0' + key); ui.code[len + 1] = 0; }
    } else if (key == KEY_BACKSPACE) {
        if (len) ui.code[len - 1] = 0;
    } else if (key == KEY_OK) {
        uint32_t id = (uint32_t)strtoul(ui.code, NULL, 10);
        if (id) lookup(id);
        return;
    } else if (key == ACT_HELP) {
        show_alert("应用编号在哪里？", "在 Sugarat 玩法社区打开一个应用，详情页上的编号就是它。前面的 0 可以不输，例如 2。",
                   "知道了", TH_PRIMARY, NULL, NULL);
        return;
    } else if (key == SW_ACTION_BACK) {
        navigate(build_home);
        return;
    } else {
        return;
    }
    refresh_code();
}

static void build_download(lv_obj_t *screen) {
    ui.view = VIEW_DOWNLOAD;
    th_header(screen, "下载应用", ACT_HELP);
    for (int i = 0; i < 6; i++) {
        ui.box[i] = th_box(screen, TH_CARD, 10);
        lv_obj_set_size(ui.box[i], 36, 44);
        lv_obj_set_pos(ui.box[i], 57 + i * 42, 88);
        lv_obj_set_style_border_width(ui.box[i], 2, 0);
        ui.digit[i] = th_text(ui.box[i], "", font(SW_FONT_NUM_S), TH_TEXT);
        lv_obj_center(ui.digit[i]);
    }
    lv_obj_t *hint = th_text(screen, "请输入应用编号", font(SW_FONT_CAPTION), TH_MUTED);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 138);
    static const int keys[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, KEY_BACKSPACE, 0, KEY_OK };
    for (int i = 0; i < 12; i++) {
        int k = keys[i];
        lv_obj_t *key = th_box(screen, k == KEY_OK ? TH_CARD : TH_KEY, 12);
        lv_obj_set_size(key, 64, 34);
        lv_obj_set_pos(key, 76 + (i % 3) * 72, 162 + (i / 3) * 40);
        th_tap(key, k);
        if (k == KEY_BACKSPACE) {
            lv_obj_center(th_text(key, LV_SYMBOL_BACKSPACE, &lv_font_montserrat_24, TH_MUTED));
        } else if (k == KEY_OK) {
            ui.ok_label = th_text(key, LV_SYMBOL_OK, &lv_font_montserrat_24, TH_DIM);
            lv_obj_center(ui.ok_label);
        } else {
            char text[2] = { (char)('0' + k), 0 };
            lv_obj_center(th_text(key, text, font(SW_FONT_NUM_S), TH_TEXT));
        }
    }
    lv_obj_add_event_cb(screen, download_click, LV_EVENT_CLICKED, NULL);
    refresh_code();
}

/* ------------------------------------------------------------ Settings */

static void remove_all_action(int action) {
    if (action == SW_ACTION_CONFIRM && launcher_job_uninstall(-1)) track_job("正在移除全部应用");
}

static void settings_click(lv_event_t *e) {
    switch (sw_event_action(e)) {
    case ACT_WIFI: open_wifi(); break;
    case ACT_CONTROL: sdgoods_cc_open(); break;
    case ACT_STORAGE: navigate(build_apps); break;
    case ACT_REMOVE_ALL:
        if (app_store_state() == APP_STORE_READY)
            show_alert("移除全部应用？", "所有已安装的应用和它们保存的数据都会删除，Wi-Fi 设置保留。",
                       "全部移除", TH_DANGER, "取消", remove_all_action);
        break;
    case SW_ACTION_BACK: navigate(build_home); break;
    default: break;
    }
}

static void wifi_poll(lv_timer_t *timer) {
    (void)timer;
    char ssid[33];
    launcher_wifi_ssid(ssid, sizeof ssid);
    if (strcmp(ssid, ui.wifi_seen)) navigate(build_settings);
}

static void build_settings(lv_obj_t *screen) {
    ui.view = VIEW_SETTINGS;
    lv_obj_t *row = th_grid_row(th_grid(screen));
    char storage[40], device[64], used[16], pool[16];
    launcher_wifi_ssid(ui.wifi_seen, sizeof ui.wifi_seen);
    th_tile(row, SW_ICON_WIFI, TH_VIOLET, NULL, "无线网络", ui.wifi_seen[0] ? ui.wifi_seen : "未连接", 128, 100, ACT_WIFI);
    th_tile(row, SW_ICON_SUN, TH_AMBER, NULL, "亮度音量", "控制中心", 128, 100, ACT_CONTROL);
    app_space_t space;
    load_apps(&space);
    format_mb(used, sizeof used, space.pool - space.free);
    format_mb(pool, sizeof pool, space.pool);
    snprintf(storage, sizeof storage, "%s / %s", used, pool);
    th_tile(row, SW_ICON_LIST, TH_GREEN, NULL, "存储空间", storage, 128, 100, ACT_STORAGE);
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(device, sizeof device, "v%s · %02X%02X%02X", esp_app_get_description()->version, mac[3], mac[4], mac[5]);
    th_tile(row, SW_ICON_INFO, TH_DIM, NULL, "设备信息", device, 128, 100, SW_ACTION_NONE);
    th_tile(row, SW_ICON_TRASH, TH_RED, NULL, "移除全部", "应用和数据", 128, 100, ACT_REMOVE_ALL);
    th_header(screen, "设置", SW_ACTION_NONE);
    lv_obj_add_event_cb(screen, settings_click, LV_EVENT_CLICKED, NULL);
    ui.timer = lv_timer_create(wifi_poll, 1500, NULL);
}

/* ------------------------------------------------------------ Home */

/* Refresh the status bar and the hero card; labels change only when the values do. */
static void home_poll(lv_timer_t *timer) {
    int wifi = launcher_wifi_connected();
    if (!timer || wifi != ui.home_wifi) {
        ui.home_wifi = wifi;
        lv_obj_set_style_text_color(ui.wifi_label, lv_color_hex(wifi ? TH_TEXT : TH_DIM), 0);
    }
    int battery = sdgoods_cc_bat_read(NULL);
    if (!timer || battery != ui.home_battery) {
        ui.home_battery = battery;
        char text[24];
        const char *symbol = battery < 0 ? LV_SYMBOL_BATTERY_EMPTY : battery > 80 ? LV_SYMBOL_BATTERY_FULL
                           : battery > 55 ? LV_SYMBOL_BATTERY_3 : battery > 30 ? LV_SYMBOL_BATTERY_2 : LV_SYMBOL_BATTERY_1;
        if (battery < 0) snprintf(text, sizeof text, "%s", symbol);
        else snprintf(text, sizeof text, "%s %d%%", symbol, battery);
        lv_label_set_text(ui.battery_label, text);
    }
    if (app_store_state() != APP_STORE_READY) return;
    app_space_t space;
    int count = load_apps(&space);
    if (timer && count == ui.home_count) return;
    ui.home_count = count;
    char text[40], free_text[16];
    snprintf(text, sizeof text, "%d", count);
    lv_label_set_text(ui.count_label, text);
    format_mb(free_text, sizeof free_text, space.free);
    snprintf(text, sizeof text, "剩余 %s", free_text);
    lv_label_set_text(ui.free_label, text);
    uint32_t used = space.pool ? (uint32_t)((uint64_t)(space.pool - space.free) * 100 / space.pool) : 0;
    sw_ring_set(ui.ring, (uint8_t)used);
    snprintf(text, sizeof text, "%lu%%", (unsigned long)used);
    lv_label_set_text(ui.percent, text);
}

static void home_click(lv_event_t *e) {
    switch (sw_event_action(e)) {
    case ACT_MY_APPS: navigate(build_apps); break;
    case ACT_DOWNLOAD: navigate(build_download); break;
    case ACT_SETTINGS: navigate(build_settings); break;
    default: break;
    }
}

static void home_gesture(lv_event_t *e) {
    (void)e;
    if (lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT) navigate(build_apps);
}

static void build_home(lv_obj_t *screen) {
    ui.view = VIEW_HOME;

    lv_obj_t *bar = th_plain(screen);
    lv_obj_set_size(bar, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bar, 10, 0);
    th_text(bar, "Sugarat", font(SW_FONT_CAPTION), TH_TEXT);
    ui.wifi_label = th_text(bar, LV_SYMBOL_WIFI, &lv_font_montserrat_14, TH_DIM);
    ui.battery_label = th_text(bar, "", &lv_font_montserrat_14, TH_TEXT);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 26);

    /* Hero: installed count and free space, with a ring for the used share of the app pool. */
    lv_obj_t *hero = th_box(screen, TH_HERO, 24);
    lv_obj_set_size(hero, 272, 104);
    lv_obj_set_pos(hero, 44, 54);
    th_tap(hero, ACT_MY_APPS);
    lv_obj_set_pos(th_text(hero, "已安装", font(SW_FONT_CAPTION), TH_MUTED), 22, 14);
    lv_obj_t *count = th_plain(hero);
    lv_obj_set_size(count, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(count, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(count, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(count, 4, 0);
    ui.count_label = th_text(count, "-", font(SW_FONT_NUM_M), TH_BRAND_TEXT);
    lv_obj_set_style_pad_bottom(th_text(count, "个应用", font(SW_FONT_BODY), TH_TEXT), 6, 0);
    lv_obj_set_pos(count, 22, 30);
    ui.free_label = th_text(hero, "", font(SW_FONT_CAPTION), TH_MUTED);
    lv_obj_set_pos(ui.free_label, 22, 76);
    ui.ring = sw_ring(hero, 76, 0, SW_TONE_ACCENT);
    lv_obj_clear_flag(ui.ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui.ring, LV_OBJ_FLAG_EVENT_BUBBLE);
    th_arc_colors(ui.ring, TH_BRAND, 0x5A3E31);
    lv_obj_set_style_arc_width(ui.ring, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui.ring, 8, LV_PART_INDICATOR);
    lv_obj_align(ui.ring, LV_ALIGN_RIGHT_MID, -16, 0);
    ui.percent = th_text(ui.ring, "", font(SW_FONT_CAPTION), TH_TEXT);
    lv_obj_set_style_text_align(ui.percent, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(ui.percent);

    static const char *const labels[3] = { "应用", "下载", "设置" };
    static const sw_icon_t icons[3] = { SW_ICON_LIST, SW_ICON_DOWN, SW_ICON_SETTINGS };
    static const uint32_t colors[3] = { TH_ACCENT, TH_VIOLET, TH_DIM };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *tile = th_tile(screen, icons[i], colors[i], NULL, labels[i], NULL, 88, 112, ACT_MY_APPS + i);
        lv_obj_set_pos(tile, 40 + i * 96, 168);
    }

    lv_obj_add_event_cb(screen, home_click, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screen, home_gesture, LV_EVENT_GESTURE, NULL);
    sdgoods_cc_bind(screen);
    ui.home_count = -1;
    home_poll(NULL);
    ui.timer = lv_timer_create(home_poll, 500, NULL);
}

/* ------------------------------------------------------------ entry points */

void launcher_ui_start(void) {
    ui.build = build_home;
    ui.old = NULL;
    lv_obj_t *initial = lv_scr_act();
    lv_obj_t *screen = th_screen();
    lv_scr_load(screen);
    if (initial) lv_obj_del(initial);
    build_home(screen);
}

void launcher_ui_home(void) {
    if (ui.view == VIEW_BUSY || ui.view == VIEW_PROGRESS || ui.view == VIEW_LAUNCH) return;
    navigate(build_home);
}

bool launcher_ui_power_short(void) {
    if (ui.alert) { close_alert(); return true; }
    switch (ui.view) {
    case VIEW_HOME: return false;
    case VIEW_APPS: case VIEW_DOWNLOAD: case VIEW_SETTINGS: navigate(build_home); return true;
    case VIEW_BUSY: case VIEW_PROGRESS: case VIEW_LAUNCH: return true;
    case VIEW_WIFI: navigate(build_settings); return true;
    case VIEW_PLAY: navigate(build_download); return true;
    case VIEW_RESULT: navigate(ui.result_back); return true;
    case VIEW_APP: navigate(build_apps); return true;
    }
    return false;
}
