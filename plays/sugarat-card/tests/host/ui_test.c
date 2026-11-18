/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Real LVGL pointer dispatch, BSP guarded taps and left-edge navigation. */
#include "card_view.h"
#include "sdgoods_swipe_back.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint64_t host_time_ms;
static card_view_t *view;
static lv_point_t pointer;
static bool down;
static unsigned actions[CARD_ACT_COUNT],backs;
static const char *directory;
static FILE *snapshot_text;
void sdgoods_lvgl_post(void (*fn)(void *),void *context){fn(context);}
void sdgoods_touch_synth_report(lv_indev_state_t state,int x,int y){(void)state;(void)x;(void)y;}
static void flush(lv_disp_drv_t *drv,const lv_area_t *area,lv_color_t *data){(void)area;(void)data;lv_disp_flush_ready(drv);}
static void read_pointer(lv_indev_drv_t *drv,lv_indev_data_t *data){(void)drv;data->point=pointer;data->state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void pump(unsigned duration){for(unsigned i=0;i<duration;i+=10){host_time_ms+=10;lv_tick_inc(10);lv_timer_handler();}}
static void tap(int x,int y){pointer=(lv_point_t){x,y};down=true;pump(80);down=false;pump(240);}
static void move(int x,int y,int to_x,int to_y)
{
    pointer=(lv_point_t){x,y};down=true;pump(60);
    for(int i=1;i<=10;++i){pointer=(lv_point_t){x+(to_x-x)*i/10,y+(to_y-y)*i/10};pump(30);}
    down=false;pump(240);
}
static void back(void){++backs;if(view->visible)card_view_drawer(view,false);else{view->page=CARD_PAGE_HOME;card_view_refresh(view);}}
static void action(void *context,card_action_t action)
{
    card_view_t *v=context;++actions[action];if(action!=CARD_ACT_LANGUAGE)card_view_pause(v,false);
    switch(action){case CARD_ACT_QR:v->page=CARD_PAGE_QR;break;case CARD_ACT_EXCHANGE:v->page=CARD_PAGE_NEARBY;break;
    case CARD_ACT_EDIT:v->page=CARD_PAGE_EDIT;break;case CARD_ACT_CONTACTS:v->page=CARD_PAGE_CONTACTS;break;
    case CARD_ACT_LANGUAGE:v->language=v->language==CARD_ZH?CARD_EN:CARD_ZH;break;case CARD_ACT_EDIT_SECURITY:v->password=v->password&&v->password[0]?"":"12345678";break;case CARD_ACT_NEXT:++v->selected;break;case CARD_ACT_PREV:--v->selected;break;default:break;}card_view_refresh(v);
}
static unsigned action_count(void){unsigned n=0;for(unsigned i=0;i<CARD_ACT_COUNT;++i)n+=actions[i];return n;}
static void visible_text(lv_obj_t *o)
{
    if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))return;
    if(lv_obj_check_type(o,&lv_label_class)) {
        const char *s=lv_label_get_text(o);uint32_t i=0;const lv_font_t *f=lv_obj_get_style_text_font(o,0);
        while(s[i]){uint32_t cp=_lv_txt_encoded_next(s,&i);lv_font_glyph_dsc_t d;if(!lv_font_get_glyph_dsc(f,&d,cp,0)){fprintf(stderr,"Missing U+%04X: %s\n",cp,s);abort();}}
        if(snapshot_text&&s[0])fprintf(snapshot_text,"%s\n",s);
    }
    for(unsigned i=0;i<lv_obj_get_child_cnt(o);++i)visible_text(lv_obj_get_child(o,i));
}
static void shot(const char *name,bool record_text)
{
    lv_obj_update_layout(view->screen);lv_refr_now(NULL);
    uint32_t size=lv_snapshot_buf_size_needed(view->screen,LV_IMG_CF_TRUE_COLOR);void *bytes=malloc(size);assert(bytes);lv_img_dsc_t image;
    assert(lv_snapshot_take_to_buf(view->screen,LV_IMG_CF_TRUE_COLOR,&image,bytes,size)==LV_RES_OK);
    char path[2048];snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n360 360\n255\n");
    for(unsigned i=0;i<360*360;++i){unsigned c=image.data[i*2]<<8|image.data[i*2+1];uint8_t rgb[]={(c>>11)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(rgb,1,3,f);}fclose(f);free(bytes);
    FILE *saved=snapshot_text;if(!record_text)snapshot_text=NULL;else fprintf(snapshot_text,"[%s]\n",name);visible_text(view->screen);snapshot_text=saved;
}
static void reset_home(void){card_view_pause(view,false);view->page=CARD_PAGE_HOME;view->notice=(card_message_t){0};card_view_refresh(view);pump(60);}
static void drawer_tests(void)
{
    reset_home();unsigned count=action_count();tap(180,327);assert(!view->visible&&action_count()==count);
    for(unsigned ix=0;ix<3;++ix)for(unsigned iy=0;iy<6;++iy){reset_home();int x=100+80*ix,y=240+17*iy;move(x,y,x,y-100);assert(view->open&&lv_obj_get_y(view->drawer)==104&&action_count()==count);}
    reset_home();move(180,150,180,70);assert(!view->visible);move(180,300,260,300);assert(!view->visible);
    move(180,320,180,310);assert(!view->visible||!view->open);reset_home();
    pointer=(lv_point_t){180,324};down=true;pump(60);
    for(int i=0;i<12;++i){pointer.y=324-i*15;pump(30);if(i>0)assert(lv_obj_get_y(view->drawer)==LV_CLAMP(104,360-i*15,360));char name[64];snprintf(name,sizeof(name),"drawer-open-%02d",i);shot(name,false);}
    down=false;for(unsigned i=0;i<12;++i){pump(20);char name[64];snprintf(name,sizeof(name),"drawer-snap-%02u",i);shot(name,false);}assert(view->open);shot("drawer",true);
    tap(180,180);assert(view->open&&action_count()==count);tap(120,205);assert(view->open&&action_count()==count);
    move(110,175,115,285);assert(!view->open&&action_count()==count);
    move(180,320,180,200);assert(view->open);tap(180,80);assert(!view->open&&action_count()==count);
    move(180,320,180,200);assert(view->open);move(180,128,180,270);assert(!view->open&&action_count()==count);
    for(unsigned button=0;button<4;++button)for(unsigned point=0;point<5;++point) {
        reset_home();move(180,320,180,200);assert(view->open);
        int left=72+(button%2)*120,top=136+(button/2)*72;
        int offsets[][2]={{4,4},{91,4},{4,59},{91,59},{48,32}};
        unsigned old=actions[button];tap(left+offsets[point][0],top+offsets[point][1]);assert(actions[button]==old+1);
        unsigned before=backs;move(12,180,115,180);assert(backs==before+1&&view->page==CARD_PAGE_HOME);
    }
    reset_home();move(180,320,180,200);assert(view->open);card_view_pause(view,true);assert(!view->visible&&!view->gesture.active);card_view_pause(view,false);
    reset_home();view->page=CARD_PAGE_QR;card_view_refresh(view);unsigned before=backs;
    move(12,180,50,180);assert(backs==before);move(12,180,100,240);assert(backs==before);move(70,180,160,180);assert(backs==before);
    move(12,180,115,180);assert(backs==before+1&&view->page==CARD_PAGE_HOME);
}
int main(int argc,char **argv)
{
    assert(argc==2);directory=argv[1];lv_init();sw_init();
    static lv_color_t pixels[360*12];static lv_disp_draw_buf_t draw;lv_disp_draw_buf_init(&draw,pixels,NULL,360*12);
    lv_disp_drv_t driver;lv_disp_drv_init(&driver);driver.hor_res=driver.ver_res=360;driver.draw_buf=&draw;driver.flush_cb=flush;lv_disp_t *display=lv_disp_drv_register(&driver);assert(display);
    lv_indev_drv_t input;lv_indev_drv_init(&input);input.type=LV_INDEV_TYPE_POINTER;input.read_cb=read_pointer;lv_indev_t *device=lv_indev_drv_register(&input);assert(device);
    lv_obj_t *blank=sw_screen();lv_scr_load(blank);card_state_t state={.id={1},.profile={.name="小林",.company="Deotaland",.role="硬件开发者",.contact="lin@example.org",.link="https://example.org/lin"}};
    card_exchange_t exchange={0};card_view_t instance;view=&instance;card_view_create(view,&state,&exchange,action,view);assert(view->language==CARD_EN);view->language=CARD_ZH;card_view_refresh(view);
    lv_scr_load(view->screen);sdgoods_swipe_back_bind(view->screen,back);pump(60);
    char path[2048];snprintf(path,sizeof(path),"%s/ui-text.txt",directory);snapshot_text=fopen(path,"w");assert(snapshot_text);
    assert(card_view_profile_supported(&state.profile));card_profile_t cjk=state.profile;strcpy(cjk.name,"璃");strcpy(cjk.company,"璃");assert(card_view_profile_supported(&cjk));card_profile_t emoji=state.profile;strcpy(emoji.name,"🙂");assert(!card_view_profile_supported(&emoji));
    shot("home",true);drawer_tests();
    reset_home();card_view_drawer(view,true);pump(240);unsigned language=actions[CARD_ACT_LANGUAGE];tap(180,308);assert(actions[CARD_ACT_LANGUAGE]==language+1&&view->language==CARD_EN&&view->open);shot("drawer-en",true);tap(180,308);assert(view->language==CARD_ZH&&view->open);reset_home();view->page=CARD_PAGE_QR;card_view_refresh(view);shot("qr",true);
    view->page=CARD_PAGE_NEARBY;card_view_refresh(view);shot("nearby-empty",true);
    exchange.count=1;strcpy(exchange.peers[0].name,"阿宁");card_view_refresh(view);shot("nearby",true);
    exchange.phase=CARD_EX_VERIFY;exchange.code=826419;strcpy(exchange.peer.name,"阿宁");view->page=CARD_PAGE_VERIFY;card_view_refresh(view);shot("verify",true);
    exchange.local_confirmed=true;card_view_refresh(view);shot("waiting",true);
    exchange.phase=CARD_EX_DONE;card_view_refresh(view);shot("success",true);
    view->page=CARD_PAGE_EDIT;view->ssid="Sugarat-DEMO";view->password="";card_view_refresh(view);shot("edit-wifi",true);
    assert(view->button_count==6);unsigned security=actions[CARD_ACT_EDIT_SECURITY];
    tap(180,280);assert(actions[CARD_ACT_EDIT_SECURITY]==security+1&&strlen(view->password)==8);
    shot("edit-password",true);tap(180,280);assert(actions[CARD_ACT_EDIT_SECURITY]==security+2&&!view->password[0]);
    view->edit_connected=true;card_view_refresh(view);shot("edit-url",true);
    assert(view->button_count==5);unsigned before=action_count();tap(180,210);assert(action_count()==before&&view->edit_connected);
    view->edit_connected=false;card_view_refresh(view);assert(view->button_count==6);shot("edit-disconnected",true);
    view->page=CARD_PAGE_CONTACTS;card_view_refresh(view);shot("contacts-empty",true);
    state.count=1;state.contacts[0].profile=state.profile;strcpy(state.contacts[0].profile.name,"阿宁");card_view_refresh(view);shot("contacts",true);
    state.count=2;state.contacts[1].profile=state.profile;view->selected=0;card_view_refresh(view);
    move(240,180,140,180);assert(view->selected==1);unsigned at_end=action_count();move(240,180,140,180);assert(view->selected==1&&action_count()==at_end);
    move(140,180,240,180);assert(view->selected==0);move(12,180,115,180);assert(view->page==CARD_PAGE_HOME);state.count=1;
    view->page=CARD_PAGE_NOTICE;view->notice=(card_message_t){"编辑热点已到期，请重新打开","Hotspot expired. Open Edit again."};card_view_refresh(view);shot("notice",true);
    
    for(unsigned lang=0;lang<2;++lang){
        view->language=(card_language_t)lang;
        for(unsigned page=CARD_PAGE_HOME;page<=CARD_PAGE_NOTICE;++page){
            view->page=(card_page_t)page;card_view_refresh(view);pump(60);
            char name[64];snprintf(name,sizeof(name),"%s-page-%u",lang?"en":"zh",page);shot(name,true);
        }
    }
    view->language=CARD_ZH;reset_home();
    card_profile_t original=state.profile;
    strcpy(state.profile.name,"喜欢开源硬件与电子设计的开发者");
    strcpy(state.profile.role,"嵌入式系统与智能硬件开发工程师");strcpy(state.profile.company,"Deotaland Hardware Research Lab");
    strcpy(state.profile.contact,"a-long-public-contact-address@example.org");
    strcpy(state.profile.link,"https://example.org/hardware/projects/social-card-profile");
    card_view_refresh(view);pump(60);shot("long-fields-start",true);
    pump(4500);shot("long-fields-scrolled",false);state.profile=original;
    state.profile=(card_profile_t){.name="Alex",.company="Deotaland",.role="Maker",.contact="alex@example.org",.link="https://example.org/alex"};
    view->language=CARD_EN;reset_home();shot("community-cover-en",true);
    card_view_drawer(view,true);pump(240);shot("community-drawer-en",true);
    card_view_pause(view,false);state.profile=original;
    fclose(snapshot_text);snapshot_text=NULL;lv_mem_monitor_t memory;lv_mem_monitor(&memory);printf("LVGL used bytes: %u\n",(unsigned)(memory.total_size-memory.free_size));
    lv_scr_load(blank);card_view_dispose(view);pump(240);_lv_font_clean_up_fmt_txt();lv_mem_buf_free_all();lv_mem_monitor(&memory);unsigned baseline=memory.total_size-memory.free_size;
    for(unsigned i=0;i<12;++i){card_view_create(view,&state,&exchange,action,view);lv_scr_load(view->screen);card_view_drawer(view,true);pump(40);lv_scr_load(blank);card_view_dispose(view);pump(240);_lv_font_clean_up_fmt_txt();lv_mem_buf_free_all();lv_mem_monitor(&memory);assert(memory.total_size-memory.free_size==baseline);}
    lv_indev_delete(device);lv_obj_del(blank);lv_disp_remove(display);lv_deinit();
    puts("PASS: 18 swipe origins, finger tracking, 20 icon-edge taps, bilingual pages, language action, long-field scrolling, gaps, slide-over rejection, outside/down dismissal, left-edge return, pause and repeated disposal");return 0;
}
