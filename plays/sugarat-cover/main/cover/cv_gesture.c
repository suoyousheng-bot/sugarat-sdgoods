/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "cv_gesture.h"
#include "sdgoods_tap.h"
#include <stdlib.h>
void cv_gesture_press(cv_gesture_t *g,int x,int y,int action)
{*g=(cv_gesture_t){.x=x,.y=y,.action=action,.held=true};}
void cv_gesture_move(cv_gesture_t *g,int x,int y)
{int dx=abs(x-g->x),dy=abs(y-g->y);if(dx>g->max_dx)g->max_dx=dx;if(dy>g->max_dy)g->max_dy=dy;}
bool cv_gesture_tap(const cv_gesture_t *g,int action,bool inside)
{return g->held&&!g->lost&&inside&&g->action==action&&g->max_dx<=SDGOODS_TAP_SLOP&&g->max_dy<=SDGOODS_TAP_SLOP;}
int cv_gesture_swipe(const cv_gesture_t *g,int x,int y)
{
    int dx=x-g->x,dy=y-g->y;
    if(!g->held||g->lost||g->action>=0||g->y<60||g->y>300||abs(dx)<40||2*abs(dx)<3*abs(dy))return 0;
    return dx<0?-1:1;
}
bool cv_gesture_back(const cv_gesture_t *g,int x,int y)
{int dx=x-g->x,dy=abs(y-g->y);return g->held&&!g->lost&&g->y>=60&&g->y<=300&&dx>=60&&2*dx>=3*dy;}
