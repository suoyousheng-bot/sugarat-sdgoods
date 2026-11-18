/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Firmware-lifetime owner. All simulation, storage and social transitions run on LVGL. */
#include "ui_helloapp.h"
#include "pet_view.h"
#include "pet_storage.h"
#include "pet_family_store.h"
#include "pet_family_nvs.h"
#include "pet_radio.h"
#include "pet_controls.h"
#include "pet_gestures.h"
#include "sdgoods_gesture.h"
#include "sdgoods_input.h"
#include "sdgoods_board.h"
#include "sdgoods_cc.h"
#include "sdgoods_app_sdk.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    pet_model_t model;
    pet_storage_t legacy;
    pet_family_t family;
    pet_family_store_t storage;
    pet_family_nvs_t nvs_store;
    pet_social_t social;
    pet_radio_t radio;
    pet_view_t view;
    uint16_t *pixels;
    uint64_t rendered_ms, action_save_ms;
    bool ready;
} pet_application_t;
static pet_application_t application;
static uint64_t now_ms(void) {return (uint64_t)esp_timer_get_time()/1000;}
static void close_nearby(pet_application_t *app)
{
    bool was_open=app->social.state!=PET_SOCIAL_OFF;
    pet_radio_close(&app->radio);pet_social_close(&app->social);
    if(was_open)app->social.notice="Nearby closed: return and tap Meet";
}
static bool save(void)
{
    pet_application_t *app=&application;
    if(app->view.starter)return true;
    pet_family_capture(&app->family,&app->model);
    sdgoods_touch_flash_window(3000);
    bool ok=pet_family_store_save(&app->storage,&app->family);
    sdgoods_touch_flash_window_end();
    if(ok) {app->model.dirty=false;app->model.save_elapsed_ms=0;app->action_save_ms=0;}
    pet_view_update(&app->view,app->storage.notice);return ok;
}
static bool social_save(void *context) {(void)context;return save();}
static bool radio_send(void *context,const uint8_t mac[6],const uint8_t *bytes,size_t size)
{return pet_radio_send(&((pet_application_t *)context)->radio,mac,bytes,size);}
static void input(void *context,pet_action_t action)
{
    pet_application_t *app=context;pet_view_t *v=&app->view;
    if(app->model.paused)return;
    switch(action) {
    case PET_FAMILY:
        close_nearby(app);v->page=PET_VIEW_FAMILY;v->selection=app->family.active;break;
    case PET_NEARBY:
        if(!app->storage.writable) {app->social.notice="Nearby needs writable family storage";app->social.state=PET_SOCIAL_FAULT;v->page=PET_VIEW_NEARBY;break;}
        pet_family_capture(&app->family,&app->model);close_nearby(app);
        v->page=PET_VIEW_NEARBY;v->selection=0;
        if(!pet_radio_open(&app->radio)) {app->social.state=PET_SOCIAL_FAULT;app->social.notice=app->radio.notice;break;}
        if(!pet_social_open(&app->social,&app->family,radio_send,social_save,app,now_ms()))pet_radio_close(&app->radio);
        break;
    case PET_NEXT:v->selection++;break;
    case PET_SELECT:
        if(v->page==PET_VIEW_NEARBY)pet_social_select(&app->social,app->social.nearby_count?v->selection%app->social.nearby_count:0);
        else if(v->starter) {
            pet_profile_t *p=&app->family.members[0].profile;p->blood[0]=p->blood[1]=(uint8_t)(v->selection%3);
            v->starter=false;v->page=PET_VIEW_CARE;save();
        } else {
            unsigned index=v->selection%(app->family.count+(unsigned)app->family.has_egg);
            if(index==app->family.count&&!app->family.pending.phase) {app->family.tending_egg=true;v->page=PET_VIEW_CARE;save();}
            else if(index<app->family.count&&pet_family_select(&app->family,&app->model,index)) {v->page=PET_VIEW_CARE;save();}
        }
        break;
    case PET_EGG:
        if(app->family.has_egg&&!app->family.pending.phase) {app->family.tending_egg=true;v->page=PET_VIEW_CARE;save();}break;
    case PET_CONFIRM:pet_social_confirm(&app->social);break;
    case PET_GREET:case PET_GIFT:case PET_TOGETHER:pet_social_interact(&app->social,(unsigned)(action-PET_GREET));break;
    case PET_BREED:pet_family_capture(&app->family,&app->model);pet_social_breed(&app->social);break;
    case PET_BACK:
        close_nearby(app);v->page=PET_VIEW_CARE;pet_model_action(&app->model,PET_BACK);break;
    default:
        if(app->family.tending_egg) {
            if(action==PET_TOUCH&&pet_family_hatch(&app->family,&app->model)) {
                app->model.emote=PET_TOUCH;app->model.emote_ms=2200;save();
            }
        } else if(pet_model_action(&app->model,action)&&app->model.dirty)app->action_save_ms=now_ms()+1500;
        break;
    }
    pet_view_update(v,app->storage.notice);
}
static void pause_pet(void)
{pet_gestures_dispose(&application.view);pet_controls_dismiss(&application.view);close_nearby(&application);pet_model_pause(&application.model,true,now_ms());save();}
static void resume_pet(void) {pet_model_pause(&application.model,false,now_ms());}
static bool power_short(void)
{if(sdgoods_cc_power_short())return true;return !save();}
esp_err_t __wrap_sdgoods_launcher_return_to_launcher(void)
{
    extern esp_err_t __real_sdgoods_launcher_return_to_launcher(void);
    if(application.ready&&!save()) {ESP_LOGE("sugarat_pet","Exit stopped: family save unconfirmed");return ESP_FAIL;}
    close_nearby(&application);return __real_sdgoods_launcher_return_to_launcher();
}
void sdgoods_app_pre_poweroff(void)
{close_nearby(&application);if(application.ready&&!save())ESP_LOGE("sugarat_pet","Power-off: family save unconfirmed");}
void ui_helloapp_start(void)
{
    pet_application_t *app=&application;
    if(app->view.screen) {lv_scr_load(app->view.screen);return;}
    pet_species_register();pet_model_init(&app->model,now_ms());pet_storage_open(&app->legacy,&app->model);
    pet_id_t id={{0}};bool identity_ok=pet_random_bytes(id.bytes,16)&&!pet_id_empty(id);
    pet_family_init(&app->family,&app->model,id);
    char directory[192];
    sdgoods_touch_flash_window(3000);
    bool mounted=sdgoods_appdata_begin("sugarat_pet",directory,sizeof(directory))==ESP_OK;
    sdgoods_touch_flash_window_end();
    pet_family_nvs_open(&app->nvs_store,&app->storage,&app->family);
    if(!app->nvs_store.present&&mounted) {
        pet_family_nvs_close(&app->nvs_store);pet_family_store_open(&app->storage,directory,&app->family);
    }
    if(!app->storage.loaded&&(!app->legacy.writable||!identity_ok)) {
        app->storage.writable=false;app->storage.notice=!identity_ok?"Identity unavailable: restart":app->legacy.notice;
    }
    if(app->storage.loaded)pet_family_restore(&app->family,&app->model);
    bool starter=!app->storage.loaded&&!app->legacy.sequence&&app->storage.writable;
    pet_storage_close(&app->legacy);
    app->pixels=heap_caps_malloc(PET_WORLD_SIZE*PET_WORLD_SIZE*sizeof(uint16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!app->pixels) {
        pet_family_nvs_close(&app->nvs_store);
        lv_obj_t *screen=lv_obj_create(NULL),*label=lv_label_create(screen);lv_label_set_text(label,"Pet could not start\nRestart the device");lv_obj_center(label);
        lv_scr_load(screen);sdgoods_app_shell_bind(screen);ESP_LOGE("sugarat_pet","Pixel allocation failed");return;
    }
    pet_view_create(&app->view,&app->model,app->pixels,input,app);pet_view_attach(&app->view,&app->family,&app->social);
    app->view.starter=starter;if(starter)app->view.page=PET_VIEW_FAMILY;
    lv_scr_load(app->view.screen);sdgoods_app_shell_bind(app->view.screen);sdgoods_app_shell_set_pause_cb(pause_pet);sdgoods_app_shell_set_resume_cb(resume_pet);
    sdgoods_set_power_short_handler(power_short);app->ready=true;
    if(!starter&&app->storage.writable)save();else pet_view_update(&app->view,app->storage.notice);
    ESP_LOGI("sugarat_pet","SUGARAT_PET_READY version=0.2.2 family=%u pending=%u launcher_mode=%s",app->family.count,app->family.pending.phase,sdgoods_device_mode_str());
}
void ui_helloapp_poll(void)
{
    pet_application_t *app=&application;if(!app->ready)return;
    uint64_t now=now_ms(),elapsed=now>=app->model.last_ms?now-app->model.last_ms:0;
    uint32_t dt=elapsed>1000?1000:(uint32_t)elapsed;
    if(!app->model.paused) {
        if(app->family.tending_egg||app->view.starter) {app->model.last_ms=now;app->model.animation_ms+=dt;if(!app->view.starter)app->model.save_elapsed_ms+=dt;}
        else pet_model_tick(&app->model,now);
        if(!app->view.starter)pet_family_tick(&app->family,dt);
        pet_radio_poll(&app->radio,&app->social);pet_social_tick(&app->social,now);
        if(app->social.state==PET_SOCIAL_FAULT&&app->radio.wifi_owned) {
            const char *notice=app->social.notice;close_nearby(app);app->social.state=PET_SOCIAL_FAULT;app->social.notice=notice;
        }
        if(!app->view.starter&&app->storage.writable&&(app->model.save_elapsed_ms>=30000||(app->action_save_ms&&now>=app->action_save_ms)))save();
    } else app->model.last_ms=now;
    if(now-app->rendered_ms>=80&&!app->model.paused) {app->rendered_ms=now;pet_view_update(&app->view,app->storage.notice);}
}
