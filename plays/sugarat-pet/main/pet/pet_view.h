/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Round-screen view, borrowing the application-owned care, family and nearby state. */
#pragma once
#include "lvgl.h"
#include "pet_social.h"
#include "sdgoods_tap.h"
#define PET_WORLD_SIZE 180
typedef void (*pet_input_fn)(void *context, pet_action_t action);
typedef enum { PET_VIEW_CARE, PET_VIEW_FAMILY, PET_VIEW_NEARBY } pet_view_page_t;
typedef struct { void *view; unsigned index; } pet_button_binding_t;
/** One press keeps its origin and suppresses taps after a drag until the next press. */
typedef struct {
    lv_point_t start, last;
    uint32_t last_tick;
    int displacement, drawer_y, velocity;
    unsigned axis;
    bool active, moved, family, eligible, was_open;
} pet_gesture_t;
typedef struct {
    lv_obj_t *screen, *world, *title, *notice, *message, *level;
    lv_obj_t *buttons[18], *labels[5], *bars[5], *values[5], *stage;
    lv_obj_t *drawer, *shade, *grabber, *dots[7];
    sdgoods_tap_ctx_t taps[18], shade_tap, grabber_tap;
    pet_button_binding_t bindings[18];
    lv_img_dsc_t image;
    pet_model_t *model;
    pet_family_t *family;
    pet_social_t *social;
    uint16_t *pixels;
    uint8_t frame_pixels[96 * 96];
    pet_input_fn input;
    void *context;
    const char *save_notice;
    pet_view_page_t page;
    unsigned selection;
    pet_gesture_t gesture;
    bool starter, controls_open, drawer_visible, controls_animating, controls_dragging, family_animating;
} pet_view_t;
/** Create the LVGL screen with a borrowed 180x180 RGB565 buffer. */
void pet_view_create(pet_view_t *view, pet_model_t *model, uint16_t *pixels, pet_input_fn input, void *context);
/** Attach family and nearby owners before presenting social controls. */
void pet_view_attach(pet_view_t *view, pet_family_t *family, pet_social_t *social);
/** Snap the icon tray open or closed; its background is frozen during movement. */
void pet_view_controls(pet_view_t *view, bool open);
/** Refresh text and the current animation frame; NULL clears a storage notice. */
void pet_view_update(pet_view_t *view, const char *notice);
/** Delete an inactive screen before releasing its borrowed buffer. */
void pet_view_destroy(pet_view_t *view);
