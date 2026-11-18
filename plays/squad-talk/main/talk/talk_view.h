/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** English round-screen view; functions and object lifetimes belong to the LVGL thread. */
#pragma once
#include "lvgl.h"
#include "talk_radio.h"
typedef struct {
    talk_status_t rendered;
    bool rendered_valid;
    lv_obj_t *screen,*ptt,*ptt_text,*channel,*status,*online,*buddy,*linkdot;
} talk_view_t;
/** Build a swipe-channel display and a bottom half-ellipse target; labels fit the physical round display. */
void talk_view_create(talk_view_t *view);
/** Render actual radio status and measured audio activity. */
void talk_view_update(talk_view_t *view,const talk_status_t *status);
