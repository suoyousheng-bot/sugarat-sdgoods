/* SPDX-License-Identifier: Apache-2.0 */
/** Local companion scenes using Sugarat Watch and Pixel; all calls belong to the LVGL thread. */
#ifndef SUGARAT_COMPANION_H
#define SUGARAT_COMPANION_H
#include "lvgl.h"
#include "companion_copy.h"
typedef enum { SC_SCENE_PET, SC_SCENE_MOODS, SC_SCENE_VOICE } sc_scene_t;
typedef enum { SC_VOICE_DEMO, SC_VOICE_EXTERNAL } sc_voice_mode_t;
typedef struct {
    uint64_t (*now_ms)(void);
    sc_locale_t locale;
    sc_scene_t scene;
    sp_char_id_t character;
    sc_timing_t timing;
    bool gallery_navigation;
    sc_voice_mode_t voice_mode;
    void (*on_voice)(bool pressed, void *user_data);
    void *user_data;
} sc_config_t;

/**
 * Create a screen whose deletion releases its timer, avatar and state.
 * @param config Explicit scene, language, character, monotonic clock, timings and voice mode.
 * External mode requires on_voice; it runs synchronously on press/release or cancellation.
 * Callbacks must not delete the screen. Providers own audio/network work and post states on the LVGL thread.
 * @returns A screen to load with lv_scr_load(); no recording, connection or sound is started by this component.
 */
lv_obj_t *sc_open(sc_config_t config);
/**
 * Publish an actual provider state in external mode; cancel pending demo transitions.
 * @param screen A live screen returned by sc_open().
 * @param state Provider state; offline remains visible until the provider reports recovery.
 */
void sc_set_voice_state(lv_obj_t *screen, sc_voice_t state);
/**
 * Suspend animation and cancel an active voice request while the app is covered or hidden.
 * @param screen A live screen returned by sc_open().
 * @param paused True while hidden; false resumes local animation without restarting recording.
 */
void sc_set_paused(lv_obj_t *screen, bool paused);
#endif
