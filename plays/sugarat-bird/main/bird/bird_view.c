/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Pixel flight drawing uses the Game palette; labels, buttons and framing use its components. */
#include "bird_view.h"
#include "sugarat_game.h"
#include <stdio.h>
#include <string.h>

static void rect(lv_draw_ctx_t *ctx, int x, int y, int w, int h, sg_tone_t color)
{
    if (w <= 0 || h <= 0) return;
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color = sg_color(color); d.bg_opa = LV_OPA_COVER; d.radius = 0;
    lv_area_t a = {.x1=x,.y1=y,.x2=x+w-1,.y2=y+h-1};
    lv_draw_rect(ctx, &d, &a);
}
static void draw(lv_event_t *event)
{
    bird_view_t *v = lv_event_get_user_data(event);
    const bird_model_t *g = v->game;
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(event);
    lv_area_t area; lv_obj_get_content_coords(v->field, &area);
    lv_area_t clip;
    if (!_lv_area_intersect(&clip, ctx->clip_area, &area)) return;
    const lv_area_t *previous = ctx->clip_area; ctx->clip_area = &clip;
    int x = area.x1, y = area.y1;
    rect(ctx,x,y,BIRD_FIELD_W,BIRD_FIELD_H,SG_TONE_SURFACE);
    for (unsigned i=0;i<9;i++) {
        int sx=(i*73 + 13 - (int)(g->elapsed_ms/140)) % BIRD_FIELD_W;
        if (sx<0) sx+=BIRD_FIELD_W;
        rect(ctx,x+sx,y+12+(i*29)%100,2,2,SG_TONE_MUTED);
    }
    for (int i=0;i<6;i++) {
        int bx=i*48-(int)(g->elapsed_ms/90)%48;
        rect(ctx,x+bx,y+147,32,21,SG_TONE_RAISED);
        rect(ctx,x+bx+8,y+139,16,8,SG_TONE_RAISED);
    }
    if (g->phase != BIRD_READY) for(unsigned i=0;i<BIRD_PIPE_COUNT;i++) {
        const bird_pipe_t *p=&g->pipes[i]; int px=x+(int)p->x;
        int top=(int)(p->center-p->gap*.5f), bottom=(int)(p->center+p->gap*.5f);
        rect(ctx,px,y,BIRD_PIPE_W,top,SG_TONE_GREEN);
        rect(ctx,px+4,y,4,top,SG_TONE_TEAL);
        rect(ctx,px-3,y+top-10,BIRD_PIPE_W+6,10,SG_TONE_GREEN);
        rect(ctx,px,y+bottom,BIRD_PIPE_W,BIRD_FIELD_H-bottom,SG_TONE_GREEN);
        rect(ctx,px+4,y+bottom,4,BIRD_FIELD_H-bottom,SG_TONE_TEAL);
        rect(ctx,px-3,y+bottom,BIRD_PIPE_W+6,10,SG_TONE_GREEN);
    }
    static const char *const sprite[]={
        "    YYYY     ","  YYYYYYYY   "," YYYYYWWWYY  ","YYYYYYWKWYY  ","YYOOYYWWWYRR ",
        "YOOOOYYYYRRRR","YYOOYYYYYRRR "," YYYYYWWYY   ","  YWWWWY     ","   YYYY      "};
    int by=y+(g->phase==BIRD_READY?35:(int)g->y), bx=x+BIRD_X-4;
    for(int row=0;row<10;row++) for(int col=0;col<13;col++) {
        char c=sprite[row][col]; if(c==' ')continue;
        sg_tone_t tone=c=='Y'?SG_TONE_YELLOW:c=='W'?SG_TONE_TEXT:c=='K'?SG_TONE_BG:c=='O'?SG_TONE_ORANGE:SG_TONE_RED;
        rect(ctx,bx+col*2,by+row*2,2,2,tone);
    }
    rect(ctx,x,y+BIRD_FIELD_H-3,BIRD_FIELD_W,3,SG_TONE_GREEN);
    ctx->clip_area=previous;
}
static void label(lv_obj_t *object, const char *text)
{
    if(strcmp(lv_label_get_text(object),text)) lv_label_set_text(object,text);
}
void bird_view_create(bird_view_t *v, const bird_model_t *game)
{
    sg_init(); v->game=game; v->screen=sg_screen();
    lv_obj_t *title=sg_label(v->screen,"POCKET BIRD",SG_FONT_BODY,SG_TONE_ACCENT);
    lv_obj_align(title,LV_ALIGN_TOP_MID,0,60);
    v->score=sg_label(v->screen,"0",SG_FONT_CAPTION,SG_TONE_TEXT); lv_obj_set_pos(v->score,68,92);
    v->best=sg_label(v->screen,"Best 0",SG_FONT_CAPTION,SG_TONE_MUTED); lv_obj_align(v->best,LV_ALIGN_TOP_RIGHT,-68,92);
    v->field=lv_obj_create(v->screen); lv_obj_remove_style_all(v->field);
    lv_obj_set_pos(v->field,60,120);lv_obj_set_size(v->field,BIRD_FIELD_W,BIRD_FIELD_H);
    lv_obj_clear_flag(v->field,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(v->field,draw,LV_EVENT_DRAW_MAIN,v);
    v->panel=lv_obj_create(v->screen);lv_obj_remove_style_all(v->panel);
    lv_obj_set_pos(v->panel,72,132);lv_obj_set_size(v->panel,216,152);
    lv_obj_clear_flag(v->panel,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    sg_box(v->panel,SG_BOX_FRAME,SG_TONE_SURFACE,true);
    v->message=sg_label(v->screen,"Ready?",SG_FONT_TITLE,SG_TONE_TEXT);lv_obj_align(v->message,LV_ALIGN_TOP_MID,24,142);
    v->detail=sg_label(v->screen,"Tap to flap",SG_FONT_CAPTION,SG_TONE_TEXT);lv_obj_align(v->detail,LV_ALIGN_TOP_MID,24,195);
    v->button=sg_button(v->screen,SG_ICON_NONE,NULL,SG_BUTTON_FILLED,SG_TONE_ACCENT);
    lv_obj_set_size(v->button,144,42);lv_obj_align(v->button,LV_ALIGN_TOP_MID,0,232);
    v->button_label=sg_label(v->button,"Play",SG_FONT_BODY,SG_TONE_BG);lv_obj_center(v->button_label);
    v->notice=sg_label(v->screen,"",SG_FONT_CAPTION,SG_TONE_RED);lv_obj_align(v->notice,LV_ALIGN_TOP_MID,0,277);
    bird_view_update(v,false);
}
void bird_view_update(bird_view_t *v, bool failed)
{
    const bird_model_t *g=v->game;char text[32];
    snprintf(text,sizeof(text),"%lu",(unsigned long)g->score);label(v->score,text);
    snprintf(text,sizeof(text),"Best %lu",(unsigned long)g->best);label(v->best,text);lv_obj_align(v->best,LV_ALIGN_TOP_RIGHT,-68,92);
    bool overlay=g->paused||g->phase!=BIRD_PLAYING;
    lv_obj_t *objects[]={v->message,v->detail,v->button};
    for(unsigned i=0;i<3;i++) {
        if(overlay)lv_obj_clear_flag(objects[i],LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(objects[i],LV_OBJ_FLAG_HIDDEN);
    }
    if(g->paused||g->phase==BIRD_OVER)lv_obj_clear_flag(v->panel,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(v->panel,LV_OBJ_FLAG_HIDDEN);
    if(overlay) {
        label(v->message,g->paused?"Paused":g->phase==BIRD_OVER?"Game Over":"Ready?");
        label(v->button_label,g->paused?"Resume":g->phase==BIRD_OVER?"Try again":"Play");
        if(g->phase==BIRD_OVER&&!g->paused){snprintf(text,sizeof(text),"Score %lu",(unsigned long)g->score);label(v->detail,text);}
        else label(v->detail,g->paused?"Tap Resume":"Tap to flap");
        lv_obj_align(v->message,LV_ALIGN_TOP_MID,g->phase==BIRD_READY&&!g->paused?24:0,142);
        lv_obj_align(v->detail,LV_ALIGN_TOP_MID,g->phase==BIRD_READY&&!g->paused?24:0,195);
    }
    label(v->notice,failed?"Best not saved":"");
    lv_obj_invalidate(v->field);
}
