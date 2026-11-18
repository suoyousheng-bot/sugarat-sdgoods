/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** LVGL input owner; release, lost touch, overlays and screen suspension revoke transmission. */
#include "ui_helloapp.h"
#include "talk/talk_view.h"
#include "sdgoods_board.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_system.h"
static talk_view_t view;
static bool held,paused,swiping;
static lv_point_t swipe_start;
static uint8_t channel=1;
static int64_t updated;
static void publish(void){talk_radio_input(held,channel,paused);}
static void pause_app(void){held=false;paused=true;swiping=false;publish();}
static void resume_app(void){held=false;paused=false;swiping=false;publish();}
static void input(lv_event_t *e)
{
    lv_event_code_t code=lv_event_get_code(e);
    talk_status_t status;talk_radio_status(&status);
    lv_indev_t *device=lv_indev_get_act();lv_point_t point;
    if(code==LV_EVENT_RELEASED||code==LV_EVENT_PRESS_LOST){held=false;publish();return;}
    if(status.fault!=TALK_FAULT_NONE){
        if(code==LV_EVENT_CLICKED&&!paused&&device){
            lv_indev_get_point(device,&point);
            if(lv_obj_hit_test(view.ptt,&point)){talk_radio_stop();esp_restart();}
        }
        return;
    }
    if(code==LV_EVENT_PRESSED&&!paused&&!sdgoods_app_shell_menu_is_open()){
        held=true;swiping=false;publish();
    }else if(code==LV_EVENT_PRESSING&&held&&device){
        lv_indev_get_point(device,&point);
        if(!lv_obj_hit_test(view.ptt,&point)){held=false;publish();}
    }
}
/** A press belongs to speech or channel navigation until release, including after a drag-out. */
static void channel_swipe(lv_event_t *e)
{
    if(lv_event_get_target(e)!=view.screen)return;
    lv_indev_t *device=lv_indev_get_act();if(!device)return;
    lv_event_code_t code=lv_event_get_code(e);lv_point_t point;lv_indev_get_point(device,&point);
    if(code==LV_EVENT_PRESSED){
        swipe_start=point;swiping=!paused&&!sdgoods_power_is_suspended()&&
            !sdgoods_app_shell_menu_is_open()&&point.y>=50;
    }else if(code==LV_EVENT_PRESS_LOST){swiping=false;}
    else if(code==LV_EVENT_RELEASED){
        bool navigate=swiping;swiping=false;
        if(!navigate||paused||sdgoods_power_is_suspended()||sdgoods_app_shell_menu_is_open())return;
        talk_status_t status;talk_radio_status(&status);if(status.fault!=TALK_FAULT_NONE)return;
        int dx=point.x-swipe_start.x,dy=point.y-swipe_start.y;
        if(LV_ABS(dx)<56||LV_ABS(dx)<=2*LV_ABS(dy))return;
        held=false;
        channel=(uint8_t)(dx<0?(channel==TALK_CHANNELS?1:channel+1):(channel==1?TALK_CHANNELS:channel-1));
        publish();ESP_LOGI("squad_talk","Channel %u (swipe %s)",channel,dx<0?"left":"right");
    }
}
static void deleted(lv_event_t *e)
{
    (void)e;held=false;swiping=false;talk_radio_stop();sdgoods_lvgl_fast_loop(false);
    sdgoods_app_shell_set_pause_cb(NULL);sdgoods_app_shell_set_resume_cb(NULL);view.screen=NULL;
}
void ui_helloapp_start(void)
{
    if(view.screen){lv_scr_load(view.screen);return;}
    talk_view_create(&view);
    lv_obj_add_event_cb(view.ptt,input,LV_EVENT_ALL,NULL);
    lv_obj_add_flag(view.screen,LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(view.screen,channel_swipe,LV_EVENT_ALL,NULL);
    lv_obj_add_event_cb(view.screen,deleted,LV_EVENT_DELETE,NULL);
    lv_scr_load(view.screen);sdgoods_app_shell_bind(view.screen);
    sdgoods_app_shell_set_pause_cb(pause_app);sdgoods_app_shell_set_resume_cb(resume_app);
    esp_err_t err=talk_radio_start();if(err!=ESP_OK)ESP_LOGE("squad_talk","Radio unavailable: %s",esp_err_to_name(err));
    publish();
}
void ui_helloapp_poll(void)
{
    if(!view.screen)return;
    bool suspend=sdgoods_power_is_suspended()||sdgoods_app_shell_menu_is_open();
    if(suspend&&!paused)pause_app();else if(!suspend&&paused)resume_app();
    int64_t now=esp_timer_get_time();if(now-updated<50000)return;updated=now;
    talk_status_t status;talk_radio_status(&status);status.channel=channel;
    if(paused)status.phase=TALK_PAUSED;
    talk_view_update(&view,&status);sdgoods_lvgl_fast_loop(!paused);
}
/** Revoke PTT before the BSP releases power; shutdown never writes application slots. */
void sdgoods_app_pre_poweroff(void){held=false;paused=true;swiping=false;publish();talk_radio_stop();}
