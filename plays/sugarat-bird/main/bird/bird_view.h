/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Game-theme presentation, 240x168 flight field and English controls in the round safe area. */
#pragma once
#include "bird_model.h"
#include "lvgl.h"
typedef struct {
    lv_obj_t *screen, *field, *score, *best, *panel, *message, *detail, *button, *button_label, *notice;
    const bird_model_t *game;
} bird_view_t;
/** Create one screen; the view and game must outlive it. All calls belong to the LVGL thread.
 * @param view Zero-initialized view storage.
 * @param game Stable model address read by the draw callback.
 */
void bird_view_create(bird_view_t *view, const bird_model_t *game);
/** Update English labels, overlays and flight pixels.
 * @param view Created view.
 * @param save_failed Whether best-score persistence is unavailable or unconfirmed.
 */
void bird_view_update(bird_view_t *view, bool save_failed);
