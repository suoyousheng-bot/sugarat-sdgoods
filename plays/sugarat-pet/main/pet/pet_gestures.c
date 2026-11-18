/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "pet_gestures.h"
#include "pet_controls.h"

static unsigned member_count(const pet_view_t *v)
{
    return v->starter?3:v->family?v->family->count+(unsigned)v->family->has_egg:0;
}
static void member_x(void *object,int32_t x) {lv_obj_set_x(object,(lv_coord_t)x);}
static void member_ready(lv_anim_t *animation)
{
    pet_view_t *v=lv_anim_get_user_data(animation);v->family_animating=false;pet_view_update(v,v->save_notice);
}
static void member_snap(pet_view_t *v,int direction)
{
    unsigned total=member_count(v),old=v->selection;
    if(direction>0&&old+1<total)v->selection++;
    if(direction<0&&old>0)v->selection--;
    int x=lv_obj_get_x(v->world);
    if(v->selection!=old) {x=direction*72;pet_view_update(v,v->save_notice);}
    lv_obj_set_x(v->world,x);
    if(!x)return;
    v->family_animating=true;
    lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,v->world);lv_anim_set_exec_cb(&a,member_x);
    lv_anim_set_values(&a,x,0);lv_anim_set_time(&a,170);lv_anim_set_path_cb(&a,lv_anim_path_ease_out);
    lv_anim_set_user_data(&a,v);lv_anim_set_ready_cb(&a,member_ready);lv_anim_start(&a);
}
static bool home(const pet_view_t *v)
{
    return !v->model->paused&&!v->starter&&v->page==PET_VIEW_CARE&&v->model->page!=PET_STATS;
}
static void gesture(lv_event_t *event)
{
    pet_view_t *v=lv_event_get_user_data(event);pet_gesture_t *g=&v->gesture;
    lv_event_code_t code=lv_event_get_code(event);
    if(code!=LV_EVENT_PRESSED&&code!=LV_EVENT_PRESSING&&code!=LV_EVENT_RELEASED&&code!=LV_EVENT_PRESS_LOST)return;
    lv_indev_t *input=lv_indev_get_act();if(!input)return;
    lv_point_t p;lv_indev_get_point(input,&p);uint32_t now=lv_tick_get();
    if(code==LV_EVENT_PRESSED) {
        *g=(pet_gesture_t){0};
        if(v->model->paused||p.y<50)return;
        g->family=v->page==PET_VIEW_FAMILY;
        if(g->family) {
            if(lv_event_get_target(event)!=v->screen||p.y<128||p.y>299)return;
            lv_anim_del(v->world,member_x);v->family_animating=false;lv_obj_set_x(v->world,0);
        } else if(!home(v))return;
        g->active=true;g->start=g->last=p;g->last_tick=now;g->was_open=v->controls_open;
        g->drawer_y=v->drawer_visible?lv_obj_get_y(v->drawer):360;
        g->eligible=g->family||v->drawer_visible||p.y>=232;
        return;
    }
    if(!g->active)return;
    int dx=p.x-g->start.x,dy=p.y-g->start.y;
    g->displacement=LV_MAX(g->displacement,LV_MAX(LV_ABS(dx),LV_ABS(dy)));
    if(code==LV_EVENT_PRESSING) {
        if(p.y!=g->last.y) {g->velocity=(p.y-g->last.y)*1000/(int)LV_MAX(1,now-g->last_tick);g->last=p;g->last_tick=now;}
        if(!g->axis&&g->displacement>=10)g->axis=LV_ABS(dx)*4>LV_ABS(dy)*5?1:2;
        if(g->displacement<10)return;
        g->moved=true;
        if(g->family&&g->axis==1) {
            unsigned total=member_count(v);int offset=dx;
            if((v->selection==0&&dx>0)||(v->selection+1>=total&&dx<0))offset/=4;
            lv_obj_set_x(v->world,LV_CLAMP(-110,offset,110));
        } else if(!g->family&&g->eligible&&g->axis==2)pet_controls_drag(v,g->drawer_y+dy);
        return;
    }
    g->active=false;
    bool cancel=code==LV_EVENT_PRESS_LOST;
    if(g->family) {
        int direction=!cancel&&g->axis==1&&LV_ABS(dx)>=38?(dx<0?1:-1):0;
        member_snap(v,direction);
    } else if(v->controls_dragging) {
        int y=lv_obj_get_y(v->drawer),velocity=now-g->last_tick>100?0:g->velocity;
        bool open=cancel?g->was_open:g->was_open?!(y>186||velocity>200):(y<324||(velocity< -200&&g->displacement>=14));
        pet_view_controls(v,open);
    }
}
void pet_gestures_create(pet_view_t *v)
{
    lv_obj_add_event_cb(v->screen,gesture,LV_EVENT_ALL,v);
    for(unsigned i=0;i<18;i++)lv_obj_add_event_cb(v->buttons[i],gesture,LV_EVENT_ALL,v);
    lv_obj_add_event_cb(v->drawer,gesture,LV_EVENT_ALL,v);lv_obj_add_event_cb(v->shade,gesture,LV_EVENT_ALL,v);
    lv_obj_add_event_cb(v->grabber,gesture,LV_EVENT_ALL,v);
}
void pet_gestures_dispose(pet_view_t *v)
{
    lv_anim_del(v->world,member_x);lv_obj_set_x(v->world,0);v->family_animating=false;v->gesture.active=false;
}
