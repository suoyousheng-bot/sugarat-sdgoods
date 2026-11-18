/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** One LVGL screen owns presentation; adapters supply device or host services. */
#pragma once
#include "cv_backend.h"
#include "cv_gesture.h"
#include "lvgl.h"
typedef enum { V_COVER,V_ACTIONS,V_PHONE,V_SHARE,V_PEERS,V_WAIT,V_PAIR,V_PROGRESS,V_DONE,V_ERROR } cv_page_t;
typedef struct {
    cv_backend_t backend;cv_status_t status;cv_page_t page;cv_error_t error;
    lv_obj_t *screen,*content,*images[2],*pressed,*ring,*percent;
    lv_obj_t *menu_title,*menu_locale,*menu_count,*menu_hint,*menu_phone,*menu_share;
    lv_indev_t *pointer;
    lv_img_dsc_t descriptors[3],qr;uint16_t *pixels[3],*qr_pixels;
    cv_gesture_t gesture;lv_area_t press_area;char loaded[CV_ID_LENGTH+1],neighbors[2][CV_ID_LENGTH+1];
    uint64_t now,menu_at;int candidate,drag_direction;bool dirty,paused,animating,drag_ready,committed,closing,background_press;
} cv_view_t;
/** Allocate three reusable RGB565 buffers: the shown cover, then the previous and next photos preloaded for swipes. */
bool cv_view_open(cv_view_t *view,cv_backend_t backend);
/** Refresh on the LVGL owner thread; no network callback may call this function. */
void cv_view_tick(cv_view_t *view,uint64_t now_ms);
/** Revoke pending gestures and hide controls while the platform overlay or sleep owns input. */
void cv_view_pause(cv_view_t *view,bool paused);
/** Release animations and the screen before releasing its pixel memory. */
void cv_view_close(cv_view_t *view);
