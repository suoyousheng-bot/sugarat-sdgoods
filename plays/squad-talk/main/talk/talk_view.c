/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Squad Talk's cream/lavender swipe-channel display and bottom fingerprint hold target. */
#include "talk_view.h"
#include "sugarat_game.h"
#include <stdio.h>
#include <string.h>
extern const lv_font_t squad_font_14,squad_font_18,squad_font_22,squad_font_26,squad_font_30;
static void text(lv_obj_t *label,const char *value)
{if(strcmp(lv_label_get_text(label),value))lv_label_set_text(label,value);}
static void place(lv_obj_t *object,int x,int y,int w,int h)
{lv_obj_set_pos(object,x,y);lv_obj_set_size(object,w,h);}
static void no_touch(lv_obj_t *object)
{lv_obj_clear_flag(object,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);}
static lv_obj_t *shape(lv_obj_t *parent,int x,int y,int w,int h,sg_tone_t tone,int radius)
{
    lv_obj_t *o=lv_obj_create(parent);lv_obj_remove_style_all(o);place(o,x,y,w,h);no_touch(o);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_bg_color(o,sg_color(tone),0);
    lv_obj_set_style_border_width(o,2,0);lv_obj_set_style_border_color(o,sg_color(SG_TONE_TEXT),0);
    lv_obj_set_style_radius(o,radius,0);return o;
}
void talk_view_create(talk_view_t *v)
{
    memset(v,0,sizeof(*v));sg_init();v->screen=sg_screen();
    lv_obj_t *title=sg_label(v->screen,"SQUAD",SG_FONT_TITLE,SG_TONE_TEXT);
    lv_obj_align(title,LV_ALIGN_TOP_MID,9,24);
    v->online=shape(v->screen,111,35,14,14,SG_TONE_MUTED,LV_RADIUS_CIRCLE);
    lv_obj_t *capsule=sg_row(v->screen);lv_obj_set_layout(capsule,0);place(capsule,109,60,142,44);
    sg_box(capsule,SG_BOX_BUTTON,SG_TONE_TEXT,false);no_touch(capsule);
    v->channel=sg_label(capsule,"CH 01",SG_FONT_BODY,SG_TONE_BG);
    lv_obj_set_style_text_font(v->channel,&squad_font_30,0);lv_obj_center(v->channel);
    lv_obj_t *hint=sg_label(v->screen,"SWIPE TO CHANGE",SG_FONT_CAPTION,SG_TONE_MUTED);
    lv_obj_set_style_text_font(hint,&squad_font_14,0);lv_obj_align(hint,LV_ALIGN_TOP_MID,0,108);
    v->status=sg_label(v->screen,"FIND A BUDDY",SG_FONT_BODY,SG_TONE_TEXT);
    lv_obj_align(v->status,LV_ALIGN_TOP_MID,0,132);
    v->buddy=sg_label(v->screen,"POWER ON YOUR BUDDY",SG_FONT_CAPTION,SG_TONE_TEXT);
    lv_obj_set_style_text_font(v->buddy,&squad_font_14,0);lv_obj_align(v->buddy,LV_ALIGN_TOP_MID,0,158);
    v->linkdot=shape(v->screen,116,165,10,10,SG_TONE_ACCENT,LV_RADIUS_CIRCLE);
    v->ptt=sg_touch_target(v->screen);lv_obj_set_pos(v->ptt,70,184);
    v->ptt_text=sg_label(v->ptt,"FIND BUDDY",SG_FONT_BODY,SG_TONE_TEXT);
    lv_obj_set_style_text_line_space(v->ptt_text,-3,0);lv_obj_set_style_text_align(v->ptt_text,LV_TEXT_ALIGN_CENTER,0);
    no_touch(v->ptt_text);lv_obj_align(v->ptt_text,LV_ALIGN_TOP_MID,0,121);
}
void talk_view_update(talk_view_t *v,const talk_status_t *s)
{
    const talk_status_t *last=&v->rendered;
    bool same=v->rendered_valid&&last->phase==s->phase&&last->channel==s->channel&&
        last->online==s->online&&last->link_failed==s->link_failed&&last->fault==s->fault;
    v->rendered=*s;v->rendered_valid=true;
    if(same){
        sg_touch_t mode=s->fault!=TALK_FAULT_NONE?SG_TOUCH_ERROR:s->phase==TALK_READY&&!s->link_failed?SG_TOUCH_IDLE:
            s->phase==TALK_TRANSMITTING?SG_TOUCH_HELD:s->phase==TALK_RECEIVING?SG_TOUCH_RECEIVING:SG_TOUCH_WAITING;
        sg_touch_target_set(v->ptt,mode,s->fault==TALK_FAULT_NONE?s->level:0);
        return;
    }

    char channel[16];snprintf(channel,sizeof(channel),"CH %02u",s->channel);text(v->channel,channel);lv_obj_center(v->channel);
    const char *status="FIND A BUDDY",*button="FIND BUDDY",*buddy="POWER ON YOUR BUDDY";
    sg_touch_t mode=SG_TOUCH_WAITING;sg_tone_t button_ink=SG_TONE_TEXT;
    switch(s->phase){
    case TALK_READY:status="READY TO ROLL";button="HOLD TO TALK";buddy="BUDDY ONLINE";mode=SG_TOUCH_IDLE;button_ink=SG_TONE_BG;break;
    case TALK_REQUESTING:status="GIVE US A SEC";button="CALLING...";buddy="CONNECTING...";break;
    case TALK_TRANSMITTING:status="YOU ARE ON AIR";button="RELEASE TO\nLISTEN";buddy="";mode=SG_TOUCH_HELD;break;
    case TALK_RECEIVING:status="BUDDY TALKING";button="LISTENING...";buddy="";mode=SG_TOUCH_RECEIVING;break;
    case TALK_PAUSED:status="RADIO PAUSED";button="TAKE A BREAK";buddy="BACK IN A MOMENT";break;
    case TALK_WAITING:break;
    }
    if(s->link_failed&&s->phase!=TALK_TRANSMITTING&&s->phase!=TALK_RECEIVING){status="TRY THAT AGAIN";button="HOLD TO RETRY";buddy="CHECK YOUR BUDDY";}
    if(s->fault!=TALK_FAULT_NONE){status=s->fault==TALK_FAULT_AUDIO?"AUDIO ERROR":"RADIO ERROR";button="RESTART\nAPP";buddy="TAP TO RESTART";mode=SG_TOUCH_ERROR;}
    text(v->status,status);lv_obj_align(v->status,LV_ALIGN_TOP_MID,0,132);
    if(s->phase==TALK_READY&&s->fault==TALK_FAULT_NONE&&!s->link_failed)lv_obj_clear_flag(v->linkdot,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(v->linkdot,LV_OBJ_FLAG_HIDDEN);
    text(v->buddy,buddy);lv_obj_align(v->buddy,LV_ALIGN_TOP_MID,0,158);
    lv_obj_set_style_text_font(v->ptt_text,mode==SG_TOUCH_HELD||mode==SG_TOUCH_ERROR||s->link_failed?&squad_font_18:&squad_font_22,0);
    text(v->ptt_text,button);lv_obj_set_style_text_color(v->ptt_text,sg_color(button_ink),0);
    lv_obj_align(v->ptt_text,LV_ALIGN_TOP_MID,0,mode==SG_TOUCH_HELD||mode==SG_TOUCH_ERROR?114:121);
    sg_touch_target_set(v->ptt,mode,s->fault==TALK_FAULT_NONE?s->level:0);
    lv_obj_set_style_bg_color(v->online,sg_color(s->online?SG_TONE_ACCENT:SG_TONE_MUTED),0);
}
