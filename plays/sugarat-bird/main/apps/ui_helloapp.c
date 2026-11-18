/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** LVGL-thread game controller; shell overlays and screen suspension pause flight. */
#include "ui_helloapp.h"
#include "bird/bird_model.h"
#include "bird/bird_storage.h"
#include "bird/bird_view.h"
#include "sdgoods_board.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>
static struct { bird_model_t game; bird_storage_t storage; bird_view_t view; int64_t previous, draw_at; bool suspended; } app;
static void save_best(void)
{
    if(!app.storage.writable||app.game.best<=app.storage.saved)return;
    sdgoods_touch_flash_window(1000);
    bird_storage_save(&app.storage,app.game.best);
    sdgoods_touch_flash_window_end();
}
static void pause_game(void)
{
    if(!app.view.screen)return;
    if(app.game.phase==BIRD_PLAYING)app.game.paused=true;
    app.previous=esp_timer_get_time();save_best();bird_view_update(&app.view,app.storage.failed);
}
static void resume_game(void)
{
    app.previous=esp_timer_get_time();
    if(app.view.screen)bird_view_update(&app.view,app.storage.failed);
}
static void deleted(lv_event_t *e)
{
    (void)e;
    sdgoods_lvgl_fast_loop(false);
    sdgoods_app_shell_set_pause_cb(NULL);sdgoods_app_shell_set_resume_cb(NULL);
    bird_storage_close(&app.storage);memset(&app,0,sizeof(app));
}
static void action(lv_event_t *e)
{
    (void)e;
    if(app.suspended||sdgoods_app_shell_menu_is_open())return;
    if(app.game.paused)app.game.paused=false;else bird_model_start(&app.game);
    app.previous=esp_timer_get_time();bird_view_update(&app.view,app.storage.failed);
}
static void pressed(lv_event_t *e)
{
    if(lv_event_get_target(e)!=app.view.screen||app.suspended||sdgoods_app_shell_menu_is_open())return;
    lv_indev_t *input=lv_indev_get_act();if(!input)return;
    lv_point_t point;lv_indev_get_point(input,&point);
    if(point.x<60||point.x>=300||point.y<60||point.y>=300)return;
    bird_model_flap(&app.game);
}
void ui_helloapp_start(void)
{
    if(app.view.screen){lv_scr_load(app.view.screen);return;}
    uint32_t best=bird_storage_open(&app.storage);
    bird_model_init(&app.game,best,(uint32_t)esp_timer_get_time());
    bird_view_create(&app.view,&app.game);
    bird_view_update(&app.view,app.storage.failed);
    lv_obj_add_event_cb(app.view.button,action,LV_EVENT_CLICKED,NULL);
    lv_obj_add_event_cb(app.view.screen,pressed,LV_EVENT_PRESSED,NULL);
    lv_obj_add_event_cb(app.view.screen,deleted,LV_EVENT_DELETE,NULL);
    lv_scr_load(app.view.screen);sdgoods_app_shell_bind(app.view.screen);
    sdgoods_app_shell_set_pause_cb(pause_game);sdgoods_app_shell_set_resume_cb(resume_game);
    app.previous=esp_timer_get_time();
    ESP_LOGI("sugarat_bird","SUGARAT_BIRD_READY version=0.1.0 language=en");
}
void ui_helloapp_poll(void)
{
    if(!app.view.screen)return;
    int64_t now=esp_timer_get_time();bool suspended=sdgoods_power_is_suspended();
    if(suspended&&!app.suspended)pause_game();
    if(suspended){app.suspended=true;app.previous=now;sdgoods_lvgl_fast_loop(false);return;}
    app.suspended=false;
    int64_t elapsed=(now-app.previous)/1000;
    if(elapsed<1)return;
    app.previous+=elapsed*1000;
    if(elapsed>250&&app.game.phase==BIRD_PLAYING){app.game.paused=true;bird_view_update(&app.view,app.storage.failed);}
    bird_phase_t before=app.game.phase;
    bird_model_tick(&app.game,(uint32_t)(elapsed>100?100:elapsed));
    if(before==BIRD_PLAYING&&app.game.phase==BIRD_OVER)save_best();
    bool running=app.game.phase==BIRD_PLAYING&&!app.game.paused;
    sdgoods_lvgl_fast_loop(running);
    if((running&&now-app.draw_at>=33000)||before!=app.game.phase){bird_view_update(&app.view,app.storage.failed);app.draw_at=now;}
}
/** Save the highest score before the BSP releases the device power latch. */
void sdgoods_app_pre_poweroff(void){save_best();}
