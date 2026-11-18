/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Elliptical hold target with independent audio-level activity bars. */
#include "sg_internal.h"
#include <math.h>
typedef struct { sg_touch_t mode; uint8_t level; } touch_t;
static void line(lv_draw_ctx_t *ctx,int x,int y,int x2,int y2,int width,lv_color_t color)
{
    lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);d.width=width;d.color=color;d.round_start=d.round_end=1;
    lv_point_t a={x,y},b={x2,y2};lv_draw_line(ctx,&d,&a,&b);
}
/** Sample a cubic curve into rounded segments at display resolution. */
static void curve(lv_draw_ctx_t *ctx,int x,int y,const int p[8],lv_color_t color)
{
    int last_x=x+p[0],last_y=y+p[1];
    for(unsigned i=1;i<=24;i++){
        float t=i/24.0f,u=1-t;
        int nx=x+(int)(u*u*u*p[0]+3*u*u*t*p[2]+3*u*t*t*p[4]+t*t*t*p[6]+.5f);
        int ny=y+(int)(u*u*u*p[1]+3*u*u*t*p[3]+3*u*t*t*p[5]+t*t*t*p[7]+.5f);
        line(ctx,x+(last_x-x)*4/5,y+(last_y-y)*4/5,x+(nx-x)*4/5,y+(ny-y)*4/5,7,color);last_x=nx;last_y=ny;
    }
}
static void fingerprint(lv_draw_ctx_t *ctx,int x,int y,lv_color_t color)
{
    static const int ridges[][8]={
        {10,27,34,-2,72,-2,98,25},
        {5,64,17,8,91,8,104,63},
        {15,79,28,65,22,41,53,40},
        {53,40,78,39,86,62,89,79},
        {29,91,42,74,32,57,53,55},
        {53,55,69,54,77,70,72,91},
        {47,95,55,83,57,77,57,73}
    };
    for(unsigned i=0;i<sizeof(ridges)/sizeof(ridges[0]);i++)curve(ctx,x,y,ridges[i],color);
}
static void arc(lv_draw_ctx_t *ctx,lv_point_t center,int radius,int start,int end,int width,lv_color_t color)
{
    lv_draw_arc_dsc_t d;lv_draw_arc_dsc_init(&d);d.width=width;d.rounded=1;d.color=color;
    lv_draw_arc(ctx,&d,&center,radius,start,end);
}
static void speaker(lv_draw_ctx_t *ctx,int x,int y,lv_color_t color)
{
    lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.radius=5;d.bg_opa=LV_OPA_COVER;d.bg_color=color;
    lv_area_t box={x,y+29,x+24,y+59};lv_draw_rect(ctx,&d,&box);
    for(int i=0;i<30;i++)line(ctx,x+23+i,y+29-i,x+23+i,y+59+i,3,color);
    lv_point_t c={x+45,y+44};arc(ctx,c,32,315,45,8,color);arc(ctx,c,50,310,50,8,color);
}
/** Filled ellipse; the screen clips the lower half beyond the physical display. */
static void ellipse(lv_draw_ctx_t *ctx,int x,int y,int rx,int ry,lv_color_t color)
{
    lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.bg_color=color;
    int top=LV_MAX(y-ry,ctx->clip_area->y1),bottom=LV_MIN(y+ry,ctx->clip_area->y2);
    for(int row=top;row<=bottom;row++){
        float relative=(float)(row-y)/ry;
        float half=rx*sqrtf(LV_MAX(0.f,1.f-relative*relative));
        float left=x-half,right=x+half;
        int start=(int)ceilf(left),end=(int)floorf(right);
        lv_area_t span={start,row,end,row};d.bg_opa=LV_OPA_COVER;
        if(start<=end)lv_draw_rect(ctx,&d,&span);
        if(left<start){
            span=(lv_area_t){start-1,row,start-1,row};d.bg_opa=(lv_opa_t)((start-left)*255);
            lv_draw_rect(ctx,&d,&span);
        }
        if(right>end){
            span=(lv_area_t){end+1,row,end+1,row};d.bg_opa=(lv_opa_t)((right-end)*255);
            lv_draw_rect(ctx,&d,&span);
        }
    }
}
static void draw(lv_event_t *event)
{
    lv_obj_t *obj=lv_event_get_target(event);const touch_t *t=lv_obj_get_user_data(obj);
    lv_draw_ctx_t *ctx=lv_event_get_draw_ctx(event);lv_area_t a;lv_obj_get_coords(obj,&a);
    lv_color_t ink=sg_color(SG_TONE_TEXT),lime=sg_color(SG_TONE_ACCENT),purple=sg_color(SG_TONE_SURFACE);
    ellipse(ctx,a.x1+110,a.y1+175,110,175,ink);
    ellipse(ctx,a.x1+110,a.y1+175,107,172,purple);
    if(t->mode==SG_TOUCH_IDLE||t->mode==SG_TOUCH_HELD)
        ellipse(ctx,a.x1+110,a.y1+175,101,165,t->mode==SG_TOUCH_IDLE?ink:lime);
    if(t->mode==SG_TOUCH_RECEIVING)speaker(ctx,a.x1+60,a.y1+22,ink);
    else if(t->mode==SG_TOUCH_ERROR){
        line(ctx,a.x1+110,a.y1+36,a.x1+110,a.y1+75,9,ink);
        line(ctx,a.x1+110,a.y1+92,a.x1+110,a.y1+93,9,ink);
    }else fingerprint(ctx,a.x1+67,a.y1+26,t->mode==SG_TOUCH_IDLE?lime:ink);
}
static void decorations(lv_event_t *event)
{
    lv_obj_t *obj=lv_event_get_target(event);const touch_t *t=lv_obj_get_user_data(obj);
    if(t->mode!=SG_TOUCH_HELD&&t->mode!=SG_TOUCH_RECEIVING)return;
    lv_draw_ctx_t *ctx=lv_event_get_draw_ctx(event);lv_area_t a;lv_obj_get_coords(obj,&a);
    lv_color_t color=sg_color(t->mode==SG_TOUCH_HELD?SG_TONE_ORANGE:SG_TONE_PURPLE);
    int heights[]={8,16,25,18,10};
    for(int i=0;i<5;i++){
        int h=5+heights[i]*(25+t->level)/125;
        line(ctx,a.x1+96+i*7,a.y1-13-h/2,a.x1+96+i*7,a.y1-13+h/2,5,color);
    }
}
static void refresh_size(lv_event_t *event)
{lv_coord_t *size=lv_event_get_param(event);if(*size<45)*size=45;}
static void hit(lv_event_t *event)
{
    lv_hit_test_info_t *hit=lv_event_get_param(event);lv_area_t a;lv_obj_get_coords(lv_event_get_target(event),&a);
    int64_t x=hit->point->x-(a.x1+110),y=hit->point->y-(a.y1+175);
    hit->res=x*x*172*172+y*y*107*107<=107LL*107*172*172;
}
lv_obj_t *sg_touch_target(lv_obj_t *parent)
{
    lv_obj_t *obj=sg_plain(parent);sg_tappable(obj);lv_obj_add_flag(obj,LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_flag(obj,LV_OBJ_FLAG_ADV_HITTEST);lv_obj_set_size(obj,220,350);
    touch_t *t=sg_state(obj,sizeof(*t));t->mode=SG_TOUCH_WAITING;
    lv_obj_add_event_cb(obj,draw,LV_EVENT_DRAW_MAIN,NULL);lv_obj_add_event_cb(obj,decorations,LV_EVENT_DRAW_POST,NULL);
    lv_obj_add_event_cb(obj,refresh_size,LV_EVENT_REFR_EXT_DRAW_SIZE,NULL);lv_obj_refresh_ext_draw_size(obj);
    lv_obj_add_event_cb(obj,hit,LV_EVENT_HIT_TEST,NULL);return obj;
}
void sg_touch_target_set(lv_obj_t *obj,sg_touch_t mode,uint8_t level)
{
    touch_t *t=lv_obj_get_user_data(obj);if(level>100)level=100;
    if(t->mode==mode&&t->level==level)return;
    bool mode_changed=t->mode!=mode;t->mode=mode;t->level=level;
    if(mode_changed)lv_obj_invalidate(obj);
    else if(mode==SG_TOUCH_HELD||mode==SG_TOUCH_RECEIVING){
        /* Audio-level updates leave the static target and fingerprint untouched. */
        lv_area_t a;lv_obj_get_coords(obj,&a);
        lv_area_t bars={a.x1+92,a.y1-31,a.x1+128,a.y1+3};
        lv_obj_invalidate_area(obj,&bars);
    }
}
