/* SPDX-License-Identifier: Apache-2.0 */
/** Watch-component time application; all calls belong to the LVGL thread. */
#ifndef SUGARAT_TIME_APP_H
#define SUGARAT_TIME_APP_H
#include "lvgl.h"
#include "time_copy.h"
#include <stdint.h>

typedef enum { TIME_TOOL_FOCUS, TIME_TOOL_COUNTDOWN, TIME_TOOL_STOPWATCH, TIME_TOOL_CLOCK } time_tool_t;
typedef struct {
    uint64_t (*now_ms)(void);
    time_locale_t locale;
    time_tool_t tool;
    bool gallery_navigation;
} time_app_config_t;

/**
 * Create an owned screen with four independent, RAM-only time tools.
 * @param config Initial tool, language, monotonic millisecond provider and explicit gallery navigation.
 * The provider must outlive the screen and include time while the display is hidden.
 * Standalone cases use gallery_navigation=false; tapping the title switches languages.
 * @returns A screen to load with lv_scr_load(); deletion releases its update timer and state.
 */
lv_obj_t *time_app_open(time_app_config_t config);
#endif
