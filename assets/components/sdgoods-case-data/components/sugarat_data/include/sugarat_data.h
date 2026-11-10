/* SPDX-License-Identifier: Apache-2.0 */
/** Four round-screen data scenes. Call every function on the LVGL thread. */
#ifndef SUGARAT_DATA_H
#define SUGARAT_DATA_H
#include "lvgl.h"
#include "data_model.h"
typedef enum { SD_LOCALE_ZH, SD_LOCALE_EN } sd_locale_t;
typedef enum { SD_SCENE_GAUGE, SD_SCENE_OVERVIEW, SD_SCENE_HISTORY, SD_SCENE_ALARM } sd_scene_t;
typedef enum { SD_SOURCE_DEMO, SD_SOURCE_EXTERNAL } sd_source_t;
typedef struct {
    uint64_t (*now_ms)(void);
    sd_locale_t locale;
    sd_scene_t scene;
    sd_source_t source;
    sd_limits_t limits;
    uint32_t refresh_ms;
    bool gallery_navigation;
} sd_config_t;
/**
 * Create an empty telemetry screen without starting a sensor or a demonstration.
 * @param config Explicit clock, locale, scene, source label, limits, positive refresh interval and navigation choice.
 * The caller owns sampling; SD_SOURCE_DEMO only adds a persistent demo label.
 * @returns A screen to load; NULL for invalid configuration or state allocation failure.
 * Load another screen before deletion. Deletion releases the component's timer and state.
 */
lv_obj_t *sd_open(sd_config_t config);
/**
 * Copy a complete reading into a live screen, including while it is hidden.
 * @param screen Screen returned by sd_open().
 * @param sample Decoded provider fields, validated before committing; failures show invalid data.
 * @returns Whether the reading was accepted; no sensor, storage or network work is started.
 */
bool sd_publish(lv_obj_t *screen, sd_sample_t sample);
/**
 * Show provider loss; retained alarms remain visible until valid recovery.
 * @param screen Live screen returned by sd_open().
 */
void sd_disconnect(lv_obj_t *screen);
/**
 * Suspend refresh and ignore touch while covered; freshness is re-evaluated on resume.
 * @param screen Live screen returned by sd_open().
 * @param paused Whether the product shell has hidden or covered the display.
 */
void sd_set_paused(lv_obj_t *screen, bool paused);
#endif
