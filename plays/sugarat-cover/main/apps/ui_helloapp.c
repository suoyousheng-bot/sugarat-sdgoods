/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Firmware-lifetime owner. Only this thread creates and updates LVGL objects. */
#include "ui_helloapp.h"
#include "cv_device.h"
#include "cv_view.h"
#include "sdgoods_board.h"
#include "esp_timer.h"
#include "esp_app_desc.h"
#include "esp_log.h"
static struct {cv_device_t device;cv_view_t view;lv_obj_t *fault;bool attempted,started,paused;} app;
static void pause_app(void){app.paused=true;cv_view_pause(&app.view,true);cv_device_suspend(&app.device);sdgoods_lvgl_fast_loop(false);}
static void resume_app(void){app.paused=false;cv_view_pause(&app.view,false);}
void ui_helloapp_start(void)
{
    if(app.started){lv_scr_load(app.view.screen);return;}
    if(app.attempted){if(app.fault)lv_scr_load(app.fault);return;}
    app.attempted=true;cv_backend_t backend;
    if(!cv_device_start(&app.device,&backend)||!cv_view_open(&app.view,backend)) {
        lv_obj_t *screen=lv_obj_create(NULL),*label=lv_label_create(screen);app.fault=screen;
        lv_label_set_text(label,"Sugarat Cover could not start.\nRestart the badge.");lv_obj_center(label);lv_scr_load(screen);sdgoods_app_shell_bind(screen);
        ESP_LOGE("sugarat_cover","Application initialization failed");return;
    }
    app.started=true;lv_scr_load(app.view.screen);sdgoods_app_shell_bind(app.view.screen);
    sdgoods_app_shell_set_pause_cb(pause_app);sdgoods_app_shell_set_resume_cb(resume_app);
    cv_view_tick(&app.view,(uint64_t)esp_timer_get_time()/1000);
    ESP_LOGI("sugarat_cover","SUGARAT_COVER_READY version=%s photos=%u locale=%u",esp_app_get_description()->version,app.device.store.library.count,app.device.store.library.locale);
}
void ui_helloapp_poll(void)
{
    if(!app.started)return;
    bool paused=sdgoods_power_is_suspended()||sdgoods_app_shell_menu_is_open();
    if(paused&&!app.paused)pause_app();else if(!paused&&app.paused)resume_app();
    if(!app.paused)cv_view_tick(&app.view,(uint64_t)esp_timer_get_time()/1000);
    sdgoods_lvgl_fast_loop(app.view.animating&&!app.paused);
}
void sdgoods_app_pre_poweroff(void){if(app.started)pause_app();}
