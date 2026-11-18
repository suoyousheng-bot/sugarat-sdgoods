/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Real flight, NVS failures, pointer input, pause ownership and round-screen rendering. */
#include "bird/bird_model.h"
#include "bird/bird_storage.h"
#include "bird/bird_view.h"
#include "ui_helloapp.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {uint32_t best;bool exists,fail_open,fail_read,fail_write,fail_commit,fail_readback;unsigned writes,commits,handles;} disk;
void sdgoods_nvs_ensure(void){}
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *h)
{assert(!strcmp(name,"sugarat_bird")&&mode==NVS_READWRITE);if(disk.fail_open)return ESP_FAIL;*h=1;disk.handles++;return ESP_OK;}
esp_err_t nvs_get_u32(nvs_handle_t h,const char *key,uint32_t *out)
{assert(h==1&&!strcmp(key,"best"));if(disk.fail_read||(disk.fail_readback&&disk.commits))return ESP_FAIL;if(!disk.exists)return ESP_ERR_NVS_NOT_FOUND;*out=disk.best;return ESP_OK;}
esp_err_t nvs_set_u32(nvs_handle_t h,const char *key,uint32_t value)
{assert(h==1&&!strcmp(key,"best"));if(disk.fail_write)return ESP_FAIL;disk.best=value;disk.exists=true;disk.writes++;return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h){assert(h==1);disk.commits++;return disk.fail_commit?ESP_FAIL:ESP_OK;}
void nvs_close(nvs_handle_t h){assert(h==1&&disk.handles);disk.handles--;}
static void storage_tests(void)
{
    bird_storage_t s={0};assert(bird_storage_open(&s)==0&&s.writable&&!s.failed);
    assert(bird_storage_save(&s,9)&&s.saved==9&&disk.writes==1);
    assert(bird_storage_save(&s,8)&&disk.writes==1);bird_storage_close(&s);
    s=(bird_storage_t){0};assert(bird_storage_open(&s)==9);bird_storage_close(&s);
    for(unsigned fault=0;fault<6;fault++){
        memset(&disk,0,sizeof(disk));s=(bird_storage_t){0};
        disk.fail_open=fault==0;disk.fail_read=fault==1;
        if(fault==5){disk.exists=true;disk.best=10000;}
        bird_storage_open(&s);disk.fail_write=fault==2;disk.fail_commit=fault==3;disk.fail_readback=fault==4;
        assert(!bird_storage_save(&s,12)&&s.failed&&s.saved==0);
        if(fault==0||fault==1||fault==5)assert(disk.writes==0);
        bird_storage_close(&s);assert(disk.handles==0);
    }
    memset(&disk,0,sizeof(disk));
    puts("PASS: highest score restore, no redundant writes, invalid-data preservation and all storage failure paths.");
}
static void model_tests(void)
{
    bird_model_t a,b;bird_model_init(&a,7,42);bird_model_start(&a);b=a;
    for(unsigned i=0;i<20;i++){if(i%10==0){bird_model_flap(&a);bird_model_flap(&b);}bird_model_tick(&a,16);bird_model_tick(&b,8);bird_model_tick(&b,8);}
    assert(a.y==b.y&&a.velocity==b.velocity&&a.elapsed_ms==b.elapsed_ms);
    a.paused=true;b=a;bird_model_tick(&a,100);bird_model_flap(&a);assert(!memcmp(&a,&b,sizeof(a)));
    bird_model_init(&a,7,42);bird_model_start(&a);for(unsigned i=0;i<1000&&a.phase==BIRD_PLAYING;i++)bird_model_tick(&a,8);
    assert(a.phase==BIRD_OVER&&a.best==7);bird_model_start(&a);assert(a.phase==BIRD_PLAYING&&a.score==0&&a.best==7);
    a.y=50;a.velocity=0;a.pipes[0]=(bird_pipe_t){.x=BIRD_X,.center=140,.gap=60};bird_model_tick(&a,8);assert(a.phase==BIRD_OVER);
    bird_model_start(&a);a.y=70;a.velocity=0;a.pipes[0]=(bird_pipe_t){.x=BIRD_X-BIRD_PIPE_W-1,.center=84,.gap=88};bird_model_tick(&a,8);assert(a.score==1);bird_model_tick(&a,8);assert(a.score==1);
    bird_model_init(&a,0,4242);bird_model_start(&a);
    for(unsigned tick=0;tick<100000&&a.score<45;tick++){
        float target=84,nearest=10000;
        for(unsigned i=0;i<BIRD_PIPE_COUNT;i++)if(a.pipes[i].x+BIRD_PIPE_W>=BIRD_X&&a.pipes[i].x<nearest){nearest=a.pipes[i].x;target=a.pipes[i].center;}
        if(a.y+BIRD_SIZE*.5f>target+8&&a.velocity>0)bird_model_flap(&a);
        bird_model_tick(&a,8);assert(a.phase==BIRD_PLAYING);
        for(unsigned i=0;i<BIRD_PIPE_COUNT;i++)assert(a.pipes[i].center-a.pipes[i].gap*.5f>=11&&a.pipes[i].center+a.pipes[i].gap*.5f<=BIRD_FIELD_H-11);
    }
    assert(a.score>=45&&bird_model_level(&a)==8&&a.best==a.score);
    puts("PASS: frame-rate independence, pause, boundary/pipe collision, restart, one score per pipe and 45-pipe difficulty/recycling run.");
}
uint64_t host_time_ms;
static void (*on_pause)(void),(*on_resume)(void);
static bool suspended,menu,fast;
void sdgoods_app_shell_bind(lv_obj_t *screen){assert(screen);}
void sdgoods_app_shell_set_pause_cb(void (*cb)(void)){on_pause=cb;}
void sdgoods_app_shell_set_resume_cb(void (*cb)(void)){on_resume=cb;}
bool sdgoods_app_shell_menu_is_open(void){return menu;}
bool sdgoods_power_is_suspended(void){return suspended;}
void sdgoods_lvgl_fast_loop(bool value){fast=value;}
void sdgoods_touch_flash_window(uint32_t ms){assert(ms==1000);}
void sdgoods_touch_flash_window_end(void){}
static lv_point_t pointer;static bool pressed;
static void read_pointer(lv_indev_drv_t *d,lv_indev_data_t *data){(void)d;data->point=pointer;data->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void flush(lv_disp_drv_t *d,const lv_area_t *a,lv_color_t *p){(void)a;(void)p;lv_disp_flush_ready(d);}
static void pump(unsigned duration,bool app)
{for(unsigned i=0;i<duration;i+=10){host_time_ms+=10;lv_tick_inc(10);if(app)ui_helloapp_poll();lv_timer_handler();}}
static void tap(int x,int y,unsigned duration)
{pointer=(lv_point_t){x,y};pressed=true;pump(duration,true);pressed=false;pump(60,true);}
static void labels(lv_obj_t *o,FILE *text)
{
    if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))return;
    if(lv_obj_check_type(o,&lv_label_class)){
        const char *s=lv_label_get_text(o);const lv_font_t *font=lv_obj_get_style_text_font(o,0);
        for(const unsigned char *p=(const unsigned char *)s;*p;p++){assert(*p>=32&&*p<127);lv_font_glyph_dsc_t glyph;assert(lv_font_get_glyph_dsc(font,&glyph,*p,0));}
        lv_area_t a;lv_obj_get_coords(o,&a);
        if(*s&&!(a.x1>=60&&a.x2<300&&a.y1>=60&&a.y2<300)){fprintf(stderr,"OUTSIDE %s %d,%d..%d,%d\n",s,a.x1,a.y1,a.x2,a.y2);abort();}
        if(*s)fprintf(text,"%s\n",s);
    }
    for(unsigned i=0;i<lv_obj_get_child_cnt(o);i++)labels(lv_obj_get_child(o,i),text);
}
static bool has_text(lv_obj_t *o,const char *text)
{if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))return false;if(lv_obj_check_type(o,&lv_label_class)&&!strcmp(lv_label_get_text(o),text))return true;for(unsigned i=0;i<lv_obj_get_child_cnt(o);i++)if(has_text(lv_obj_get_child(o,i),text))return true;return false;}
static void shot(lv_obj_t *screen,const char *directory,const char *name,FILE *text)
{
    lv_obj_update_layout(screen);fprintf(text,"[%s]\n",name);labels(screen,text);
    uint32_t bytes=lv_snapshot_buf_size_needed(screen,LV_IMG_CF_TRUE_COLOR);void *buffer=malloc(bytes);assert(buffer);lv_img_dsc_t image;
    assert(lv_snapshot_take_to_buf(screen,LV_IMG_CF_TRUE_COLOR,&image,buffer,bytes)==LV_RES_OK);
    char path[1024];snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n360 360\n255\n");
    for(unsigned i=0;i<360*360;i++){unsigned c=image.data[i*2]<<8|image.data[i*2+1];uint8_t rgb[]={(c>>11)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(rgb,1,3,f);}fclose(f);free(buffer);
}
static void ui_tests(const char *out)
{
    lv_init();static lv_color_t pixels[360*12];lv_disp_draw_buf_t draw;lv_disp_draw_buf_init(&draw,pixels,NULL,360*12);
    lv_disp_drv_t d;lv_disp_drv_init(&d);d.hor_res=d.ver_res=360;d.draw_buf=&draw;d.flush_cb=flush;lv_disp_t *display=lv_disp_drv_register(&d);assert(display);
    lv_indev_drv_t input;lv_indev_drv_init(&input);input.type=LV_INDEV_TYPE_POINTER;input.read_cb=read_pointer;lv_indev_t *device=lv_indev_drv_register(&input);assert(device);
    lv_obj_t *blank=lv_obj_create(NULL);lv_scr_load(blank);pump(60,false);
    char path[1024];snprintf(path,sizeof(path),"%s/ui-text.txt",out);FILE *text=fopen(path,"w");assert(text);
    ui_helloapp_start();pump(60,true);lv_obj_t *screen=lv_scr_act();assert(has_text(screen,"Play")&&on_pause&&on_resume);shot(screen,out,"ready",text);
    tap(180,252,60);assert(fast&&!has_text(screen,"Play"));pump(350,true);tap(180,180,60);pump(300,true);tap(180,180,60);pump(150,true);shot(screen,out,"playing",text);
    menu=true;on_pause();pump(3000,true);assert(has_text(screen,"Paused")&&!fast);on_resume();menu=false;shot(screen,out,"paused",text);
    tap(180,252,60);assert(!has_text(screen,"Paused")&&fast);
    suspended=true;pump(2000,true);suspended=false;pump(60,true);assert(has_text(screen,"Paused"));tap(180,252,60);pump(2200,true);assert(has_text(screen,"Game Over"));shot(screen,out,"over",text);
    tap(180,252,60);assert(fast&&!has_text(screen,"Game Over"));tap(180,180,800);pump(2000,true);assert(has_text(screen,"Game Over"));
    lv_scr_load(blank);lv_obj_del(screen);pump(60,false);assert(disk.handles==0&&!on_pause&&!on_resume);
    disk.fail_read=true;ui_helloapp_start();pump(60,true);screen=lv_scr_act();assert(has_text(screen,"Best not saved"));shot(screen,out,"save-error",text);
    lv_scr_load(blank);lv_obj_del(screen);disk.fail_read=false;pump(60,false);
    bird_model_t maximum;bird_model_init(&maximum,9999,1);maximum.score=9999;maximum.phase=BIRD_OVER;
    bird_view_t maximum_view={0};bird_view_create(&maximum_view,&maximum);lv_scr_load(maximum_view.screen);pump(60,false);shot(maximum_view.screen,out,"high-score",text);fclose(text);
    lv_scr_load(blank);lv_obj_del(maximum_view.screen);pump(60,false);
    _lv_font_clean_up_fmt_txt();lv_mem_buf_free_all();lv_mem_monitor_t before,after;lv_mem_monitor(&before);
    for(unsigned i=0;i<5;i++){ui_helloapp_start();pump(60,true);screen=lv_scr_act();lv_scr_load(blank);lv_obj_del(screen);pump(60,false);_lv_font_clean_up_fmt_txt();lv_mem_buf_free_all();lv_mem_monitor(&after);assert(after.free_size==before.free_size&&disk.handles==0);}
    printf("PASS: real LVGL pointer input, held press, overlay/sleep pause, explicit resume, English glyphs/bounds and repeated teardown; free=%u.\n",after.free_size);
    lv_indev_delete(device);lv_disp_remove(display);lv_deinit();
}
int main(int argc,char **argv){assert(argc==2);model_tests();storage_tests();ui_tests(argv[1]);return 0;}
