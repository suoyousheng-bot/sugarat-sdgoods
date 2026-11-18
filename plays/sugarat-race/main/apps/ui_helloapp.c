/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Worker-owned physics and RGB565 frames; LVGL owns touch, menus and presentation. */
#include "ui_helloapp.h"
#include "sdgoods_board.h"
#include "sugarat_game.h"
#include "sugarat_imu.h"
#include "sugarat_frame.h"
#include "race/game.h"
#include "race/radio.h"
#include "race/sound.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

enum{PAGE_MODE,PAGE_MAP,PAGE_CALIBRATE,PAGE_LOBBY,PAGE_PLAY,PAGE_MENU,PAGE_RESULT,PAGE_ERROR};
enum{CMD_ENTER=1,CMD_SOLO,CMD_NEARBY,CMD_READY,CMD_MENU,CMD_RESUME,CMD_HOME,CMD_AGAIN,CMD_NEXT,CMD_PREVIOUS,CMD_BACK};
typedef struct{
    int page,mode,map,phase,countdown,elapsed,rank,room,peer_elapsed;
    bool motion,nearby,paired,ready,peer_ready,lost,peer_paused,radio_failed,defeated;
}view_state;
typedef struct{
    lv_obj_t *screen,*image,*overlay,*status,*counter;
    lv_img_dsc_t descriptor;
    SemaphoreHandle_t lock,stopped;
    QueueHandle_t commands;
    TaskHandle_t worker;
    bool stop,suspended,stream_failed;
    uint16_t *rendering,*ready,*visible,*maze_cache;
    unsigned produced,presented;
    view_state view;
    int shown_key,shown_count;
    sugarat_frame_stream_t *stream;
    sugarat_frame_pacer_t pacer;
    sugarat_frame_stats_t stats;
    int64_t last_status;
}race_app;
static race_app app;
static const char *TAG="sugarat_race";
static const char *const mode_names[]={"Tilt Racer","Marble Run"};
static const char *const ordinal[]={"1st","2nd","3rd","4th"};
static const char *const map_names[2][3]={{"Coast Cruise","Red Canyon","Neon Loop"},{"Jade Garden","Amber Ruins","Ice Corridor"}};
static void command(int action){
    if(!app.commands)return;
    if(xQueueSend(app.commands,&action,0)!=pdTRUE){ESP_LOGW(TAG,"Input queue full; action rejected");sg_toast(app.screen,SG_ICON_NONE,SG_TONE_ORANGE,"Try again later",1200);}
}
static void worker(void *argument){
    (void)argument;
    race_game game;race_game_begin(&game,0,0,false);
    race_radio radio={0};race_link link={0};
    view_state v={.page=PAGE_MODE,.rank=1};
    race_tilt tilt={0};float input_x=0,input_y=0;
    bool started=false;
    int heard_count=0,heard_rank=0;race_phase heard_phase=RACE_IDLE;int64_t last_wall=0;
    uint32_t samples=0;int64_t last_sample=0,previous=esp_timer_get_time(),last_render=0,report=previous;
    unsigned renders=0;int64_t render_cost=0,max_render=0;
    int rendered_page= -1,rendered_mode= -1,rendered_map= -1,rendered_phase= -1;
    for(;;){
        int64_t now=esp_timer_get_time();uint32_t ms=(uint32_t)(now/1000);
        xSemaphoreTake(app.lock,portMAX_DELAY);bool stop=app.stop,suspended=app.suspended;xSemaphoreGive(app.lock);
        if(stop)break;
        sugarat_imu_state_t imu;sugarat_imu_get_state(&imu);
        if(imu.samples!=samples){samples=imu.samples;last_sample=now;}
        v.motion=imu.running&&imu.sensor_ok&&last_sample&&now-last_sample<300000;
        int action;
        while(xQueueReceive(app.commands,&action,0)==pdTRUE){
            ESP_LOGI(TAG,"action=%d page=%d mode=%d map=%d",action,v.page,v.mode,v.map);
            race_sound_play(RACE_SFX_TICK,0);
            if(action==CMD_NEXT||action==CMD_PREVIOUS){
                int step=action==CMD_NEXT?1: -1;
                if(v.page==PAGE_MODE)v.mode=(v.mode+step+2)%2;
                else if(v.page==PAGE_MAP)v.map=(v.map+step+3)%3;
            }else if(action==CMD_ENTER&&v.page==PAGE_MODE)v.page=PAGE_MAP;
            else if((action==CMD_SOLO||action==CMD_NEARBY)&&v.page==PAGE_MAP){v.nearby=action==CMD_NEARBY;v.page=PAGE_CALIBRATE;v.radio_failed=false;}
            else if(action==CMD_READY&&v.page==PAGE_CALIBRATE&&v.motion){
                if(!race_tilt_calibrate(&tilt,imu.accel_g.x,imu.accel_g.y,imu.accel_g.z)){ESP_LOGW(TAG,"Calibration needs a steady hold");continue;}
                input_x=input_y=0;race_game_begin(&game,v.mode,v.map,!v.nearby);started=false;
                if(v.nearby){
                    esp_err_t err=race_radio_open(&radio);
                    if(err!=ESP_OK){v.radio_failed=true;v.page=PAGE_ERROR;continue;}
                    uint8_t mac[6];esp_read_mac(mac,ESP_MAC_WIFI_STA);
                    race_link_begin(&link,v.mode,v.map,mac,esp_random(),race_radio_send,&radio);v.page=PAGE_LOBBY;
                }else{v.page=PAGE_PLAY;started=true;}
            }else if(action==CMD_READY&&v.page==PAGE_LOBBY)link.ready=true;
            else if(action==CMD_MENU){
                if(v.page==PAGE_PLAY)v.page=PAGE_MENU;
                else if(v.page==PAGE_MAP)v.page=PAGE_MODE;
                else if(v.page==PAGE_CALIBRATE||v.page==PAGE_LOBBY){race_radio_close(&radio);memset(&link,0,sizeof link);v.page=PAGE_MAP;v.nearby=false;}
            }
            else if(action==CMD_RESUME&&v.page==PAGE_MENU)v.page=PAGE_PLAY;
            else if(action==CMD_AGAIN&&v.page==PAGE_RESULT){race_radio_close(&radio);memset(&link,0,sizeof link);v.page=PAGE_CALIBRATE;}
            else if(action==CMD_HOME){race_radio_close(&radio);memset(&link,0,sizeof link);v.nearby=false;v.page=PAGE_MODE;started=false;}
            else if(action==CMD_BACK){
                if(v.page==PAGE_MAP)v.page=PAGE_MODE;
                else if(v.page==PAGE_CALIBRATE||v.page==PAGE_LOBBY||v.page==PAGE_ERROR){race_radio_close(&radio);memset(&link,0,sizeof link);v.page=PAGE_MAP;v.nearby=false;}
            }
        }
        float dt=(now-previous)*.000001f;previous=now;if(dt>.05f)dt=.05f;
        if(link.enabled){
            race_radio_poll(&radio,&link,ms);link.paused=suspended||v.page==PAGE_MENU||!v.motion;race_link_tick(&link,&game,ms);
            if(link.committed&&!started){race_game_begin(&game,v.mode,v.map,false);v.page=PAGE_PLAY;started=true;}
            if(started&&game.phase==RACE_COUNTDOWN)game.countdown=fmaxf(0,(int32_t)(link.local_deadline-ms)*.001f);
        }
        bool playing=v.page==PAGE_PLAY&&v.motion&&!suspended;
        if(link.enabled)playing=playing&&!link.lost&&!link.peer_paused;
        if(playing&&started){
            race_tilt_read(&tilt,imu.accel_g.x,imu.accel_g.y,imu.accel_g.z,&input_x,&input_y);
            race_game_step(&game,dt,input_x,input_y);if(game.phase==RACE_FINISHED)v.page=PAGE_RESULT;
        }
        /* The first badge across the line wins: a peer finish ends this race as a defeat, even from the menu. */
        if(link.enabled&&started&&game.phase!=RACE_FINISHED&&(v.page==PAGE_PLAY||v.page==PAGE_MENU)&&race_link_peer_finished(&link)){
            game.phase=RACE_FINISHED;link.conceded=true;v.page=PAGE_RESULT;
        }
        game.frame.players=link.enabled?2:1+game.rival_count;game.frame.opponent_visible=link.paired&&!link.lost;
        game.frame.opponent_distance_m=link.remote.distance_m;game.frame.opponent_x=link.remote.marble_x;game.frame.opponent_y=link.remote.marble_y;
        int rank=link.enabled?race_link_rank(&link,&game):game.frame.rank;
        game.frame.rank=rank;
        bool in_race=started&&(v.page==PAGE_PLAY||v.page==PAGE_MENU||v.page==PAGE_RESULT);
        if(in_race){
            int count=(int)ceilf(game.countdown);
            if(game.phase==RACE_COUNTDOWN&&playing&&count!=heard_count&&count>=1&&count<=3)race_sound_play(RACE_SFX_COUNT,0);
            if(game.phase==RACE_RUNNING&&heard_phase==RACE_COUNTDOWN)race_sound_play(RACE_SFX_GO,0);
            if(game.events&RACE_EVENT_WALL&&now-last_wall>60000){race_sound_play(RACE_SFX_WALL,game.impact/250);last_wall=now;}
            if(game.events&RACE_EVENT_CHECKPOINT)race_sound_play(RACE_SFX_CHECKPOINT,0);
            if(game.events&RACE_EVENT_BOOST)race_sound_play(RACE_SFX_BOOST,0);
            if(game.events&RACE_EVENT_BUMP)race_sound_play(RACE_SFX_BUMP,game.impact/15);
            if(game.events&RACE_EVENT_LAP)race_sound_play(RACE_SFX_LAP,0);
            if(game.events&RACE_EVENT_FINAL_LAP)race_sound_play(RACE_SFX_FINAL_LAP,0);
            if(game.phase==RACE_RUNNING&&v.mode==0&&heard_rank&&rank<heard_rank)race_sound_play(RACE_SFX_OVERTAKE,0);
            if(game.phase==RACE_FINISHED&&heard_phase==RACE_RUNNING)race_sound_play(link.conceded?RACE_SFX_LOSE:rank==1?RACE_SFX_WIN:RACE_SFX_FINISH,0);
            heard_count=count;heard_phase=game.phase;heard_rank=rank;
        }else{heard_count=0;heard_phase=RACE_IDLE;heard_rank=0;}
        game.events=0;game.impact=0;
        race_synth_params sound={0};
        if(in_race&&playing&&game.phase!=RACE_FINISHED){
            if(v.mode==0){sound.engine=true;sound.engine_speed=game.speed/36.0f;sound.boost=game.boost>0;sound.offroad=game.phase==RACE_RUNNING&&fabsf(game.lateral)>3.0f;}
            else sound.roll=hypotf(game.vx,game.vy)/300.0f;
        }
        race_sound_set(&sound);
        v.phase=game.phase;v.countdown=(int)ceilf(game.countdown);v.elapsed=game.frame.elapsed_ms;v.rank=rank;v.defeated=link.conceded;v.peer_elapsed=link.remote.elapsed_ms;
        v.paired=link.paired;v.ready=link.ready;v.peer_ready=link.peer_ready;v.lost=link.lost;v.peer_paused=link.peer_paused;v.room=(int)((link.host?link.nonce:link.peer_nonce)%10000);
        xSemaphoreTake(app.lock,portMAX_DELAY);app.view=v;xSemaphoreGive(app.lock);
        bool animated=playing&&game.phase==RACE_RUNNING;
        bool scene_changed=v.page!=rendered_page||v.mode!=rendered_mode||v.map!=rendered_map||v.phase!=rendered_phase;
        if(!suspended&&(animated||scene_changed)&&now-last_render>=33333){
            rr_state frame;
            if(v.page==PAGE_PLAY||v.page==PAGE_MENU||v.page==PAGE_RESULT)frame=game.frame;
            else{frame=rr_reference_state(v.mode,v.map);frame.hide_hud=true;}
            int64_t begin=esp_timer_get_time();rr_render(app.rendering,&frame);
            for(int i=0;i<360*360;i++)app.rendering[i]=__builtin_bswap16(app.rendering[i]);
            int64_t cost=esp_timer_get_time()-begin;render_cost+=cost;if(cost>max_render)max_render=cost;renders++;
            xSemaphoreTake(app.lock,portMAX_DELAY);uint16_t *old=app.ready;app.ready=app.rendering;app.rendering=old;app.produced++;xSemaphoreGive(app.lock);last_render=now;
            rendered_page=v.page;rendered_mode=v.mode;rendered_map=v.map;rendered_phase=v.phase;
        }
        if(now-report>=5000000){
            ESP_LOGI(TAG,"page=%d mode=%d map=%d motion=%d imu=%.1fHz render=%.1ffps avg=%.1fms max=%.1fms heap=%u psram=%u",v.page,v.mode,v.map,v.motion,(double)imu.sample_hz,(double)renders*1000000/(now-report),renders?(double)render_cost/renders/1000:0,(double)max_render/1000,(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),(unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
            renders=0;render_cost=0;max_render=0;report=now;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    race_radio_close(&radio);sugarat_imu_stop();xSemaphoreGive(app.stopped);vTaskDelete(NULL);
}
static lv_obj_t *text(lv_obj_t *parent,const char *value,int y,sg_font_t role,sg_tone_t tone){
    lv_obj_t *label=sg_label(parent,value,role,tone);lv_obj_set_width(label,300);lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);lv_obj_align(label,LV_ALIGN_TOP_MID,0,y);return label;
}
static void button(lv_obj_t *parent,const char *title,int action,int y,bool primary){
    lv_obj_t *b=sg_button(parent,SG_ICON_NONE,title,primary?SG_BUTTON_FILLED:SG_BUTTON_TINTED,primary?SG_TONE_ACCENT:SG_TONE_TEAL);
    lv_obj_set_size(b,210,64);lv_obj_align(b,LV_ALIGN_TOP_MID,0,y);sg_set_action(b,action);
}
static void overlay(const view_state *v){
    lv_obj_clean(app.overlay);app.status=NULL;app.counter=NULL;app.shown_count= -1;
    bool is_play=v->page==PAGE_PLAY;
    if(is_play&&v->phase!=RACE_COUNTDOWN&&v->motion&&!v->lost&&!v->peer_paused){lv_obj_add_flag(app.overlay,LV_OBJ_FLAG_HIDDEN);return;}
    lv_obj_clear_flag(app.overlay,LV_OBJ_FLAG_HIDDEN);
    if(is_play){
        lv_obj_set_style_bg_opa(app.overlay,LV_OPA_TRANSP,0);
        if(v->phase==RACE_COUNTDOWN)app.counter=text(app.overlay,"3",140,SG_FONT_NUM_L,SG_TONE_ACCENT);
        else app.status=text(app.overlay,!v->motion?"Motion lost\nPlease wait":v->lost?"Disconnected\nHold for menu":"Rival paused\nPlease wait",137,SG_FONT_BODY,SG_TONE_ACCENT);
        return;
    }
    lv_obj_set_style_bg_color(app.overlay,sg_color(SG_TONE_BG),0);lv_obj_set_style_bg_opa(app.overlay,(v->page==PAGE_MODE||v->page==PAGE_MAP)?LV_OPA_70:LV_OPA_COVER,0);
    if(v->page==PAGE_MODE){
        text(app.overlay,"SUGARAT",60,SG_FONT_CAPTION,SG_TONE_TEAL);text(app.overlay,mode_names[v->mode],91,SG_FONT_TITLE,SG_TONE_TEXT);
        text(app.overlay,v->mode?"Roll to the flag":"3 laps + boosts",150,SG_FONT_CAPTION,SG_TONE_TEXT);text(app.overlay,"Swipe for mode",190,SG_FONT_CAPTION,SG_TONE_MUTED);
        button(app.overlay,"Play",CMD_ENTER,236,true);
        lv_obj_t *dots=sg_page_dots(app.overlay,2,(uint8_t)v->mode);lv_obj_align(dots,LV_ALIGN_BOTTOM_MID,0,-28);
    }else if(v->page==PAGE_MAP){
        text(app.overlay,map_names[v->mode][v->map],68,SG_FONT_BODY,SG_TONE_TEXT);char hint[60];snprintf(hint,sizeof hint,"Map %d/3 · swipe",v->map+1);text(app.overlay,hint,115,SG_FONT_CAPTION,SG_TONE_MUTED);
        button(app.overlay,"Solo",CMD_SOLO,164,true);button(app.overlay,"Versus",CMD_NEARBY,236,false);
    }else if(v->page==PAGE_CALIBRATE){
        text(app.overlay,"Hold steady",77,SG_FONT_BODY,SG_TONE_TEXT);text(app.overlay,"Flat or upright",137,SG_FONT_CAPTION,SG_TONE_MUTED);
        app.status=text(app.overlay,v->motion?"Motion ready":"Waiting motion",177,SG_FONT_CAPTION,v->motion?SG_TONE_TEAL:SG_TONE_ORANGE);button(app.overlay,"Start here",CMD_READY,236,true);
    }else if(v->page==PAGE_LOBBY){
        text(app.overlay,"Versus lobby",62,SG_FONT_BODY,SG_TONE_TEXT);text(app.overlay,"Same mode & map",109,SG_FONT_CAPTION,SG_TONE_MUTED);app.status=text(app.overlay,"Searching...",140,SG_FONT_CAPTION,SG_TONE_TEAL);
        button(app.overlay,v->ready?"Waiting...":"Ready",CMD_READY,164,true);button(app.overlay,"Back",CMD_BACK,236,false);
    }else if(v->page==PAGE_MENU){
        text(app.overlay,"Paused",70,SG_FONT_BODY,SG_TONE_TEXT);text(app.overlay,v->nearby?"Both paused":"Race paused",121,SG_FONT_CAPTION,SG_TONE_MUTED);button(app.overlay,"Resume",CMD_RESUME,164,true);button(app.overlay,"Exit",CMD_HOME,236,false);
    }else if(v->page==PAGE_RESULT){
        char value[80];const char *note="Your time";int time=v->elapsed;sg_tone_t tone=SG_TONE_ACCENT;
        if(v->defeated){snprintf(value,sizeof value,"Defeat - 2nd");note="Rival won in";time=v->peer_elapsed;tone=SG_TONE_ORANGE;}
        else if(v->nearby){snprintf(value,sizeof value,v->rank==1?"Victory! 1st":"Defeat - 2nd");note=v->rank==1?"You won in":"Your time";if(v->rank!=1)tone=SG_TONE_ORANGE;}
        else if(v->mode==0)snprintf(value,sizeof value,v->rank==1?"Champion! 1st":"Finished %s",ordinal[v->rank<1||v->rank>4?0:v->rank-1]);
        else snprintf(value,sizeof value,"Cleared!");
        text(app.overlay,value,60,SG_FONT_BODY,tone);text(app.overlay,note,100,SG_FONT_CAPTION,SG_TONE_MUTED);
        snprintf(value,sizeof value,"%02d:%02d.%02d",time/60000,time/1000%60,time/10%100);text(app.overlay,value,122,SG_FONT_NUM_S,SG_TONE_TEXT);button(app.overlay,"Play again",CMD_AGAIN,164,true);button(app.overlay,"Exit",CMD_HOME,236,false);
    }else{ text(app.overlay,"Versus offline",85,SG_FONT_BODY,SG_TONE_ORANGE);text(app.overlay,"Solo still works",145,SG_FONT_CAPTION,SG_TONE_MUTED);button(app.overlay,"Back",CMD_BACK,236,true);}
}
static void events(lv_event_t *e){
    lv_event_code_t code=lv_event_get_code(e);
    if(code==LV_EVENT_CLICKED){int action=sg_event_action(e);if(action!=SG_ACTION_NONE){command(action);return;}}
    if(code==LV_EVENT_LONG_PRESSED){
        lv_obj_t *target=lv_event_get_target(e);
        if(target==app.screen||target==app.overlay)command(CMD_MENU);
        return;
    }
    if(code!=LV_EVENT_GESTURE)return;
    lv_indev_t *input=lv_indev_get_act();if(!input)return;lv_dir_t dir=lv_indev_get_gesture_dir(input);
    if(dir==LV_DIR_LEFT)command(CMD_NEXT);else if(dir==LV_DIR_RIGHT)command(CMD_PREVIOUS);
}
static void pause_app(void){if(app.lock){xSemaphoreTake(app.lock,portMAX_DELAY);app.suspended=true;xSemaphoreGive(app.lock);}sdgoods_lvgl_fast_loop(false);}
static void resume_app(void){if(app.lock){xSemaphoreTake(app.lock,portMAX_DELAY);app.suspended=false;xSemaphoreGive(app.lock);}app.shown_key= -1;}
static void close_app(void){
    if(app.worker){xSemaphoreTake(app.lock,portMAX_DELAY);app.stop=true;xSemaphoreGive(app.lock);if(xSemaphoreTake(app.stopped,pdMS_TO_TICKS(4000))!=pdTRUE){ESP_LOGE(TAG,"Worker did not stop; retaining borrowed buffers");return;}app.worker=NULL;}
    race_sound_stop();
    sdgoods_lvgl_fast_loop(false);if(app.screen){lv_obj_del(app.screen);app.screen=NULL;}
    rr_set_maze_cache(NULL);heap_caps_free(app.maze_cache);app.maze_cache=NULL;
    heap_caps_free(app.rendering);heap_caps_free(app.ready);heap_caps_free(app.visible);app.rendering=app.ready=app.visible=NULL;
    if(app.commands){vQueueDelete(app.commands);app.commands=NULL;}if(app.lock){vSemaphoreDelete(app.lock);app.lock=NULL;}if(app.stopped){vSemaphoreDelete(app.stopped);app.stopped=NULL;}
}
void ui_helloapp_start(void){
    if(app.screen){lv_scr_load(app.screen);return;}
    sg_init();app.screen=sg_screen();app.shown_key= -1;app.stop=false;app.suspended=false;app.view=(view_state){0};app.produced=app.presented=0;
    app.rendering=heap_caps_malloc(360*360*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);app.ready=heap_caps_malloc(360*360*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);app.visible=heap_caps_calloc(360*360,2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    app.lock=xSemaphoreCreateMutex();app.stopped=xSemaphoreCreateBinary();app.commands=xQueueCreate(12,sizeof(int));
    if(!app.rendering||!app.ready||!app.visible||!app.lock||!app.stopped||!app.commands){text(app.screen,"Out of memory\nRestart device",130,SG_FONT_BODY,SG_TONE_ORANGE);lv_scr_load(app.screen);sdgoods_app_shell_bind(app.screen);sdgoods_app_shell_set_exit_cb(close_app);ESP_LOGE(TAG,"Frame allocation failed; game not started");return;}
    app.maze_cache=heap_caps_malloc(360*360*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);rr_set_maze_cache(app.maze_cache);
    if(!app.maze_cache)ESP_LOGW(TAG,"Maze cache unavailable; drawing every frame in full");
    app.descriptor=(lv_img_dsc_t){.header={.cf=LV_IMG_CF_TRUE_COLOR,.w=360,.h=360},.data_size=360*360*2,.data=(const uint8_t*)app.visible};
    app.image=lv_img_create(app.screen);lv_img_set_src(app.image,&app.descriptor);lv_obj_clear_flag(app.image,LV_OBJ_FLAG_CLICKABLE);
    app.overlay=sg_column(app.screen);lv_obj_set_layout(app.overlay,0);lv_obj_set_size(app.overlay,360,360);lv_obj_set_pos(app.overlay,0,0);lv_obj_clear_flag(app.overlay,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(app.overlay,LV_OBJ_FLAG_EVENT_BUBBLE|LV_OBJ_FLAG_GESTURE_BUBBLE);lv_obj_clear_flag(app.overlay,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(app.screen,events,LV_EVENT_ALL,NULL);lv_scr_load(app.screen);sdgoods_app_shell_bind(app.screen);sdgoods_app_shell_set_pause_cb(pause_app);sdgoods_app_shell_set_resume_cb(resume_app);sdgoods_app_shell_set_exit_cb(close_app);
    if(!app.stream&&!app.stream_failed){sugarat_frame_stream_config_t c=sugarat_frame_stream_default_config();c.sender_core=0;esp_err_t err=sugarat_frame_stream_create(&c,&app.stream);if(err!=ESP_OK){app.stream_failed=true;ESP_LOGW(TAG,"Direct frame unavailable: %s",esp_err_to_name(err));}}
    sugarat_frame_pacer_init(&app.pacer,33333);
    esp_err_t sound=race_sound_start();if(sound!=ESP_OK)ESP_LOGW(TAG,"Sound unavailable: %s; playing silently",esp_err_to_name(sound));
    sugarat_imu_config_t imu=sugarat_imu_default_config();imu.step_detection=false;imu.task_priority=8;esp_err_t err=sugarat_imu_start(&imu);if(err!=ESP_OK)ESP_LOGE(TAG,"IMU initialization failed: %s",esp_err_to_name(err));
    if(xTaskCreatePinnedToCore(worker,"race",8192,NULL,3,&app.worker,1)!=pdPASS){app.worker=NULL;sugarat_imu_stop();race_sound_stop();text(app.screen,"Start failed\nRestart device",130,SG_FONT_BODY,SG_TONE_ORANGE);ESP_LOGE(TAG,"Worker creation failed");return;}
    ESP_LOGI(TAG,"SUGARAT_RACE_READY version=0.2.1 maps=6");
}
static void fill_strip(uint16_t *pixels,int x1,int y,int x2,int rows,void *arg){const uint16_t *frame=arg;for(int r=0;r<rows;r++)memcpy(pixels+r*(x2-x1),frame+(y+r)*360+x1,(size_t)(x2-x1)*2);}
void ui_helloapp_poll(void){
    if(!app.worker||!app.lock||lv_scr_act()!=app.screen)return;
    int64_t now=esp_timer_get_time();view_state v;bool changed=false,suspended;
    xSemaphoreTake(app.lock,portMAX_DELAY);v=app.view;suspended=app.suspended;
    if(!suspended&&app.produced!=app.presented&&sugarat_frame_pacer_due(&app.pacer,now)){uint16_t *old=app.visible;app.visible=app.ready;app.ready=old;app.presented=app.produced;changed=true;}xSemaphoreGive(app.lock);
    if(suspended)return;
    int key=v.page+v.mode*16+v.map*64+v.phase*256+v.motion*1024+v.lost*2048+v.peer_paused*4096+v.ready*8192+v.rank*16384+v.defeated*131072;
    if(key!=app.shown_key){app.shown_key=key;overlay(&v);}
    if(app.counter&&app.shown_count!=v.countdown){char n[12];snprintf(n,sizeof n,"%d",v.countdown>0?v.countdown:1);lv_label_set_text(app.counter,n);app.shown_count=v.countdown;}
    if(app.status&&now-app.last_status>200000){if(v.page==PAGE_LOBBY){char hint[96];if(v.paired)snprintf(hint,sizeof hint,v.peer_ready?"Rival ready #%04d":"Rival in #%04d",v.room);else snprintf(hint,sizeof hint,"Searching...");lv_label_set_text(app.status,hint);}app.last_status=now;}
    bool clear=lv_obj_has_flag(app.overlay,LV_OBJ_FLAG_HIDDEN);sdgoods_lvgl_fast_loop(v.page==PAGE_PLAY);
    if(changed){
        app.descriptor.data=(const uint8_t*)app.visible;lv_img_cache_invalidate_src(&app.descriptor);int64_t start=esp_timer_get_time();bool direct=false;
        if(clear&&app.stream&&!app.stream_failed&&sugarat_frame_lvgl_layers_clear()&&sdgoods_touch_get_state()!=LV_INDEV_STATE_PRESSED&&!sdgoods_power_is_suspended()){
            esp_err_t err=sugarat_frame_stream_send(app.stream,0,0,360,360,fill_strip,app.visible);direct=err==ESP_OK;if(err!=ESP_OK){app.stream_failed=true;ESP_LOGE(TAG,"Direct frame stopped: %s; using LVGL",esp_err_to_name(err));}
        }
        if(!direct)lv_obj_invalidate(app.image);
        sugarat_frame_stats_record(&app.stats,start,esp_timer_get_time(),true,direct);sugarat_frame_pacer_finish(&app.pacer,esp_timer_get_time(),true);
    }
    sugarat_frame_summary_t summary;if(sugarat_frame_stats_window(&app.stats,now,5000000,&summary))ESP_LOGI(TAG,"present=%.1ffps direct=%lu/%lu max=%.1fms",(double)summary.fps,(unsigned long)summary.direct_frames,(unsigned long)summary.frames,(double)summary.max_us/1000);
}
