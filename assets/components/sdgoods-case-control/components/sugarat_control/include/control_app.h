/* SPDX-License-Identifier: Apache-2.0 */
/** Local device-control cases. Call every function on the LVGL thread; no commands leave the application. */
#ifndef SUGARAT_CONTROL_APP_H
#define SUGARAT_CONTROL_APP_H
#include "lvgl.h"
#include <stdbool.h>

typedef enum { CONTROL_LOCALE_ZH, CONTROL_LOCALE_EN } control_locale_t;
typedef enum { CONTROL_SCENE_SWITCHES, CONTROL_SCENE_LEVELS, CONTROL_SCENE_SCENES, CONTROL_SCENE_REMOTE } control_scene_t;
typedef struct {
    control_locale_t locale;
    control_scene_t scene;
    bool gallery_navigation;
} control_app_config_t;

/** Create an independent local simulation without starting a device transport.
 * @param config Explicit language, initial scene and navigation choice; false omits gallery arrows.
 * @returns A screen owning its RAM-only values and timer, or NULL for invalid configuration/allocation failure.
 * Load another screen before deletion. Deletion releases the screen-owned timer and state.
 */
lv_obj_t *control_app_open(control_app_config_t config);

/** Suspend refresh and ignore touch while hidden, preserving simulated values.
 * @param screen Live screen returned by control_app_open().
 * @param paused Whether the product shell has hidden or covered the application.
 */
void control_app_set_paused(lv_obj_t *screen, bool paused);
#endif
