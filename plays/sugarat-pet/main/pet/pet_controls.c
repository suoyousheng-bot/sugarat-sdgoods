/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "pet_controls.h"
#include "sugarat_watch.h"

static void slide(void *object, int32_t y) { lv_obj_set_y(object, (lv_coord_t)y); }
static void finished(lv_anim_t *animation)
{
    pet_view_t *v=lv_anim_get_user_data(animation);
    v->controls_animating=false;
    if(!v->controls_open) {
        v->drawer_visible=false;lv_obj_add_flag(v->shade,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(v->drawer,LV_OBJ_FLAG_HIDDEN);
    }
    pet_view_update(v,v->save_notice);
}
void pet_view_controls(pet_view_t *v,bool open)
{
    if(open&&(v->model->paused||v->page!=PET_VIEW_CARE||v->model->page==PET_STATS||v->starter))return;
    if(v->controls_open==open&&!v->controls_dragging&&!v->controls_animating)return;
    v->controls_dragging=v->controls_animating=false;
    v->controls_open=open;v->drawer_visible=true;
    lv_obj_clear_flag(v->shade,LV_OBJ_FLAG_HIDDEN);lv_obj_clear_flag(v->drawer,LV_OBJ_FLAG_HIDDEN);
    lv_anim_del(v->drawer,slide);
    pet_view_update(v,v->save_notice);v->controls_animating=true;
    lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,v->drawer);lv_anim_set_exec_cb(&a,slide);
    lv_anim_set_values(&a,lv_obj_get_y(v->drawer),open?150:360);lv_anim_set_time(&a,170);
    lv_anim_set_path_cb(&a,lv_anim_path_ease_out);lv_anim_set_user_data(&a,v);lv_anim_set_ready_cb(&a,finished);lv_anim_start(&a);
}
void pet_controls_drag(pet_view_t *v,int y)
{
    lv_anim_del(v->drawer,slide);v->controls_animating=false;
    if(!v->drawer_visible) {
        v->drawer_visible=true;lv_obj_clear_flag(v->shade,LV_OBJ_FLAG_HIDDEN);lv_obj_clear_flag(v->drawer,LV_OBJ_FLAG_HIDDEN);
        pet_view_update(v,v->save_notice);
    }
    v->controls_dragging=true;lv_obj_set_y(v->drawer,LV_CLAMP(150,y,360));
}
void pet_controls_dismiss(pet_view_t *v)
{
    if(!v->drawer)return;
    lv_anim_del(v->drawer,slide);v->controls_open=v->drawer_visible=v->controls_dragging=v->controls_animating=false;
    lv_obj_set_y(v->drawer,360);lv_obj_add_flag(v->drawer,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(v->shade,LV_OBJ_FLAG_HIDDEN);
}
void pet_controls_dispose(pet_view_t *v) {if(v->drawer)lv_anim_del(v->drawer,slide);}
static void outside(void *context) {pet_view_t *v=context;if(!v->gesture.moved)pet_view_controls(v,false);}
static void open(void *context) {pet_view_t *v=context;if(!v->gesture.moved)pet_view_controls(v,true);}
static void bowl(lv_event_t *event)
{
    lv_obj_t *o=lv_event_get_target(event);lv_area_t a;lv_obj_get_coords(o,&a);
    lv_draw_ctx_t *draw=lv_event_get_draw_ctx(event);
    lv_draw_line_dsc_t line;lv_draw_line_dsc_init(&line);line.width=2;line.round_start=line.round_end=1;line.color=lv_color_hex(0x685744);
    lv_point_t from={a.x1+3,a.y1+13},to={a.x1+25,a.y1+13};lv_draw_line(draw,&line,&from,&to);
    lv_draw_arc_dsc_t arc;lv_draw_arc_dsc_init(&arc);arc.width=2;arc.color=line.color;
    lv_point_t center={a.x1+14,a.y1+13};lv_draw_arc(draw,&arc,&center,11,0,180);
    from=(lv_point_t){a.x1+10,a.y1+26};to=(lv_point_t){a.x1+18,a.y1+26};lv_draw_line(draw,&line,&from,&to);
    for(unsigned i=0;i<2;i++) {from=(lv_point_t){a.x1+10+(int)i*7,a.y1+3};to=(lv_point_t){from.x-2,a.y1+8};lv_draw_line(draw,&line,&from,&to);}
}
static void icon(pet_view_t *v,unsigned index,unsigned slot,sw_icon_t glyph)
{
    lv_obj_t *b=v->buttons[index];lv_obj_set_parent(b,v->drawer);lv_obj_clean(b);
    lv_obj_set_pos(b,62+(int)(slot%3)*82,28+(int)(slot/3)*74);lv_obj_set_size(b,72,64);
    lv_obj_set_style_radius(b,21,0);lv_obj_set_style_bg_color(b,lv_color_hex(0xfffcf6),0);
    lv_obj_set_style_border_width(b,1,0);lv_obj_set_style_border_color(b,lv_color_hex(0xe9dfcf),0);
    lv_obj_set_style_bg_color(b,lv_color_hex(0xf6eee0),LV_STATE_PRESSED);lv_obj_set_style_opa(b,LV_OPA_40,LV_STATE_DISABLED);
    lv_obj_t *image;
    if(glyph==SW_ICON_NONE) {
        image=lv_obj_create(b);lv_obj_remove_style_all(image);lv_obj_set_size(image,28,28);lv_obj_clear_flag(image,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(image,bowl,LV_EVENT_DRAW_MAIN,NULL);
    } else image=sw_icon(b,glyph,SW_SIZE_L,SW_TONE_TEXT);
    lv_obj_set_style_text_color(image,lv_color_hex(0x605341),0);lv_obj_clear_flag(image,LV_OBJ_FLAG_CLICKABLE);lv_obj_align(image,LV_ALIGN_TOP_MID,0,8);
    static const char *names[]={"Feed","Play","Sleep","Stats","Family","Meet"};
    lv_obj_t *text=lv_label_create(b);lv_label_set_text(text,index==5?"Wake":names[slot]);
    lv_obj_set_style_text_font(text,sw_font(SW_FONT_CAPTION),0);lv_obj_set_style_text_color(text,lv_color_hex(0x605341),0);
    lv_obj_align(text,LV_ALIGN_BOTTOM_MID,0,-7);lv_obj_clear_flag(text,LV_OBJ_FLAG_CLICKABLE);
}
void pet_controls_create(pet_view_t *v)
{
    v->grabber=lv_obj_create(v->screen);lv_obj_remove_style_all(v->grabber);lv_obj_set_pos(v->grabber,96,288);lv_obj_set_size(v->grabber,168,72);
    lv_obj_clear_flag(v->grabber,LV_OBJ_FLAG_SCROLLABLE);sdgoods_tap_bind(v->grabber,&v->grabber_tap,open,v);
    lv_obj_t *pill=lv_obj_create(v->grabber);lv_obj_remove_style_all(pill);lv_obj_set_pos(pill,52,38);lv_obj_set_size(pill,64,24);
    lv_obj_set_style_radius(pill,12,0);lv_obj_set_style_bg_color(pill,lv_color_hex(0xfffcf6),0);lv_obj_set_style_bg_opa(pill,LV_OPA_COVER,0);
    lv_obj_clear_flag(pill,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);lv_obj_t *up=sw_icon(pill,SW_ICON_UP,SW_SIZE_M,SW_TONE_TEXT);lv_obj_set_style_text_color(up,lv_color_hex(0x605341),0);lv_obj_center(up);lv_obj_clear_flag(up,LV_OBJ_FLAG_CLICKABLE);
    v->shade=lv_obj_create(v->screen);lv_obj_remove_style_all(v->shade);lv_obj_set_size(v->shade,360,360);
    lv_obj_set_style_bg_color(v->shade,lv_color_black(),0);lv_obj_set_style_bg_opa(v->shade,LV_OPA_20,0);lv_obj_clear_flag(v->shade,LV_OBJ_FLAG_SCROLLABLE);
    sdgoods_tap_bind(v->shade,&v->shade_tap,outside,v);
    v->drawer=lv_obj_create(v->screen);lv_obj_remove_style_all(v->drawer);lv_obj_set_pos(v->drawer,0,360);lv_obj_set_size(v->drawer,360,220);
    lv_obj_set_style_radius(v->drawer,28,0);lv_obj_set_style_bg_color(v->drawer,lv_color_hex(0xfff8e9),0);lv_obj_set_style_bg_opa(v->drawer,LV_OPA_COVER,0);
    lv_obj_clear_flag(v->drawer,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_GESTURE_BUBBLE);lv_obj_add_flag(v->drawer,LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_t *handle=lv_obj_create(v->drawer);lv_obj_remove_style_all(handle);lv_obj_set_pos(handle,160,10);lv_obj_set_size(handle,40,4);
    lv_obj_set_style_radius(handle,2,0);lv_obj_set_style_bg_color(handle,lv_color_hex(0xc2b99e),0);lv_obj_set_style_bg_opa(handle,LV_OPA_COVER,0);lv_obj_clear_flag(handle,LV_OBJ_FLAG_CLICKABLE);
    icon(v,1,0,SW_ICON_NONE);icon(v,2,1,SW_ICON_GAME);icon(v,3,2,SW_ICON_MOON);icon(v,5,2,SW_ICON_SUN);
    icon(v,4,3,SW_ICON_LIST);icon(v,8,4,SW_ICON_HOME);icon(v,9,5,SW_ICON_HEART);
    pet_controls_dismiss(v);
}
