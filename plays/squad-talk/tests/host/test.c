/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Real protocol/codec and LVGL pointer tests; radio delivery is deterministic and explicitly simulated. */
#include "talk/talk_core.h"
#include "talk/talk_view.h"
extern const lv_font_t sg_font_icon_24,sg_font_icon_40;
#include "ui_helloapp.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {unsigned from;size_t size;uint8_t data[TALK_PACKET_MAX],destination[6];} queued_t;
static queued_t wire[512];static unsigned head,tail;static talk_core_t peers[2];
static unsigned played[2];static bool fail_send;static uint64_t clock_ms;
static bool send_wire(const uint8_t *destination,const uint8_t *data,size_t size,void *user)
{
    unsigned id=(unsigned)(uintptr_t)user;
    if(fail_send)return false;
    assert(tail-head<512&&size<=TALK_PACKET_MAX);
    queued_t *p=&wire[tail++%512];p->from=id;p->size=size;memcpy(p->data,data,size);memcpy(p->destination,destination,6);return true;
}
static void play_wire(const int16_t *pcm,size_t n,void *user)
{assert(n==TALK_SAMPLES&&pcm);played[(unsigned)(uintptr_t)user]++;}
static void deliver(void)
{
    unsigned limit=0;
    while(head<tail){assert(limit++<500);queued_t p=wire[head++%512];unsigned other=1-p.from;talk_core_receive(&peers[other],peers[p.from].mac,p.data,p.size);}
}
static void reset_peers(void)
{
    head=tail=played[0]=played[1]=0;clock_ms=100;fail_send=false;
    const uint8_t a[6]={2,0,0,0,0,1},b[6]={2,0,0,0,0,2};
    talk_core_init(&peers[0],a,10,send_wire,play_wire,(void *)(uintptr_t)0);
    talk_core_init(&peers[1],b,20,send_wire,play_wire,(void *)(uintptr_t)1);
    talk_core_tick(&peers[0],clock_ms);talk_core_tick(&peers[1],clock_ms);deliver();
    assert(peers[0].phase==TALK_READY&&peers[1].phase==TALK_READY);
}
static void advance(unsigned ms)
{clock_ms+=ms;talk_core_tick(&peers[0],clock_ms);talk_core_tick(&peers[1],clock_ms);deliver();}
static void codec_tests(void)
{
    int16_t pcm[TALK_SAMPLES],decoded[TALK_SAMPLES];uint8_t coded[TALK_AUDIO_BYTES];double error=0,energy=0;
    for(unsigned i=0;i<TALK_SAMPLES;i++)pcm[i]=(int16_t)(sin(i*.17)*8000);
    talk_audio_encode(coded,pcm);talk_audio_decode(decoded,coded);
    for(unsigned i=0;i<TALK_SAMPLES;i++){double e=pcm[i]-decoded[i];error+=e*e;energy+=(double)pcm[i]*pcm[i];}
    assert(error/energy<.02);
    for(unsigned t=0;t<3;t++){
        for(unsigned i=0;i<TALK_SAMPLES;i++)pcm[i]=t==0?0:t==1?32767:-32768;
        talk_audio_encode(coded,pcm);talk_audio_decode(decoded,coded);
        assert(abs((int)decoded[TALK_SAMPLES-1]-pcm[TALK_SAMPLES-1])<20);
    }
    puts("PASS: independent ADPCM blocks, speech-band sine reconstruction, silence and saturation");
}
static void protocol_tests(void)
{
    reset_peers();int16_t audio[TALK_SAMPLES]={0};
    for(unsigned turn=0;turn<200;turn++){
        unsigned a=turn%2,b=1-a;talk_core_hold(&peers[a],true);deliver();
        assert(peers[a].phase==TALK_TRANSMITTING&&peers[b].phase==TALK_RECEIVING);
        talk_core_audio(&peers[a],audio);deliver();
        talk_core_hold(&peers[a],false);deliver();
        assert(peers[a].phase==TALK_READY&&peers[b].phase==TALK_READY);advance(16);
    }
    assert(played[0]==100&&played[1]==100);
    reset_peers();talk_core_hold(&peers[0],true);talk_core_hold(&peers[1],true);deliver();
    assert(peers[0].phase==TALK_TRANSMITTING&&peers[1].phase==TALK_RECEIVING);
    talk_core_hold(&peers[0],false);deliver();assert(peers[1].phase==TALK_READY);
    talk_core_hold(&peers[1],true);assert(peers[1].phase==TALK_READY);
    talk_core_hold(&peers[1],false);talk_core_hold(&peers[1],true);deliver();assert(peers[1].phase==TALK_TRANSMITTING);
    reset_peers();talk_core_hold(&peers[0],true);talk_core_hold(&peers[0],false);deliver();
    assert(peers[0].phase!=TALK_TRANSMITTING);advance(700);assert(peers[1].phase==TALK_READY);
    reset_peers();talk_core_hold(&peers[0],true);deliver();
    talk_core_audio(&peers[0],audio);queued_t packet=wire[(tail-1)%512];deliver();unsigned before=played[1];
    talk_core_receive(&peers[1],peers[0].mac,packet.data,packet.size);assert(played[1]==before);
    talk_core_channel(&peers[1],2);talk_core_receive(&peers[1],peers[0].mac,packet.data,packet.size);assert(played[1]==before);
    deliver();advance(600);assert(peers[1].phase==TALK_WAITING);
    talk_core_channel(&peers[1],1);advance(600);assert(peers[1].phase==TALK_READY);
    reset_peers();talk_core_hold(&peers[0],true);deliver();talk_core_pause(&peers[0],true);deliver();
    assert(peers[0].phase==TALK_PAUSED&&peers[1].phase==TALK_READY);talk_core_tick(&peers[0],++clock_ms);deliver();
    assert(peers[1].phase==TALK_WAITING);talk_core_pause(&peers[0],false);advance(600);assert(peers[1].phase==TALK_READY);
    reset_peers();talk_core_hold(&peers[0],true);head=tail;talk_core_tick(&peers[0],1100);assert(peers[0].phase==TALK_READY&&peers[0].transport_failed);
    reset_peers();talk_core_hold(&peers[0],true);deliver();fail_send=true;talk_core_audio(&peers[0],audio);assert(peers[0].phase==TALK_READY&&peers[0].transport_failed);
    reset_peers();talk_core_tick(&peers[0],4000);assert(peers[0].phase==TALK_WAITING);
    reset_peers();talk_core_hold(&peers[0],true);deliver();for(unsigned i=0;i<59;i++){talk_core_audio(&peers[0],audio);deliver();advance(500);assert(peers[0].phase==TALK_TRANSMITTING);}
    talk_core_audio(&peers[0],audio);deliver();advance(600);assert(peers[0].phase!=TALK_TRANSMITTING);
    reset_peers();talk_core_hold(&peers[0],true);deliver();talk_core_audio(&peers[0],audio);packet=wire[(tail-1)%512];deliver();
    talk_packet_t p;assert(talk_packet_read(&p,packet.data,packet.size));
    for(size_t n=0;n<packet.size;n++)assert(!talk_packet_read(&p,packet.data,n));
    const unsigned bad[]={0,4,5,6,7,20,21,22,23,26,27};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++){uint8_t copy[TALK_PACKET_MAX];memcpy(copy,packet.data,packet.size);copy[bad[i]]=255;assert(!talk_packet_read(&p,copy,packet.size));}
    uint32_t random=1;uint8_t fuzz[TALK_PACKET_MAX];
    for(unsigned i=0;i<10000;i++){for(unsigned j=0;j<sizeof(fuzz);j++){random=random*1664525+1013904223;fuzz[j]=(uint8_t)(random>>24);}talk_packet_read(&p,fuzz,i%sizeof(fuzz));}
    puts("PASS: 200 bidirectional turns, simultaneous press, early release, duplicate/stale/group isolation, pause, peer loss, send/request/voice timeouts and malformed packets");
}

static talk_status_t ui_status={.channel=1};static bool ui_held,ui_paused,ui_stopped,menu,suspended;
static void (*pause_cb)(void),(*resume_cb)(void);
uint64_t host_time_ms;
esp_err_t talk_radio_start(void){return 0;}
void talk_radio_status(talk_status_t *s){*s=ui_status;}
void talk_radio_input(bool held,uint8_t ch,bool paused){ui_held=held;ui_paused=paused;ui_status.channel=ch;}
void talk_radio_stop(void){ui_stopped=true;ui_held=false;}
static bool restarted;
void esp_restart(void){restarted=true;}
void sdgoods_app_shell_bind(lv_obj_t *o){assert(o);}
void sdgoods_app_shell_set_pause_cb(void (*cb)(void)){pause_cb=cb;}
void sdgoods_app_shell_set_resume_cb(void (*cb)(void)){resume_cb=cb;}
bool sdgoods_app_shell_menu_is_open(void){return menu;}
bool sdgoods_power_is_suspended(void){return suspended;}
void sdgoods_lvgl_fast_loop(bool b){(void)b;}
static lv_point_t pointer;static bool down;
static void input(lv_indev_drv_t *d,lv_indev_data_t *data){(void)d;data->point=pointer;data->state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static unsigned flushed_pixels;
static void flush(lv_disp_drv_t *d,const lv_area_t *a,lv_color_t *p)
{(void)p;flushed_pixels+=(unsigned)(a->x2-a->x1+1)*(unsigned)(a->y2-a->y1+1);lv_disp_flush_ready(d);}
static void pump(unsigned ms){for(unsigned n=0;n<ms;n+=10){host_time_ms+=10;lv_tick_inc(10);ui_helloapp_poll();lv_timer_handler();}}
static void labels(lv_obj_t *o,FILE *out)
{
    if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))return;
    if(lv_obj_check_type(o,&lv_label_class)){
        const char *s=lv_label_get_text(o);const lv_font_t *f=lv_obj_get_style_text_font(o,0);
        if(f==&sg_font_icon_24||f==&sg_font_icon_40)return;
        for(const unsigned char *p=(const unsigned char *)s;*p;p++)if(*p!='\n'){assert(*p>=32&&*p<127);lv_font_glyph_dsc_t g;assert(lv_font_get_glyph_dsc(f,&g,*p,0));}
        lv_area_t a;lv_obj_get_coords(o,&a);
        int dx=(abs(a.x1-180)>abs(a.x2-180)?abs(a.x1-180):abs(a.x2-180));
        int dy=(abs(a.y1-180)>abs(a.y2-180)?abs(a.y1-180):abs(a.y2-180));
        if(*s&&dx*dx+dy*dy>178*178){fprintf(stderr,"OUTSIDE %s %d,%d..%d,%d\n",s,a.x1,a.y1,a.x2,a.y2);abort();}
        lv_obj_t *parent=lv_obj_get_parent(o);
        if(*s&&lv_obj_get_width(parent)==220&&lv_obj_get_height(parent)==350){
            lv_area_t target;lv_obj_get_coords(parent,&target);
            int64_t tx=abs(a.x1-(target.x1+110))>abs(a.x2-(target.x1+110))?abs(a.x1-(target.x1+110)):abs(a.x2-(target.x1+110));
            int64_t ty=abs(a.y1-(target.y1+175))>abs(a.y2-(target.y1+175))?abs(a.y1-(target.y1+175)):abs(a.y2-(target.y1+175));
            assert(tx*tx*172*172+ty*ty*107*107<=107LL*107*172*172);
        }
        if(*s)fprintf(out,"%s\n",s);
    }
    for(unsigned i=0;i<lv_obj_get_child_cnt(o);i++)labels(lv_obj_get_child(o,i),out);
}
static void shot(const char *out,const char *name,FILE *text)
{
    pump(100);lv_obj_t *screen=lv_scr_act();lv_obj_update_layout(screen);fprintf(text,"[%s]\n",name);labels(screen,text);
    uint32_t bytes=lv_snapshot_buf_size_needed(screen,LV_IMG_CF_TRUE_COLOR);void *buffer=malloc(bytes);assert(buffer);lv_img_dsc_t image;
    assert(lv_snapshot_take_to_buf(screen,LV_IMG_CF_TRUE_COLOR,&image,buffer,bytes)==LV_RES_OK);
    char path[1024];snprintf(path,sizeof(path),"%s/%s.ppm",out,name);FILE *file=fopen(path,"wb");assert(file);fprintf(file,"P6\n360 360\n255\n");
    for(unsigned i=0;i<360*360;i++){unsigned c=image.data[i*2]<<8|image.data[i*2+1];uint8_t rgb[]={(c>>11)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(rgb,1,3,file);}fclose(file);free(buffer);
}
static void drag(int x,int y,int end_x,int end_y)
{
    pointer=(lv_point_t){x,y};down=true;pump(80);
    for(int i=1;i<=6;i++){pointer=(lv_point_t){x+(end_x-x)*i/6,y+(end_y-y)*i/6};pump(30);}
    down=false;pump(80);
}
static void ui_tests(const char *out)
{
    lv_init();static lv_color_t pixels[360*12];lv_disp_draw_buf_t draw;lv_disp_draw_buf_init(&draw,pixels,NULL,360*12);
    lv_disp_drv_t d;lv_disp_drv_init(&d);d.hor_res=d.ver_res=360;d.draw_buf=&draw;d.flush_cb=flush;lv_disp_t *display=lv_disp_drv_register(&d);assert(display);
    lv_indev_drv_t i;lv_indev_drv_init(&i);i.type=LV_INDEV_TYPE_POINTER;i.read_cb=input;lv_indev_t *device=lv_indev_drv_register(&i);assert(device);
    ui_helloapp_start();pump(100);assert(pause_cb&&resume_cb);
    char path[1024];snprintf(path,sizeof(path),"%s/ui-text.txt",out);FILE *text=fopen(path,"w");assert(text);
    shot(out,"waiting",text);
    ui_status.phase=TALK_REQUESTING;shot(out,"requesting",text);
    ui_status.phase=TALK_READY;ui_status.link_failed=true;shot(out,"retry",text);
    ui_status.link_failed=false;ui_status.online=true;shot(out,"ready",text);
    pointer=(lv_point_t){74,186};down=true;pump(80);assert(!ui_held);down=false;pump(60);
    pointer=(lv_point_t){100,250};down=true;pump(80);assert(ui_held);down=false;pump(80);assert(!ui_held);
    pointer=(lv_point_t){180,274};down=true;pump(80);assert(ui_held);
    ui_status.phase=TALK_TRANSMITTING;ui_status.level=70;shot(out,"talking",text);
    pointer=(lv_point_t){180,130};pump(80);assert(!ui_held);down=false;pump(80);
    ui_status.phase=TALK_RECEIVING;ui_status.level=50;shot(out,"listening",text);
    pump(100);flushed_pixels=0;ui_status.level=75;pump(100);
    assert(flushed_pixels>0&&flushed_pixels<4000);
    puts("PASS: measured audio-level update redraws fewer than 4000 pixels, preserving the static target");
    drag(240,90,120,90);assert(ui_status.channel==2&&!ui_held);
    drag(120,90,240,90);assert(ui_status.channel==1&&!ui_held);
    drag(120,90,240,90);assert(ui_status.channel==TALK_CHANNELS);
    drag(240,90,120,90);assert(ui_status.channel==1);
    drag(280,90,80,90);assert(ui_status.channel==2);
    drag(180,90,145,90);assert(ui_status.channel==2);
    drag(240,90,140,170);assert(ui_status.channel==2);
    drag(180,80,180,160);assert(ui_status.channel==2);
    drag(180,35,90,35);assert(ui_status.channel==2);
    drag(82,90,82,90);assert(ui_status.channel==2&&!ui_held);
    drag(180,274,40,274);assert(ui_status.channel==2&&!ui_held);
    drag(240,140,180,274);assert(ui_status.channel==2&&!ui_held);
    pointer=(lv_point_t){240,90};down=true;pump(80);
    menu=true;pause_cb();pointer=(lv_point_t){120,90};pump(80);down=false;pump(80);assert(ui_paused&&!ui_held&&ui_status.channel==2);shot(out,"paused",text);
    menu=false;resume_cb();pump(80);assert(!ui_paused&&!ui_held);
    pointer=(lv_point_t){180,274};down=true;pump(80);assert(ui_held);suspended=true;pump(80);assert(!ui_held&&ui_paused);
    suspended=false;down=false;pump(80);ui_status.fault=TALK_FAULT_AUDIO;shot(out,"error",text);fclose(text);
    drag(240,90,120,90);assert(ui_status.channel==2&&!ui_held&&!restarted);
    pointer=(lv_point_t){180,274};down=true;pump(80);down=false;pump(80);assert(restarted);
    lv_obj_t *screen=lv_scr_act(),*blank=lv_obj_create(NULL);lv_scr_load(blank);lv_obj_del(screen);assert(ui_stopped&&!pause_cb&&!resume_cb);
    lv_indev_delete(device);lv_disp_remove(display);lv_deinit();
    puts("PASS: actual LVGL half-ellipse hold/release/drag-out, swipe directions/wrap/one-step, short/diagonal/top-edge rejection, speech/swipe isolation, overlay/sleep cancellation and circle-safe glyphs");
}
int main(int argc,char **argv){assert(argc==2);codec_tests();protocol_tests();ui_tests(argv[1]);return 0;}
