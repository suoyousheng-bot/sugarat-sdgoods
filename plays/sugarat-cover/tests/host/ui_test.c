/* SPDX-License-Identifier: Apache-2.0 */
/** Exercise real LVGL pointer dispatch and record the bilingual, shared device UI. */
#include "cv_view.h"
#include "cv_jpeg.h"
#include "cv_copy.h"
#include "sugarat_watch.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {cv_status_t status;const uint8_t *jpeg;size_t size;unsigned selections,modes,confirms,decodes;bool down,delay_close,defer_locale;cv_locale_t requested_locale;lv_point_t point;} fixture_t;
static lv_color_t frame[CV_PIXELS];static uint64_t clock_ms;
static void flush(lv_disp_drv_t *driver,const lv_area_t *area,lv_color_t *pixels)
{for(int y=area->y1;y<=area->y2;y++)for(int x=area->x1;x<=area->x2;x++)frame[y*360+x]=*pixels++;lv_disp_flush_ready(driver);}
static void read_pointer(lv_indev_drv_t *driver,lv_indev_data_t *data)
{fixture_t *f=driver->user_data;data->point=f->point;data->state=f->down?LV_INDEV_STATE_PR:LV_INDEV_STATE_REL;}
static bool status(void *user,cv_status_t *out){*out=((fixture_t *)user)->status;return true;}
static cv_error_t pixels(void *user,const char *id,uint16_t *out)
{fixture_t *f=user;(void)id;f->decodes++;return cv_jpeg(f->jpeg,f->size,out);}
static cv_error_t select_photo(void *user,const char *id)
{fixture_t *f=user;for(unsigned i=0;i<f->status.library.count;i++)if(!strcmp(id,f->status.library.photos[i].id)){f->status.library.selected=i;f->status.library.revision++;f->selections++;return CV_OK;}return CV_INVALID;}
static bool request_locale(void *user,cv_locale_t value){fixture_t *f=user;if(f->status.locale_pending)return false;f->requested_locale=value;f->status.locale_pending=f->defer_locale;if(!f->defer_locale){f->status.library.locale=value;f->status.library.revision++;f->status.locale_result++;f->status.locale_error=CV_OK;}return true;}
static bool mode(void *user,cv_mode_t value){fixture_t *f=user;f->modes++;f->status.mode=value;f->status.phase=value==CV_SEND_MODE?L_DISCOVER:value==CV_RECEIVE_MODE?L_ADVERTISE:L_OFF;f->status.ready=value!=CV_IDLE;f->status.error=CV_OK;f->status.changing=value==CV_IDLE&&f->delay_close;return true;}
static bool phone_security(void *user,bool required){fixture_t *f=user;snprintf(f->status.password,sizeof(f->status.password),"%s",required?"12345678":"");return mode(user,CV_PHONE_MODE);}
static bool offer(void *user,const uint8_t peer[6],const char *id){fixture_t *f=user;(void)peer;(void)id;f->status.phase=L_CONFIRM;return true;}
static bool confirm(void *user){fixture_t *f=user;f->confirms++;f->status.phase=L_WAIT;return true;}
static void tick(cv_view_t *v,unsigned duration)
{for(unsigned i=0;i<duration;i+=10){clock_ms+=10;cv_view_tick(v,clock_ms);lv_tick_inc(10);lv_timer_handler();}}
static void tap(cv_view_t *v,fixture_t *f,int x,int y)
{f->point=(lv_point_t){x,y};f->down=true;tick(v,80);f->down=false;tick(v,80);}
static void tap_child(cv_view_t *v,fixture_t *f,lv_obj_t *object)
{lv_area_t area;lv_obj_get_coords(object,&area);tap(v,f,(area.x1+area.x2)/2,(area.y1+area.y2)/2);}
static void drag(cv_view_t *v,fixture_t *f,int x,int y,int end_x,int end_y,bool return_start)
{
    f->point=(lv_point_t){x,y};f->down=true;tick(v,80);
    for(int i=1;i<=8;i++){f->point=(lv_point_t){x+(end_x-x)*i/8,y+(end_y-y)*i/8};tick(v,30);}
    if(return_start){f->point=(lv_point_t){x,y};tick(v,80);}
    f->down=false;tick(v,400);
}
static void save(const char *root,const char *name)
{
    char path[512];snprintf(path,sizeof(path),"%s/%s.bmp",root,name);FILE *f=fopen(path,"wb");assert(f);
    uint8_t header[54]={'B','M'};uint32_t fields[][2]={{2,54+CV_PIXELS*3},{10,54},{14,40},{18,360},{22,360},{34,CV_PIXELS*3}};
    for(unsigned i=0;i<sizeof(fields)/sizeof(fields[0]);i++)for(int b=0;b<4;b++)header[fields[i][0]+b]=(uint8_t)(fields[i][1]>>(b*8));
    header[26]=1;header[28]=24;fwrite(header,1,54,f);
    for(int y=359;y>=0;y--)for(int x=0;x<360;x++){lv_color32_t c;c.full=lv_color_to32(frame[y*360+x]);uint8_t pixel[]={c.ch.blue,c.ch.green,c.ch.red};fwrite(pixel,1,3,f);}assert(fclose(f)==0);
}
static void inspect(lv_obj_t *object,FILE *expected)
{
    if(lv_obj_check_type(object,&lv_label_class)){
        const char *value=lv_label_get_text(object);if(*value&&(unsigned char)*value<0xe0)fprintf(expected,"text=%s\n",value);
        else if(*value&&(unsigned char)*value>=0xe0&&((unsigned char)value[0]!=0xee&&(unsigned char)value[0]!=0xef))fprintf(expected,"text=%s\n",value);
    }
    if(lv_obj_has_flag(object,LV_OBJ_FLAG_USER_1)) {
        lv_area_t area;lv_obj_get_coords(object,&area);assert(area.x1>=60&&area.y1>=60&&area.x2<300&&area.y2<300);
        assert(lv_obj_get_width(object)>=60&&lv_obj_get_height(object)>=60);
        fprintf(expected,"target=%d,%d,%d,%d\n",area.x1,area.y1,lv_obj_get_width(object),lv_obj_get_height(object));
    }
    for(unsigned i=0;i<lv_obj_get_child_cnt(object);i++)inspect(lv_obj_get_child(object,i),expected);
}
static unsigned image_count(lv_obj_t *object)
{unsigned count=lv_obj_check_type(object,&lv_img_class)?1:0;for(unsigned i=0;i<lv_obj_get_child_cnt(object);i++)count+=image_count(lv_obj_get_child(object,i));return count;}
static void assert_hotspot_visible(void)
{
    unsigned lit=0;
    for(int y=157;y<184;y++)for(int x=60;x<300;x++){lv_color32_t c;c.full=lv_color_to32(frame[y*360+x]);if(c.ch.red>100&&c.ch.green>100&&c.ch.blue>100)lit++;}
    assert(lit>500);
}
int main(int argc,char **argv)
{
    assert(argc==4);FILE *jpeg=fopen(argv[1],"rb");assert(jpeg);fseek(jpeg,0,SEEK_END);long length=ftell(jpeg);rewind(jpeg);uint8_t *bytes=malloc((size_t)length);assert(bytes);assert(fread(bytes,1,(size_t)length,jpeg)==(size_t)length);fclose(jpeg);
    fixture_t f={.jpeg=bytes,.size=(size_t)length};f.status.library.count=2;f.status.library.writable=true;
    strcpy(f.status.library.photos[0].id,"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");strcpy(f.status.library.photos[1].id,"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    strcpy(f.status.ssid,"Sugarat-A7C4");memset(f.status.token,'1',32);f.status.own[4]=0xa7;f.status.own[5]=0xc4;
    lv_init();static lv_color_t strip[360*40];static lv_disp_draw_buf_t buffer;lv_disp_draw_buf_init(&buffer,strip,NULL,360*40);
    static lv_disp_drv_t driver;lv_disp_drv_init(&driver);driver.hor_res=360;driver.ver_res=360;driver.draw_buf=&buffer;driver.flush_cb=flush;lv_disp_drv_register(&driver);
    static lv_indev_drv_t input;lv_indev_drv_init(&input);input.type=LV_INDEV_TYPE_POINTER;input.read_cb=read_pointer;input.user_data=&f;lv_indev_drv_register(&input);
    cv_backend_t backend={.user=&f,.status=status,.pixels=pixels,.select=select_photo,.request_locale=request_locale,.mode=mode,.phone_security=phone_security,.offer=offer,.confirm=confirm};
    cv_view_t view;assert(cv_view_open(&view,backend));lv_scr_load(view.screen);tick(&view,200);
    assert(view.status.library.locale==CV_EN);
    drag(&view,&f,180,150,200,155,true);assert(view.page==V_ACTIONS);
    tap(&view,&f,180,145);assert(view.page==V_COVER);
    tap(&view,&f,180,150);assert(view.page==V_ACTIONS);
    unsigned jitter_modes=f.modes;drag(&view,&f,180,198,200,204,false);assert(f.modes==jitter_modes+1&&view.page==V_PHONE);
    drag(&view,&f,90,155,260,155,false);assert(view.page==V_ACTIONS);

    unsigned modes=f.modes;drag(&view,&f,180,207,260,207,true);assert(f.modes==modes&&view.page==V_ACTIONS);
    tap(&view,&f,180,207);assert(f.modes==modes+1&&view.page==V_PHONE);drag(&view,&f,90,155,260,155,false);assert(view.page==V_ACTIONS);tap(&view,&f,180,145);assert(view.page==V_COVER);
    for(unsigned repeat=0;repeat<4;repeat++) {
        tap(&view,&f,180,150);assert(view.page==V_ACTIONS);
        lv_obj_t *upload=lv_obj_get_child(view.content,5);assert(lv_obj_has_flag(upload,LV_OBJ_FLAG_USER_1));
        modes=f.modes;
        if(repeat<2)tap(&view,&f,repeat?290:70,198);
        else tap_child(&view,&f,lv_obj_get_child(upload,(int)repeat-2));
        assert(f.modes==modes+1&&view.page==V_PHONE);
        drag(&view,&f,170,270,290,270,false);assert(view.page==V_ACTIONS&&! *f.status.password);tap(&view,&f,180,145);assert(view.page==V_COVER);
    }
    tap(&view,&f,180,150);modes=f.modes;tap(&view,&f,180,234);assert(f.modes==modes&&view.page==V_COVER);
    tap(&view,&f,180,150);f.point=(lv_point_t){180,198};f.down=true;tick(&view,80);
    f.status.library.revision++;f.status.library.photos[0].size++;tick(&view,80);f.down=false;tick(&view,80);assert(f.modes==modes&&view.page==V_ACTIONS);
    tap(&view,&f,180,198);assert(f.modes==modes+1&&view.page==V_PHONE);drag(&view,&f,90,155,260,155,false);assert(view.page==V_ACTIONS);tap(&view,&f,180,145);assert(view.page==V_COVER);
    drag(&view,&f,280,170,80,180,false);assert(f.selections==1&&f.status.library.selected==1);drag(&view,&f,80,170,280,180,false);assert(f.selections==2&&f.status.library.selected==0);
    unsigned decodes=f.decodes;drag(&view,&f,280,170,80,180,false);
    assert(f.selections==3&&f.status.library.selected==1&&f.decodes==decodes&&!strcmp(view.loaded,f.status.library.photos[1].id));
    drag(&view,&f,80,170,280,180,false);assert(f.selections==4&&f.status.library.selected==0&&f.decodes==decodes&&!strcmp(view.loaded,f.status.library.photos[0].id));
    drag(&view,&f,280,170,260,170,false);assert(f.selections==4);drag(&view,&f,280,170,180,280,false);assert(f.selections==4);
    assert(view.page==V_ACTIONS);tap(&view,&f,180,145);assert(view.page==V_COVER);
    tap(&view,&f,180,150);tap(&view,&f,267,90);assert(f.status.library.locale==CV_ZH);
    lv_obj_t *retained_phone=view.menu_phone,*retained_locale=view.menu_locale;f.defer_locale=true;
    tap(&view,&f,267,90);assert(f.status.locale_pending&&f.status.library.locale==CV_ZH&&view.page==V_ACTIONS);
    assert(view.menu_phone==retained_phone&&view.menu_locale==retained_locale&&lv_obj_has_state(view.menu_locale,LV_STATE_DISABLED));
    tick(&view,2000);assert(view.page==V_ACTIONS&&view.menu_phone==retained_phone);
    f.point=(lv_point_t){180,198};f.down=true;tick(&view,80);
    f.status.library.locale=f.requested_locale;f.status.library.revision++;f.status.locale_pending=false;f.status.locale_result++;tick(&view,80);
    f.down=false;tick(&view,80);assert(view.page==V_PHONE&&f.status.library.locale==CV_EN);
    drag(&view,&f,90,155,260,155,false);assert(view.page==V_ACTIONS);
    tap(&view,&f,267,90);assert(f.status.locale_pending);f.status.locale_pending=false;f.status.locale_result++;f.status.locale_error=CV_SETTINGS;tick(&view,80);assert(view.page==V_ERROR&&view.error==CV_SETTINGS);
    drag(&view,&f,90,210,260,210,false);assert(view.page==V_ACTIONS);tick(&view,80);assert(view.page==V_ACTIONS);
    f.defer_locale=false;tap(&view,&f,267,90);assert(f.status.library.locale==CV_ZH);
    view.page=V_PHONE;view.dirty=true;f.status.mode=CV_PHONE_MODE;f.status.ready=false;f.status.changing=true;tick(&view,100);
    f.point=(lv_point_t){90,150};f.down=true;tick(&view,80);f.status.ready=true;f.status.changing=false;tick(&view,80);
    for(int x=110;x<=270;x+=20){f.point.x=(lv_coord_t)x;tick(&view,30);}f.down=false;tick(&view,100);assert(view.page==V_ACTIONS);
    view.page=V_SHARE;view.dirty=true;tick(&view,100);
    drag(&view,&f,90,150,120,150,false);assert(view.page==V_SHARE);
    drag(&view,&f,260,150,90,150,false);assert(view.page==V_SHARE);
    drag(&view,&f,90,150,260,150,false);assert(view.page==V_ACTIONS);
    view.page=V_WAIT;view.dirty=true;f.status.mode=CV_RECEIVE_MODE;f.status.phase=L_ADVERTISE;tick(&view,100);
    f.delay_close=true;drag(&view,&f,90,150,260,150,false);assert(view.page==V_SHARE&&view.closing);
    modes=f.modes;tap(&view,&f,180,198);assert(f.modes==modes&&view.closing);
    f.status.changing=false;f.status.error=CV_RADIO;tick(&view,100);assert(view.page==V_ERROR&&!view.closing);
    f.delay_close=false;f.status.error=CV_OK;drag(&view,&f,90,210,260,210,false);assert(view.page==V_ACTIONS);
    view.page=V_PAIR;view.dirty=true;f.status.phase=L_CONFIRM;tick(&view,100);unsigned confirmations=f.confirms;
    f.point=(lv_point_t){240,270};f.down=true;tick(&view,80);f.status.phase=L_RECEIVE;tick(&view,80);f.down=false;tick(&view,80);assert(f.confirms==confirmations&&view.page==V_PROGRESS);
    FILE *expected=fopen(argv[3],"wb");assert(expected);
    const cv_page_t pages[]={V_COVER,V_ACTIONS,V_PHONE,V_SHARE,V_PEERS,V_WAIT,V_PAIR,V_PROGRESS,V_DONE,V_ERROR};
    const char *names[]={"cover","actions","phone","share","peers","waiting","pair","progress","saved","error"};
    for(int language=CV_EN;language<=CV_ZH;language++)for(unsigned i=0;i<10;i++) {
        f.status.library.locale=(cv_locale_t)language;f.status.mode=CV_RECEIVE_MODE;f.status.phase=L_OFF;f.status.error=CV_OK;f.status.ready=true;f.status.code=123456;f.status.progress=68;
        f.status.peer_count=1;f.status.peers[0][4]=0x12;f.status.peers[0][5]=0x34;strcpy(f.status.saved,f.status.library.photos[0].id);
        if(pages[i]==V_PHONE)f.status.mode=CV_PHONE_MODE;
        if(pages[i]==V_PAIR)f.status.phase=L_CONFIRM;
        if(pages[i]==V_PROGRESS)f.status.phase=L_RECEIVE;
        if(pages[i]==V_DONE)f.status.phase=L_DONE;
        view.error=pages[i]==V_ERROR?CV_TIMEOUT:CV_OK;view.page=pages[i];view.dirty=true;view.menu_at=clock_ms;tick(&view,150);lv_refr_now(NULL);
        char name[64];snprintf(name,sizeof(name),"%s-%s",language==CV_EN?"en":"zh",names[i]);save(argv[2],name);fprintf(expected,"[%s]\n",name);inspect(view.content,expected);
        if(pages[i]==V_PHONE){
            assert_hotspot_visible();assert(image_count(view.content)==0);tap(&view,&f,180,270);assert(!strcmp(f.status.password,"12345678"));assert(image_count(view.content)==0);
            lv_refr_now(NULL);assert_hotspot_visible();snprintf(name,sizeof(name),"%s-phone-password",language==CV_EN?"en":"zh");save(argv[2],name);fprintf(expected,"[%s]\n",name);inspect(view.content,expected);
            tap(&view,&f,180,270);assert(!*f.status.password);
            f.status.stations=1;tick(&view,100);assert(image_count(view.content)==1);lv_refr_now(NULL);snprintf(name,sizeof(name),"%s-page-qr",language==CV_EN?"en":"zh");save(argv[2],name);fprintf(expected,"[%s]\n",name);inspect(view.content,expected);
            f.status.stations=0;tick(&view,100);assert(image_count(view.content)==0);
        }
    }
    assert(fclose(expected)==0);cv_view_pause(&view,true);tap(&view,&f,180,150);assert(view.paused);
    lv_scr_load(sw_screen());cv_view_close(&view);free(bytes);
    puts("PASS: real LVGL taps/swipes, decode-free preloaded swipes, held-refresh cancellation, connection-first QR visibility, optional visible password, English/Chinese text, safe targets and 24 bilingual UI snapshots");return 0;
}
