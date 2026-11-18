/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "pet_view.h"
#include "pet_animation.h"
#include "pet_art.h"
#include "pet_controls.h"
#include "pet_gestures.h"
#include "sugarat_watch.h"
#include <stdio.h>
#include <string.h>
#define INK 0x605341
#define CREAM 0xfffaf0
static const pet_action_t actions[] = {PET_TOUCH,PET_FEED,PET_PLAY,PET_SLEEP,PET_STATUS,PET_WAKE,PET_BACK,PET_CLEAN,
    PET_FAMILY,PET_NEARBY,PET_NEXT,PET_SELECT,PET_EGG,PET_CONFIRM,PET_GREET,PET_GIFT,PET_TOGETHER,PET_BREED};
static void block(pet_view_t *v, int x, int y, int w, int h, uint16_t c)
{
    for (int j = y; j < y + h && j < 180; j++) for (int i = x; i < x + w && i < 180; i++)
        if (i >= 0 && j >= 0) v->pixels[j * 180 + i] = c;
}
static void oval(pet_view_t *v, int cx, int cy, int rx, int ry, uint16_t c)
{
    for (int y = -ry; y <= ry; y++) for (int x = -rx; x <= rx; x++)
        if (x*x*ry*ry + y*y*rx*rx <= rx*rx*ry*ry) block(v,cx+x,cy+y,1,1,c);
}
static uint16_t tint(uint16_t c, unsigned trait)
{
    if (!trait) return c;
    int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    if (trait & 1) { r += r < 30 ? 1 : 0; b -= b > 1 ? 1 : 0; }
    if (trait & 2) { g += g < 61 ? 2 : 0; r -= r > 1 ? 1 : 0; }
    return (uint16_t)(r << 11 | g << 5 | b);
}
static void character(pet_view_t *v, const pet_profile_t *p, int cx, int baseline, unsigned size, bool animate)
{
    pet_kind_t kind = pet_profile_kind(p);
    unsigned frame = animate ? pet_animation_frame(v->model,p) : (v->model->animation_ms / 240) % 4;
    if (!pet_animation_decode(kind,frame,v->frame_pixels)) return;
    unsigned width = size + size * (p->traits[PET_BODY] % 3) / 24;
    int x0 = cx - (int)width/2, y0 = baseline - (int)size;
    if (animate && v->model->pet.asleep) y0 += (v->model->animation_ms / 700) % 2;
    const uint16_t *palette = pet_art_characters[kind].palette;
    for (unsigned y=0;y<size;y++) for (unsigned x=0;x<width;x++) {
        unsigned index = v->frame_pixels[(y*96/size)*96+x*96/width];
        if (index) {
            uint16_t color = tint(palette[index],p->traits[PET_COLOR]);
            /* Inherited forehead markings are anchored in the opaque body. */
            if (p->traits[PET_PATTERN] % 3 && y > size*40/100 && y < size*44/100 && x > width*46/100 && x < width*54/100)
                color = __builtin_bswap16(p->traits[PET_PATTERN] % 3 == 1 ? EPET_RGB(250,235,167) : EPET_RGB(158,127,172));
            block(v,x0+(int)x,y0+(int)y,1,1,__builtin_bswap16(color));
        }
    }
}
static void world(pet_view_t *v)
{
    pet_model_t *m=v->model;
    bool night=m->pet.asleep && v->page==PET_VIEW_CARE;
    block(v,0,0,180,180,night?EPET_RGB(31,46,72):EPET_RGB(255,250,240));
    bool stats=v->page==PET_VIEW_CARE && m->page==PET_STATS;
    if(stats) return;
    oval(v,38,178,100,66,night?EPET_RGB(42,66,75):EPET_RGB(234,240,222));
    oval(v,151,185,110,65,night?EPET_RGB(44,70,65):EPET_RGB(227,235,212));
    oval(v,90,v->page==PET_VIEW_CARE?143:v->page==PET_VIEW_FAMILY?134:110,34,4,night?EPET_RGB(28,43,58):EPET_RGB(213,224,192));
    pet_profile_t fallback={.blood={0,0},.generation=1};
    const pet_profile_t *p=v->family?&v->family->members[v->family->active].profile:&fallback;
    if(v->page==PET_VIEW_FAMILY) {
        if(v->starter) { fallback.blood[0]=fallback.blood[1]=(uint8_t)(v->selection%3); p=&fallback; }
        else p=&v->family->members[v->selection%v->family->count].profile;
    }
    if(v->page==PET_VIEW_NEARBY && v->social) {
        pet_social_t *s=v->social;
        if(s->state==PET_SOCIAL_VERIFY || s->state==PET_SOCIAL_KEY) return;
        if(s->state==PET_SOCIAL_LINKED) {
            character(v,&s->self,57,112,64,false); character(v,&s->peer,123,112,64,false); return;
        }
        if(s->nearby_count) {
            unsigned k=s->nearby[v->selection%s->nearby_count].kind;
            static const uint8_t blood[6][2]={{0,0},{1,1},{2,2},{0,1},{0,2},{1,2}};
            memcpy(fallback.blood,blood[k],2); p=&fallback;
        }
    }
    if(v->family && ((v->family->tending_egg && v->page==PET_VIEW_CARE) || (v->page==PET_VIEW_FAMILY && !v->starter && v->family->has_egg && v->selection%(v->family->count+1)==v->family->count))) {
        int wobble=(int)((m->animation_ms/260)%3)-1;
        int cy=v->page==PET_VIEW_CARE?108:104,rx=v->page==PET_VIEW_CARE?28:21,ry=v->page==PET_VIEW_CARE?36:27;
        oval(v,90+wobble,cy,rx,ry,EPET_RGB(241,207,159)); oval(v,90+wobble,cy-1,rx-3,ry-3,EPET_RGB(255,241,197));
        for(int i=0;i<3;i++) oval(v,82+i*8+wobble,cy-7+i*7,3,4,EPET_RGB(173,193,160));
        if(v->family->egg_ms>=PET_EGG_TIME_MS) { block(v,88+wobble,cy-20,2,10,EPET_RGB(145,118,86)); block(v,90+wobble,cy-11,5,2,EPET_RGB(145,118,86)); }
        return;
    }
    unsigned size=epet_age_level(&m->pet)<11?88:epet_age_level(&m->pet)<31?96:100;
    if(v->page!=PET_VIEW_CARE) size=v->page==PET_VIEW_FAMILY?80:64;
    character(v,p,90,v->page==PET_VIEW_CARE?145:v->page==PET_VIEW_FAMILY?136:112,size,v->page==PET_VIEW_CARE);
    if(night) { block(v,124,60,6,1,EPET_RGB(235,226,190)); block(v,128,61,1,3,EPET_RGB(235,226,190)); block(v,124,65,6,1,EPET_RGB(235,226,190)); }
    for(unsigned i=0;i<m->pet.poop && v->page==PET_VIEW_CARE;i++) oval(v,36+(int)i*7,145,3,2,EPET_RGB(132,94,68));
}
static lv_obj_t *label(lv_obj_t *p,int x,int y,int w,const char *text,sw_font_t font)
{
    lv_obj_t *o=lv_label_create(p); lv_obj_set_pos(o,x,y); lv_obj_set_width(o,w);
    lv_obj_set_style_text_font(o,sw_font(font),0); lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
    lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP); lv_label_set_text(o,text); lv_obj_clear_flag(o,LV_OBJ_FLAG_CLICKABLE); return o;
}
static bool tray_button(unsigned i) {return i==1||i==2||i==3||i==4||i==5||i==8||i==9;}
static void clicked(void *context)
{
    pet_button_binding_t *binding=context;pet_view_t *v=binding->view;unsigned i=binding->index;
    if(v->gesture.moved||v->controls_animating||v->controls_dragging)return;
    if(v->family_animating)pet_gestures_dispose(v);
    if(tray_button(i)) {
        if(!v->controls_open)return;
        pet_controls_dismiss(v);
    }
    v->input(v->context,actions[i]);
    pet_view_update(v,v->save_notice);
}
static void button(pet_view_t *v,unsigned i,int x,int y,int w,int h,const char *text)
{
    lv_obj_t *o=lv_btn_create(v->screen); v->buttons[i]=o; lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h); lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(o,lv_color_hex(0xf3e8cf),0); lv_obj_set_style_bg_opa(o,i?LV_OPA_COVER:LV_OPA_TRANSP,0);
    lv_obj_set_style_bg_color(o,lv_color_hex(0xe2d4b7),LV_STATE_PRESSED); lv_obj_set_style_radius(o,12,0);
    if(text[0]) {lv_obj_t *t=label(o,0,0,w,text,SW_FONT_CAPTION);lv_obj_set_style_text_color(t,lv_color_hex(INK),0);lv_obj_center(t);}
    v->bindings[i]=(pet_button_binding_t){v,i};sdgoods_tap_bind(o,&v->taps[i],clicked,&v->bindings[i]);
}
static void visible(lv_obj_t *o,bool show) {if(show)lv_obj_clear_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);}
void pet_view_create(pet_view_t *v,pet_model_t *m,uint16_t *pixels,pet_input_fn input,void *context)
{
    memset(v,0,sizeof(*v));v->model=m;v->pixels=pixels;v->input=input;v->context=context;
    v->screen=lv_obj_create(NULL);lv_obj_remove_style_all(v->screen);lv_obj_clear_flag(v->screen,LV_OBJ_FLAG_SCROLLABLE);
    sdgoods_tap_lock(v->screen);
    lv_obj_set_style_bg_color(v->screen,lv_color_hex(CREAM),0);lv_obj_set_style_bg_opa(v->screen,LV_OPA_COVER,0);
    v->image=(lv_img_dsc_t){.header={.cf=LV_IMG_CF_TRUE_COLOR,.w=180,.h=180},.data_size=180*180*2,.data=(const uint8_t *)pixels};
    v->world=lv_img_create(v->screen);lv_img_set_src(v->world,&v->image);lv_img_set_pivot(v->world,0,0);lv_img_set_zoom(v->world,512);lv_img_set_antialias(v->world,false);
    lv_obj_clear_flag(v->world,LV_OBJ_FLAG_CLICKABLE);
    v->title=label(v->screen,64,60,160,"",SW_FONT_HEADLINE);v->level=label(v->screen,60,88,240,"",SW_FONT_CAPTION);
    v->notice=label(v->screen,60,88,240,"",SW_FONT_CAPTION);v->message=label(v->screen,60,224,240,"",SW_FONT_CAPTION);
    button(v,0,76,106,208,180,"");button(v,1,60,254,76,44,"Feed");button(v,2,142,254,76,44,"Play");button(v,3,224,254,76,44,"Sleep");
    button(v,4,232,60,68,30,"Stats");button(v,5,124,260,112,38,"Wake up");button(v,6,64,268,84,32,"Back");button(v,7,212,268,84,32,"Clean");
    button(v,8,64,222,110,26,"Family");button(v,9,186,222,110,26,"Meet");button(v,10,64,260,100,38,"Next");button(v,11,196,260,100,38,"Choose");
    button(v,12,128,222,104,28,"Egg");button(v,13,112,260,136,38,"Code matches");
    button(v,14,64,232,72,34,"Greet");button(v,15,144,232,72,34,"Gift");button(v,16,224,232,72,34,"Play");button(v,17,120,276,120,32,"Breed egg");
    lv_obj_t *back=v->buttons[6];lv_obj_clean(back);lv_obj_set_style_bg_opa(back,LV_OPA_TRANSP,LV_PART_MAIN);lv_obj_set_style_bg_opa(back,LV_OPA_TRANSP,LV_STATE_PRESSED);
    lv_obj_set_size(back,72,72);lv_obj_t *pad=lv_obj_create(back);lv_obj_remove_style_all(pad);lv_obj_set_pos(pad,4,4);lv_obj_set_size(pad,64,64);
    lv_obj_set_style_radius(pad,32,0);lv_obj_set_style_bg_color(pad,lv_color_hex(0xfffcf6),0);lv_obj_set_style_bg_opa(pad,LV_OPA_COVER,0);
    lv_obj_set_style_border_width(pad,1,0);lv_obj_set_style_border_color(pad,lv_color_hex(0xe9dfcf),0);lv_obj_clear_flag(pad,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *arrow=sw_icon(pad,SW_ICON_LEFT,SW_SIZE_M,SW_TONE_TEXT);lv_obj_set_style_text_color(arrow,lv_color_hex(INK),0);lv_obj_align(arrow,LV_ALIGN_TOP_MID,0,8);lv_obj_clear_flag(arrow,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *back_text=label(pad,0,36,64,"Back",SW_FONT_CAPTION);lv_obj_set_style_text_color(back_text,lv_color_hex(INK),0);
    lv_obj_t *choose=v->buttons[11],*select=lv_obj_get_child(choose,0);lv_obj_set_pos(choose,104,303);lv_obj_set_size(choose,152,56);
    lv_obj_set_style_bg_opa(choose,LV_OPA_TRANSP,0);lv_obj_set_style_bg_opa(choose,LV_OPA_TRANSP,LV_STATE_PRESSED);
    lv_obj_t *confirm=lv_obj_create(choose);lv_obj_remove_style_all(confirm);lv_obj_set_pos(confirm,8,4);lv_obj_set_size(confirm,136,44);
    lv_obj_set_style_bg_color(confirm,lv_color_hex(0xcad9bb),0);lv_obj_set_style_bg_opa(confirm,LV_OPA_COVER,0);lv_obj_set_style_radius(confirm,22,0);lv_obj_clear_flag(confirm,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_parent(select,confirm);lv_label_set_text(select,"Select");lv_obj_set_width(select,72);lv_obj_set_align(select,LV_ALIGN_TOP_LEFT);lv_obj_set_pos(select,46,12);
    lv_obj_t *check=sw_icon(confirm,SW_ICON_CHECK,SW_SIZE_M,SW_TONE_TEXT);lv_obj_set_style_text_color(check,lv_color_hex(0x3f5532),0);lv_obj_set_pos(check,21,10);lv_obj_clear_flag(check,LV_OBJ_FLAG_CLICKABLE);
    for(unsigned i=0;i<7;i++) {v->dots[i]=lv_obj_create(v->screen);lv_obj_remove_style_all(v->dots[i]);lv_obj_set_style_radius(v->dots[i],3,0);lv_obj_set_style_bg_opa(v->dots[i],LV_OPA_COVER,0);lv_obj_clear_flag(v->dots[i],LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);}
    static const char *names[]={"Fullness","Mood","Energy","Hygiene","Health"};
    for(unsigned i=0;i<5;i++) {
        int y=108+(int)i*27;v->labels[i]=label(v->screen,64,y,82,names[i],SW_FONT_CAPTION);
        v->bars[i]=lv_bar_create(v->screen);lv_obj_set_pos(v->bars[i],148,y+2);lv_obj_set_size(v->bars[i],112,13);
        lv_obj_set_style_bg_color(v->bars[i],lv_color_hex(0x91a77c),LV_PART_INDICATOR);
        v->values[i]=label(v->screen,262,y,34,"",SW_FONT_CAPTION);
    }
    v->stage=label(v->screen,64,276,232,"Baby > Growing > Adult",SW_FONT_CAPTION);pet_controls_create(v);pet_gestures_create(v);pet_view_update(v,NULL);
}
void pet_view_attach(pet_view_t *v,pet_family_t *f,pet_social_t *s) {v->family=f;v->social=s;}
void pet_view_update(pet_view_t *v,const char *notice)
{
    pet_model_t *m=v->model;bool care=v->page==PET_VIEW_CARE,stats=care&&m->page==PET_STATS,sleep=m->pet.asleep;
    bool egg=care&&v->family&&v->family->tending_egg,feeding=care&&m->page==PET_FEEDING&&!egg;
    v->save_notice=notice;
    if(v->controls_animating||v->controls_dragging||v->family_animating||(v->gesture.active&&v->gesture.family&&v->gesture.moved))return;
    if(!v->drawer_visible) {world(v);lv_img_cache_invalidate_src(&v->image);lv_obj_invalidate(v->world);}
    for(unsigned i=0;i<18;i++) visible(v->buttons[i],false);
    for(unsigned i=0;i<5;i++) {visible(v->labels[i],stats);visible(v->bars[i],stats);visible(v->values[i],stats);}
    visible(v->stage,false);visible(v->level,!notice);visible(v->notice,notice!=NULL);
    lv_label_set_text(v->notice,notice?notice:"");lv_obj_set_style_text_color(v->notice,lv_color_hex(0xa44a38),0);
    lv_obj_set_style_text_color(v->title,lv_color_hex(sleep&&care&&!stats?CREAM:INK),0);
    lv_obj_set_style_text_color(v->level,lv_color_hex(sleep&&care&&!stats?CREAM:INK),0);
    lv_obj_set_style_text_color(v->message,lv_color_hex(sleep&&care&&!stats?CREAM:INK),0);
    lv_obj_set_pos(v->level,60,88);lv_obj_set_style_text_font(v->level,sw_font(SW_FONT_CAPTION),0);
    lv_obj_set_pos(v->title,care&&!stats?60:64,60);lv_obj_set_width(v->title,care&&!stats?240:160);lv_obj_set_pos(v->message,care&&!stats?80:60,care&&!stats?296:224);lv_obj_set_width(v->message,care&&!stats?200:240);
    visible(v->grabber,care&&!stats&&!v->drawer_visible);
    lv_obj_set_pos(v->buttons[6],44,58);
    if(stats) {lv_obj_set_pos(v->title,120,72);lv_obj_set_width(v->title,180);lv_obj_set_pos(v->level,120,101);lv_obj_set_width(v->level,180);}
    else lv_obj_set_width(v->level,240);
    lv_obj_set_pos(v->notice,care&&!stats?60:120,care&&!stats?88:102);lv_obj_set_width(v->notice,care&&!stats?240:180);
    for(unsigned i=0;i<7;i++)visible(v->dots[i],false);
    char title[64],detail[128],message[160];message[0]=0;
    const pet_profile_t *p=v->family?&v->family->members[v->family->active].profile:NULL;
    snprintf(title,sizeof(title),"%s",p?pet_kind_name(pet_profile_kind(p)):"Ember");
    snprintf(detail,sizeof(detail),"Lv. %u  /  %s",epet_age_level(&m->pet),pet_model_stage(m));
    if(care) {
        if(stats) {snprintf(title,sizeof(title),"Care stats");visible(v->buttons[6],true);visible(v->buttons[7],!sleep);}
        else if(egg) {
            snprintf(title,sizeof(title),"Your new egg");snprintf(detail,sizeof(detail),"%u / 30 min together",(unsigned)(v->family->egg_ms/60000));
            snprintf(message,sizeof(message),"%s",v->family->pending.phase?"Finish pairing with your friend":v->family->egg_ms>=PET_EGG_TIME_MS?"Tap the egg to hatch!":"Staying here helps it hatch");
            visible(v->buttons[0],true);
        } else {
            visible(v->buttons[0],!sleep);
            if(feeding||sleep||m->feedback_ms)snprintf(message,sizeof(message),"%s",sleep?"Resting and recharging":m->message);
            else if(m->pet.health<40)snprintf(message,sizeof(message),"A little care, please");
        }
        if(!stats&&v->drawer_visible) {
            for(unsigned i=1;i<10;i++)if(tray_button(i)) {
                bool show=i!=3&&i!=5?true:(i==5)==sleep;visible(v->buttons[i],show);
                bool enabled=v->controls_open&&!egg&&(!sleep||i==5||i==4||i==8);
                if(i==8)enabled=v->controls_open&&v->family;
                if(i==9)enabled=enabled&&v->family;
                if(enabled)lv_obj_clear_state(v->buttons[i],LV_STATE_DISABLED);else lv_obj_add_state(v->buttons[i],LV_STATE_DISABLED);
            }

        }
    } else if(v->page==PET_VIEW_FAMILY) {
        unsigned total=v->family->count+(unsigned)v->family->has_egg;
        unsigned index=v->starter?v->selection%3:v->selection%total;
        bool selected_egg=!v->starter&&index==v->family->count;
        const pet_profile_t *chosen=&v->family->members[index<v->family->count?index:0].profile;
        snprintf(title,sizeof(title),"%s",v->starter?"First friend":"Your family");
        snprintf(detail,sizeof(detail),"%s  %u / %u",v->starter?pet_kind_name((pet_kind_t)index):pet_kind_name(pet_profile_kind(chosen)),index+1,v->starter?3:total);
        if(selected_egg)snprintf(detail,sizeof(detail),"Egg %u / %u",index+1,total);
        if(v->starter)snprintf(message,sizeof(message),"Swipe to meet your first friend");
        else snprintf(message,sizeof(message),"Gen %u  |  %s",chosen->generation,chosen->age_ms>=900000?"Adult":chosen->age_ms>=300000?"Growing":"Baby");
        if(selected_egg)snprintf(message,sizeof(message),"%u / 30 min together",(unsigned)(v->family->egg_ms/60000));
        lv_obj_set_pos(v->title,116,68);lv_obj_set_width(v->title,184);lv_obj_set_pos(v->level,116,99);lv_obj_set_width(v->level,184);
        if(v->starter) {lv_obj_set_x(v->title,60);lv_obj_set_width(v->title,240);lv_obj_set_x(v->level,60);lv_obj_set_width(v->level,240);}
        lv_obj_set_pos(v->message,60,267);lv_obj_set_width(v->message,240);visible(v->buttons[11],true);visible(v->buttons[6],!v->starter);
        unsigned count=v->starter?3:total;int width=(int)count*13+6;
        for(unsigned i=0;i<count;i++) {lv_obj_t *dot=v->dots[i];visible(dot,true);lv_obj_set_pos(dot,180-width/2+(int)i*13+(i>index?12:0),290);lv_obj_set_size(dot,i==index?18:6,6);lv_obj_set_style_bg_color(dot,lv_color_hex(i==index?0x3f5532:0xcbd1bd),0);}

    } else if(v->social) {
        pet_social_t *s=v->social;visible(v->buttons[6],true);snprintf(title,sizeof(title),"Nearby");
        lv_obj_set_pos(v->title,120,72);lv_obj_set_width(v->title,180);lv_obj_set_pos(v->level,120,103);lv_obj_set_width(v->level,180);
        snprintf(detail,sizeof(detail),"%s",s->notice?s->notice:"Start nearby discovery");
        if(s->state==PET_SOCIAL_SEARCH) {
            snprintf(message,sizeof(message),s->nearby_count?"%s  %u / %u\nSelect each other on both devices":"Searching for nearby devices...",s->nearby_count?pet_kind_name((pet_kind_t)s->nearby[v->selection%s->nearby_count].kind):"",s->nearby_count?v->selection%s->nearby_count+1:0,s->nearby_count);
            visible(v->buttons[10],s->nearby_count>1);visible(v->buttons[11],s->nearby_count>0);
        } else if(s->state==PET_SOCIAL_VERIFY) {
            snprintf(title,sizeof(title),"Same friend?");snprintf(detail,sizeof(detail),"%06lu",(unsigned long)s->code);
            lv_obj_set_pos(v->level,60,150);lv_obj_set_width(v->level,240);lv_obj_set_style_text_font(v->level,sw_font(SW_FONT_NUM_M),0);
            snprintf(message,sizeof(message),"Check the code on both screens");visible(v->buttons[13],!s->confirmed);
        } else if(s->state==PET_SOCIAL_LINKED) {
            pet_friend_t *f=pet_family_friend(v->family,&s->peer,false);
            snprintf(message,sizeof(message),"Friendship: %u / 3",f?(unsigned)((f->interactions&1)!=0)+(unsigned)((f->interactions&2)!=0)+(unsigned)((f->interactions&4)!=0):0);
            lv_obj_set_y(v->message,210);for(unsigned i=14;i<=17;i++)visible(v->buttons[i],true);
            bool pending=v->family->pending.phase!=PET_TX_NONE;
            lv_label_set_text(lv_obj_get_child(v->buttons[17],0),pending?"Syncing...":v->family->has_egg?"Egg saved":"Breed egg");
            if(pending||v->family->has_egg)lv_obj_add_state(v->buttons[17],LV_STATE_DISABLED);
            else lv_obj_clear_state(v->buttons[17],LV_STATE_DISABLED);
        }
    }
    lv_label_set_text(v->title,title);lv_label_set_text(v->level,detail);lv_label_set_text(v->message,message);
    float amounts[]={100-m->pet.hunger,m->pet.happiness,m->pet.energy,m->pet.hygiene,m->pet.health};
    for(unsigned i=0;i<5;i++) {
        int y=140+(int)i*26;lv_obj_set_pos(v->labels[i],feeding?88:64,feeding?252:y);lv_obj_set_pos(v->values[i],feeding?238:262,feeding?252:y);
        lv_obj_set_pos(v->bars[i],feeding?88:148,feeding?278:y+2);lv_obj_set_width(v->bars[i],feeding?184:112);
        lv_bar_set_value(v->bars[i],(int)amounts[i],LV_ANIM_OFF);char n[8];snprintf(n,sizeof(n),"%d",(int)amounts[i]);lv_label_set_text(v->values[i],n);
    }
}
void pet_view_destroy(pet_view_t *v) {pet_controls_dispose(v);pet_gestures_dispose(v);lv_img_cache_invalidate_src(&v->image);lv_obj_del(v->screen);memset(v,0,sizeof(*v));}
