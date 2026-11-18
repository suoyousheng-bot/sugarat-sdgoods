/** LVGL artwork for the game screen. UI calls run on the display thread. */
#pragma once

#include "beat_level.h"
#include "lvgl.h"

typedef struct {
    lv_obj_t *scene;
    lv_obj_t *progress;
    beat_level_t level;
    uint32_t ms;
    bool preview;
    int cue_pad;
} beat_view_t;

/** Add the backdrop, game viewport and progress strip to the supplied screen. */
void beat_view_create(beat_view_t *view, lv_obj_t *screen);

/** Redraw the scene and progress strip from one music position. Pass BEAT_NO_CUE to hide the cue. */
void beat_view_draw(beat_view_t *view, const beat_level_t *level, uint32_t ms, bool preview, int cue_pad);
