/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "cv_view.h"
#include "cv_copy.h"
#include "sugarat_watch.h"
#include "sdgoods_tap.h"
#include "src/extra/libs/qrcode/qrcodegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum { A_PHONE=1,A_SHARE,A_SEND,A_RECEIVE,A_CONFIRM,A_DONE,A_SHOW,A_LOCALE,A_SECURITY,A_PEER=100 };
static const char *text(cv_view_t *v,cv_copy_key_t key){return cv_text(v->status.library.locale,key);}
static void cancel_press(cv_view_t *v)
{
    /* A rebuilt target must not acquire a press already held on its predecessor. */
    if(v->gesture.held&&v->pointer)lv_indev_wait_release(v->pointer);
    v->pointer=NULL;v->gesture.held=false;v->gesture.lost=true;v->pressed=NULL;v->drag_ready=false;v->background_press=false;
}
/* The existing Watch API supplies every control, text role and icon. */
static lv_obj_t *label(cv_view_t *v,const char *copy,int x,int y,int width,sw_font_t role,sw_tone_t tone)
{
    lv_obj_t *o=sw_label(v->content,copy,role,tone);lv_obj_set_width(o,width);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_pos(o,x,y);return o;
}
static lv_obj_t *button(cv_view_t *v,const char *copy,sw_icon_t icon,int x,int y,int width,int action,sw_tone_t tone)
{
    lv_obj_t *o=sw_button(v->content,icon,copy,tone==SW_TONE_TEXT?SW_BUTTON_PLAIN:SW_BUTTON_FILLED,tone);
    lv_obj_set_width(o,width);lv_obj_set_height(o,60);lv_obj_set_style_min_width(o,60,0);lv_obj_set_style_pad_hor(o,8,0);lv_obj_set_pos(o,x,y);sw_set_action(o,action);
    lv_obj_add_flag(o,LV_OBJ_FLAG_PRESS_LOCK);return o;
}
static void title(cv_view_t *v,cv_copy_key_t key)
{label(v,text(v,key),60,78,240,SW_FONT_HEADLINE,SW_TONE_TEXT);}
static void thumbnail(cv_view_t *v,int x,int y,int size)
{
    lv_obj_t *o=lv_img_create(v->content);lv_img_set_src(o,&v->descriptors[0]);
    lv_img_set_pivot(o,0,0);lv_img_set_zoom(o,(uint16_t)(256*size/360));lv_obj_set_pos(o,x,y);
}
static void show_qr(cv_view_t *v,const char *content)
{
    uint8_t scratch[qrcodegen_BUFFER_LEN_FOR_VERSION(10)],data[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
    if(!qrcodegen_encodeText(content,scratch,data,qrcodegen_Ecc_MEDIUM,1,10,qrcodegen_Mask_AUTO,true)){v->error=CV_INVALID;return;}
    int modules=qrcodegen_getSize(data),scale=144/(modules+8),size=(modules+8)*scale;
    if(scale<2){v->error=CV_INVALID;return;}
    for(int y=0;y<size;y++)for(int x=0;x<size;x++) {
        int mx=x/scale-4,my=y/scale-4;
        bool dark=mx>=0&&my>=0&&mx<modules&&my<modules&&qrcodegen_getModule(data,mx,my);
        v->qr_pixels[y*size+x]=lv_color_hex(dark?0:0xffffff).full;
    }
    v->qr=(lv_img_dsc_t){.header={.cf=LV_IMG_CF_TRUE_COLOR,.w=size,.h=size},.data_size=(uint32_t)(size*size*2),.data=(uint8_t *)v->qr_pixels};
    lv_obj_t *o=lv_img_create(v->content);lv_img_set_src(o,&v->qr);lv_obj_set_pos(o,180-size/2,212-size/2);
}
static void set_text(lv_obj_t *object,const char *value)
{if(strcmp(lv_label_get_text(object),value))lv_label_set_text(object,value);}
static void update_menu(cv_view_t *v)
{
    char count[24];snprintf(count,sizeof(count),"%02u / %02u",v->status.library.selected+1,v->status.library.count);
    set_text(v->menu_title,text(v,v->status.library.selected==0?C_TITLE:C_PHOTO));
    set_text(lv_obj_get_child(v->menu_locale,-1),v->status.locale_pending?"…":v->status.library.locale==CV_EN?"中":"EN");
    if(v->status.locale_pending)lv_obj_add_state(v->menu_locale,LV_STATE_DISABLED);else lv_obj_clear_state(v->menu_locale,LV_STATE_DISABLED);
    set_text(v->menu_count,count);set_text(v->menu_hint,text(v,C_SWIPE));
    /* Watch buttons append their text label after the optional icon. */
    set_text(lv_obj_get_child(v->menu_phone,-1),text(v,C_PHONE));set_text(lv_obj_get_child(v->menu_share,-1),text(v,C_EXCHANGE));
    lv_obj_update_layout(v->screen);
}
static void render(cv_view_t *v)
{
    if(v->page==V_ACTIONS&&v->menu_title&&!v->closing){update_menu(v);v->dirty=false;return;}
    if(!(v->gesture.held&&v->background_press))cancel_press(v);
    lv_obj_clean(v->content);v->ring=v->percent=NULL;v->dirty=false;
    v->menu_title=v->menu_locale=v->menu_count=v->menu_hint=v->menu_phone=v->menu_share=NULL;
    bool cover=v->page==V_COVER||v->page==V_ACTIONS;
    lv_obj_set_style_bg_opa(v->screen,LV_OPA_COVER,0);
    for(unsigned i=0;i<2;i++){lv_obj_set_x(v->images[i],i?360:0);if(cover)lv_obj_clear_flag(v->images[i],LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(v->images[i],LV_OBJ_FLAG_HIDDEN);}
    char line[128];cv_library_t *library=&v->status.library;
    if(v->closing){label(v,text(v,C_CLOSING),60,170,240,SW_FONT_BODY,SW_TONE_MUTED);lv_obj_update_layout(v->screen);return;}
    switch(v->page) {
    case V_COVER:
        if(!library->count){label(v,text(v,C_NO_STORAGE),60,120,240,SW_FONT_HEADLINE,SW_TONE_TEXT);label(v,cv_error_text(library->locale,library->error),66,163,228,SW_FONT_BODY,SW_TONE_MUTED);button(v,library->locale==CV_EN?"中文":"English",SW_ICON_NONE,66,240,228,A_LOCALE,SW_TONE_TEXT);}
        break;
    case V_ACTIONS: {
        lv_obj_t *scrim=sw_card(v->content);lv_obj_set_layout(scrim,0);lv_obj_set_size(scrim,360,360);lv_obj_set_pos(scrim,0,0);
        lv_obj_set_style_radius(scrim,0,0);lv_obj_set_style_bg_color(scrim,sw_color(SW_TONE_BG),0);lv_obj_set_style_bg_opa(scrim,175,0);
        v->menu_title=label(v,library->selected==0?text(v,C_TITLE):text(v,C_PHOTO),66,78,162,SW_FONT_HEADLINE,SW_TONE_TEXT);
        v->menu_locale=button(v,library->locale==CV_EN?"中":"EN",SW_ICON_NONE,240,60,60,A_LOCALE,SW_TONE_TEXT);
        snprintf(line,sizeof(line),"%02u / %02u",library->selected+1,library->count);v->menu_count=label(v,line,60,118,240,SW_FONT_BODY,SW_TONE_TEXT);
        v->menu_hint=label(v,text(v,C_SWIPE),60,146,240,SW_FONT_CAPTION,SW_TONE_TEXT);
        v->menu_phone=button(v,text(v,C_PHONE),SW_ICON_PHONE,66,168,228,A_PHONE,SW_TONE_BLUE);
        v->menu_share=button(v,text(v,C_EXCHANGE),SW_ICON_GIFT,66,240,228,A_SHARE,SW_TONE_TEXT);update_menu(v);break;
    }
    case V_PHONE:
        title(v,v->status.stations?C_UPLOAD:C_CONNECT);
        if(!v->status.ready||v->status.changing){label(v,text(v,C_PREPARING),60,170,240,SW_FONT_BODY,SW_TONE_MUTED);break;}
        label(v,text(v,v->status.stations?C_PAGE_STEP:C_WIFI_STEP),60,119,240,SW_FONT_BODY,SW_TONE_TEXT);
        if(v->status.stations) {
            show_qr(v,CV_PORTAL_ORIGIN "/");label(v,CV_PORTAL_ORIGIN,60,282,240,SW_FONT_CAPTION,SW_TONE_TEXT);
        } else {
            snprintf(line,sizeof(line),"Wi-Fi: %s",v->status.ssid);
            lv_obj_t *ssid=label(v,line,60,157,240,SW_FONT_HEADLINE,SW_TONE_TEXT);lv_label_set_long_mode(ssid,LV_LABEL_LONG_CLIP);
            if(*v->status.password)snprintf(line,sizeof(line),"%s: %s",text(v,C_PASSWORD),v->status.password);
            else snprintf(line,sizeof(line),"%s",text(v,C_NO_PASSWORD));
            label(v,line,60,191,240,SW_FONT_BODY,SW_TONE_TEXT);
            label(v,text(v,C_KEEP_WIFI),60,218,240,SW_FONT_CAPTION,SW_TONE_MUTED);
            button(v,text(v,*v->status.password?C_DISABLE_PASSWORD:C_ENABLE_PASSWORD),SW_ICON_NONE,66,240,228,A_SECURITY,SW_TONE_TEXT);
        }break;
    case V_SHARE:
        title(v,C_EXCHANGE);label(v,text(v,C_KEEP_NEAR),60,128,240,SW_FONT_CAPTION,SW_TONE_MUTED);
        button(v,text(v,C_SEND),SW_ICON_GIFT,66,168,228,A_SEND,SW_TONE_ACCENT);
        button(v,text(v,C_RECEIVE),SW_ICON_IMAGE,66,240,228,A_RECEIVE,SW_TONE_TEXT);break;
    case V_PEERS:
        title(v,C_CHOOSE);
        if(!v->status.peer_count){label(v,text(v,C_SEARCHING),60,155,240,SW_FONT_BODY,SW_TONE_TEXT);label(v,text(v,C_NO_PEERS),60,244,240,SW_FONT_CAPTION,SW_TONE_MUTED);}
        else {
            unsigned index=v->candidate>=0&&(unsigned)v->candidate<v->status.peer_count?(unsigned)v->candidate:0;
            const uint8_t *mac=v->status.peers[index];snprintf(line,sizeof(line),"%s %02X%02X",text(v,C_BADGE),mac[4],mac[5]);
            button(v,line,SW_ICON_USER,66,168,228,A_PEER+(int)index,SW_TONE_TEXT);
            if(v->status.peer_count>1)button(v,library->locale==CV_EN?"Next badge":"下一台徽章",SW_ICON_RIGHT,66,240,228,A_PEER+CV_PEERS,SW_TONE_TEXT);
            else label(v,text(v,C_NO_PEERS),60,266,240,SW_FONT_CAPTION,SW_TONE_MUTED);
        }break;
    case V_WAIT:
        title(v,C_WAIT);label(v,text(v,C_ASK_PEER),60,128,240,SW_FONT_BODY,SW_TONE_MUTED);
        snprintf(line,sizeof(line),"%s %02X%02X",text(v,C_MY_BADGE),v->status.own[4],v->status.own[5]);
        label(v,line,60,190,240,SW_FONT_HEADLINE,SW_TONE_TEXT);label(v,text(v,C_KEEP_NEAR),60,264,240,SW_FONT_CAPTION,SW_TONE_MUTED);break;
    case V_PAIR:
        title(v,C_PAIR);snprintf(line,sizeof(line),"%06lu",(unsigned long)v->status.code);
        label(v,line,60,126,240,SW_FONT_NUM_M,SW_TONE_TEXT);label(v,text(v,C_COMPARE),60,192,240,SW_FONT_CAPTION,SW_TONE_MUTED);
        {lv_obj_t *accept=button(v,text(v,C_ACCEPT),SW_ICON_NONE,66,240,228,A_CONFIRM,SW_TONE_ACCENT);if(v->status.phase==L_WAIT)lv_obj_add_state(accept,LV_STATE_DISABLED);}break;
    case V_PROGRESS:
        title(v,v->status.mode==CV_SEND_MODE?C_SENDING:C_RECEIVING);
        v->ring=sw_ring(v->content,104,(uint8_t)v->status.progress,SW_TONE_ACCENT);lv_obj_set_pos(v->ring,128,123);
        snprintf(line,sizeof(line),"%u%%",v->status.progress);v->percent=label(v,line,130,158,100,SW_FONT_NUM_S,SW_TONE_TEXT);
        label(v,text(v,C_KEEP_NEAR),60,255,240,SW_FONT_CAPTION,SW_TONE_MUTED);break;
    case V_DONE:
        label(v,text(v,v->status.mode==CV_SEND_MODE?C_SENT:C_SAVED),60,70,240,SW_FONT_TITLE,SW_TONE_TEXT);
        thumbnail(v,134,119,92);
        if(v->status.mode==CV_RECEIVE_MODE){button(v,text(v,C_LATER),SW_ICON_NONE,66,240,108,A_DONE,SW_TONE_TEXT);button(v,text(v,C_SHOW),SW_ICON_NONE,186,240,108,A_SHOW,SW_TONE_ACCENT);}
        else button(v,text(v,C_DONE),SW_ICON_CHECK,66,240,228,A_DONE,SW_TONE_ACCENT);break;
    case V_ERROR:
        title(v,C_RETRY);label(v,cv_error_text(library->locale,v->error),66,132,228,SW_FONT_BODY,SW_TONE_TEXT);
        label(v,text(v,C_BACK_SWIPE),60,260,240,SW_FONT_BODY,SW_TONE_MUTED);break;
    }
    lv_obj_update_layout(v->screen);
}
static void go_back(cv_view_t *v)
{
    cv_page_t destination;
    switch(v->page) {
    case V_PHONE: case V_SHARE: case V_ERROR: destination=V_ACTIONS;break;
    case V_PEERS: case V_WAIT: case V_PAIR: case V_PROGRESS: destination=V_SHARE;break;
    case V_DONE: destination=V_COVER;break;
    case V_COVER: case V_ACTIONS: return;
    default: abort();
    }
    if(v->status.mode!=CV_IDLE||v->status.changing) {
        if(!v->backend.mode(v->backend.user,CV_IDLE)){v->error=v->status.error!=CV_OK?v->status.error:CV_BUSY;v->page=V_ERROR;v->dirty=true;return;}
        v->closing=true;
    }
    v->page=destination;v->error=CV_OK;v->menu_at=v->now;v->dirty=true;
}
static void action(cv_view_t *v,int id)
{
    cv_error_t error=CV_OK;
    if(id==A_LOCALE){if(!v->backend.request_locale(v->backend.user,v->status.library.locale==CV_EN?CV_ZH:CV_EN))error=CV_BUSY;v->menu_at=v->now;}
    else if(id==A_SECURITY){if(!v->backend.phone_security(v->backend.user,!*v->status.password))error=CV_BUSY;}
    else if(id==A_PHONE){if(v->backend.mode(v->backend.user,CV_PHONE_MODE))v->page=V_PHONE;else error=CV_BUSY;}
    else if(id==A_SHARE)v->page=V_SHARE;
    else if(id==A_SEND||id==A_RECEIVE){if(v->backend.mode(v->backend.user,id==A_SEND?CV_SEND_MODE:CV_RECEIVE_MODE))v->page=id==A_SEND?V_PEERS:V_WAIT;else error=CV_BUSY;}
    else if(id==A_DONE){v->backend.mode(v->backend.user,CV_IDLE);v->page=V_COVER;v->error=CV_OK;}
    else if(id==A_SHOW){error=v->backend.select(v->backend.user,v->status.saved);if(error==CV_OK){v->backend.mode(v->backend.user,CV_IDLE);v->page=V_COVER;}}
    else if(id==A_CONFIRM){if(!v->backend.confirm(v->backend.user))error=CV_BUSY;}
    else if(id==A_PEER+CV_PEERS){v->candidate=(v->candidate+1)%(int)v->status.peer_count;}
    else if(id>=A_PEER&&id<A_PEER+CV_PEERS&&v->status.library.count) {
        if(!v->backend.offer(v->backend.user,v->status.peers[id-A_PEER],v->status.library.photos[v->status.library.selected].id))error=CV_BUSY;
    }
    if(error!=CV_OK){v->error=error;v->page=V_ERROR;}
    v->dirty=true;
}
/* Buffer 1 holds the photo a rightward swipe reveals (selected-1); buffer 2 the leftward one (selected+1). */
static unsigned neighbor_slot(int direction){return direction>0?1:2;}
static void set_buffer(cv_view_t *v,unsigned slot,uint16_t *pixels)
{v->pixels[slot]=pixels;v->descriptors[slot].data=(uint8_t *)pixels;lv_img_cache_invalidate_src(&v->descriptors[slot]);}
/** Decode the neighbor in one swipe direction unless its buffer already holds it. @returns Whether the buffer holds that photo. */
static bool load_neighbor(cv_view_t *v,int direction)
{
    int index=(int)v->status.library.selected-direction;unsigned slot=neighbor_slot(direction);char *tag=v->neighbors[slot-1];
    if(index<0||(unsigned)index>=v->status.library.count)return false;
    const char *id=v->status.library.photos[index].id;if(!strcmp(tag,id))return true;
    *tag=0;if(v->backend.pixels(v->backend.user,id,v->pixels[slot])!=CV_OK)return false;
    strcpy(tag,id);lv_img_cache_invalidate_src(&v->descriptors[slot]);return true;
}
static void animation_done(lv_anim_t *animation)
{
    cv_view_t *v=animation->user_data;
    if(v->committed) {
        /* The revealed buffer becomes the cover and the old cover becomes the opposite neighbor, so neither is decoded again. */
        unsigned revealed=neighbor_slot(v->drag_direction),opposite=neighbor_slot(-v->drag_direction);
        uint16_t *shown=v->pixels[0],*stale=v->pixels[opposite];
        strcpy(v->neighbors[opposite-1],v->loaded);strcpy(v->loaded,v->neighbors[revealed-1]);
        set_buffer(v,0,v->pixels[revealed]);set_buffer(v,opposite,shown);set_buffer(v,revealed,stale);v->neighbors[revealed-1][0]=0;
        lv_img_set_src(v->images[1],&v->descriptors[1]);v->committed=false;
        cv_error_t error=v->backend.select(v->backend.user,v->loaded);if(error!=CV_OK){v->error=error;v->page=V_ERROR;}
    }
    v->animating=false;v->dirty=true;v->drag_ready=false;
}
static void animation_x(void *object,int32_t value){lv_obj_set_x(object,(lv_coord_t)value);}
static void animate(cv_view_t *v,lv_obj_t *object,int destination,bool last)
{
    lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,object);lv_anim_set_exec_cb(&a,animation_x);
    lv_anim_set_values(&a,lv_obj_get_x(object),destination);lv_anim_set_time(&a,180);lv_anim_set_path_cb(&a,lv_anim_path_ease_out);
    if(last){a.user_data=v;lv_anim_set_ready_cb(&a,animation_done);}lv_anim_start(&a);
}
static void input(lv_event_t *event)
{
    cv_view_t *v=lv_event_get_user_data(event);lv_indev_t *indev=lv_indev_get_act();if(!indev||v->paused||v->animating||v->closing)return;
    lv_point_t p;lv_indev_get_point(indev,&p);lv_event_code_t code=lv_event_get_code(event);
    if(code==LV_EVENT_PRESSED) {
        v->pointer=indev;
        int id=sw_event_action(event);cv_gesture_press(&v->gesture,p.x,p.y,id);
        lv_obj_t *target=lv_event_get_target(event);v->background_press=id<0&&(target==v->screen||target==v->content);
        v->pressed=id>=0?lv_event_get_target(event):NULL;
        while(v->pressed&&!lv_obj_has_flag(v->pressed,LV_OBJ_FLAG_USER_1))v->pressed=lv_obj_get_parent(v->pressed);
        if(v->pressed)lv_obj_get_coords(v->pressed,&v->press_area);
        v->drag_ready=false;v->menu_at=v->now;
    } else if(code==LV_EVENT_PRESS_LOST){cancel_press(v);v->dirty=true;}
    else if(code==LV_EVENT_PRESSING&&v->gesture.held) {
        cv_gesture_move(&v->gesture,p.x,p.y);
        if(v->gesture.action<0&&(v->page==V_COVER||v->page==V_ACTIONS)&&v->status.library.count>1) {
            int dx=p.x-v->gesture.x,dy=p.y-v->gesture.y;
            int direction=v->gesture.y>=60&&v->gesture.y<=300&&abs(dx)>SDGOODS_TAP_SLOP&&2*abs(dx)>=3*abs(dy)?(dx<0?-1:1):0;
            if(direction&&(!v->drag_ready||direction!=v->drag_direction)) {
                if(load_neighbor(v,direction)) {
                    v->candidate=(int)v->status.library.selected-direction;v->drag_direction=direction;v->drag_ready=true;
                    lv_img_set_src(v->images[1],&v->descriptors[neighbor_slot(direction)]);lv_obj_invalidate(v->images[1]);
                }
            }
            if(v->drag_ready){lv_obj_add_flag(v->content,LV_OBJ_FLAG_HIDDEN);int dx=p.x-v->gesture.x;lv_obj_set_x(v->images[0],dx);lv_obj_set_x(v->images[1],dx-v->drag_direction*360);}
        }
    } else if(code==LV_EVENT_RELEASED&&v->gesture.held) {
        cv_gesture_move(&v->gesture,p.x,p.y);int direction=cv_gesture_swipe(&v->gesture,p.x,p.y);
        if(v->page!=V_COVER&&v->page!=V_ACTIONS&&cv_gesture_back(&v->gesture,p.x,p.y)) {
            go_back(v);
        } else if(v->drag_ready) {
            /* Persisting the selection blocks on flash, so it runs after the slide instead of delaying its first frame. */
            bool commit=direction==v->drag_direction;v->committed=commit;
            v->page=v->error==CV_OK?V_COVER:V_ERROR;v->animating=true;
            animate(v,v->images[0],commit?direction*360:0,false);animate(v,v->images[1],commit?0:-v->drag_direction*360,true);
        } else if(v->gesture.action>=0) {
            bool inside=v->pressed&&p.x>=v->press_area.x1&&p.x<=v->press_area.x2&&p.y>=v->press_area.y1&&p.y<=v->press_area.y2;
            if(cv_gesture_tap(&v->gesture,sw_event_action(event),inside))action(v,v->gesture.action);
        } else if(cv_gesture_tap(&v->gesture,-1,true)&&(v->page==V_COVER||v->page==V_ACTIONS)) {v->page=v->page==V_COVER?V_ACTIONS:V_COVER;v->dirty=true;}
        v->gesture.held=false;v->pointer=NULL;v->background_press=false;
    }
}
bool cv_view_open(cv_view_t *v,cv_backend_t backend)
{
    memset(v,0,sizeof(*v));v->backend=backend;v->candidate=0;sw_init();
    for(unsigned i=0;i<3;i++)v->pixels[i]=calloc(CV_PIXELS,2);
    v->qr_pixels=calloc(144*144,2);
    if(!v->pixels[0]||!v->pixels[1]||!v->pixels[2]||!v->qr_pixels){cv_view_close(v);return false;}
    v->screen=sw_screen();lv_obj_add_flag(v->screen,LV_OBJ_FLAG_PRESS_LOCK);
    for(unsigned i=0;i<3;i++)v->descriptors[i]=(lv_img_dsc_t){.header={.cf=LV_IMG_CF_TRUE_COLOR,.w=360,.h=360},.data_size=CV_PIXELS*2,.data=(uint8_t *)v->pixels[i]};
    for(unsigned i=0;i<2;i++){v->images[i]=lv_img_create(v->screen);lv_img_set_src(v->images[i],&v->descriptors[i]);lv_obj_set_x(v->images[i],i?360:0);}
    v->content=sw_column(v->screen);lv_obj_set_layout(v->content,0);lv_obj_set_size(v->content,360,360);lv_obj_set_pos(v->content,0,0);
    lv_obj_add_flag(v->content,LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(v->screen,input,LV_EVENT_ALL,v);v->dirty=true;return true;
}
void cv_view_tick(cv_view_t *v,uint64_t now)
{
    if(!v->screen||v->paused)return;v->now=now;
    cv_status_t status;if(!v->backend.status(v->backend.user,&status))return;
    bool changed=status.library.locale!=v->status.library.locale||status.phase!=v->status.phase||status.stations!=v->status.stations||
        status.peer_count!=v->status.peer_count||memcmp(status.peers,v->status.peers,sizeof(status.peers))||strcmp(status.password,v->status.password)||status.ready!=v->status.ready||status.changing!=v->status.changing||status.locale_pending!=v->status.locale_pending;
    bool photos_changed=status.library.count!=v->status.library.count||status.library.selected!=v->status.library.selected;
    if(status.library.revision!=v->status.library.revision)for(unsigned i=0;!photos_changed&&i<status.library.count;i++)
        photos_changed=strcmp(status.library.photos[i].id,v->status.library.photos[i].id)||status.library.photos[i].size!=v->status.library.photos[i].size;
    changed=changed||photos_changed;
    if(photos_changed&&v->gesture.held){cancel_press(v);changed=true;}
    if(status.locale_result!=v->status.locale_result&&status.locale_error!=CV_OK){v->error=status.locale_error;v->page=V_ERROR;changed=true;}
    v->status=status;
    if(v->animating)return;
    if(v->closing&&!status.changing){v->closing=false;changed=true;if(status.error!=CV_OK){v->error=status.error;v->page=V_ERROR;}}
    if(status.library.count) {
        const char *id=status.library.photos[status.library.selected].id;
        const char *display=v->page==V_DONE&&status.mode==CV_RECEIVE_MODE&&*status.saved?status.saved:id;
        if(strcmp(v->loaded,display)) {
            cv_error_t error=v->backend.pixels(v->backend.user,display,v->pixels[0]);
            if(error==CV_OK){strcpy(v->loaded,display);lv_img_cache_invalidate_src(&v->descriptors[0]);lv_obj_invalidate(v->images[0]);}
            else {v->error=error;v->page=V_ERROR;changed=true;}
        }
    }
    if((v->page==V_COVER||v->page==V_ACTIONS)&&!v->gesture.held&&!v->dirty&&!changed)
        for(int direction=1;direction>=-1;direction-=2) {
            int index=(int)status.library.selected-direction;
            if(index>=0&&(unsigned)index<status.library.count&&strcmp(v->neighbors[neighbor_slot(direction)-1],status.library.photos[index].id)){load_neighbor(v,direction);break;}
        }
    bool wireless=v->page==V_PHONE||v->page==V_PEERS||v->page==V_WAIT||v->page==V_PAIR||v->page==V_PROGRESS;
    if(wireless) {
        if(status.error!=CV_OK){v->error=status.error;v->page=V_ERROR;changed=true;}
        else if(status.phase==L_CONFIRM||status.phase==L_WAIT){if(v->page!=V_PAIR){v->page=V_PAIR;changed=true;}}
        else if(status.phase==L_SEND||status.phase==L_RECEIVE){if(v->page!=V_PROGRESS){v->page=V_PROGRESS;changed=true;}}
        else if(status.phase==L_DONE){v->page=V_DONE;changed=true;}
    }
    if(v->page==V_ACTIONS&&now-v->menu_at>7000){v->page=V_COVER;changed=true;}
    if(changed)v->dirty=true;
    if(v->dirty){lv_obj_clear_flag(v->content,LV_OBJ_FLAG_HIDDEN);render(v);}
    else if(v->page==V_PROGRESS&&v->ring&&v->percent){sw_ring_set(v->ring,(uint8_t)status.progress);char value[12];snprintf(value,sizeof(value),"%u%%",status.progress);lv_label_set_text(v->percent,value);}
}
void cv_view_pause(cv_view_t *v,bool paused)
{
    v->paused=paused;cancel_press(v);
    if(v->screen){for(unsigned i=0;i<2;i++)lv_anim_del(v->images[i],NULL);v->animating=false;v->drag_ready=false;v->committed=false;v->page=V_COVER;v->dirty=true;}
}
void cv_view_close(cv_view_t *v)
{
    if(v->screen){if(lv_scr_act()==v->screen)lv_scr_load(lv_obj_create(NULL));lv_obj_del(v->screen);v->screen=NULL;}
    for(unsigned i=0;i<3;i++){free(v->pixels[i]);v->pixels[i]=NULL;}
    free(v->qr_pixels);v->qr_pixels=NULL;
}
