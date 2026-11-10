/* SPDX-License-Identifier: Apache-2.0 */
/** Music-player, album and transport-control scenes built from Sugarat Watch controls. */
#ifndef SUGARAT_MEDIA_H
#define SUGARAT_MEDIA_H
#include "lvgl.h"
#include "media_transport.h"
typedef enum { MEDIA_LOCALE_ZH, MEDIA_LOCALE_EN } media_locale_t;
typedef enum { MEDIA_SCENE_PLAYER, MEDIA_SCENE_COVER, MEDIA_SCENE_CONTROLS } media_scene_t;
typedef struct {
    bool (*request)(void *context, media_request_t request);
    bool (*volume)(void *context, uint8_t percent);
    media_snapshot_t (*read)(void *context);
    void *context;
} media_backend_t;
typedef struct {
    media_backend_t backend;
    const uint32_t *durations_ms;
    uint32_t track_count;
    uint8_t unmute_volume;
    media_locale_t locale;
    media_scene_t scene;
    bool gallery_navigation, silent_preview;
} media_config_t;
/**
 * Create a screen with explicit backend, language and scene, on the LVGL thread.
 * @param config Three static tracks, durations and a backend that outlives the screen.
 * Read/volume/request callbacks run on the LVGL thread and must not block on hardware.
 * Providers own audio buffers, queues and shutdown; accepted requests are pending until reported.
 * @returns A screen whose deletion requests stop and releases UI resources, never provider buffers.
 */
lv_obj_t *media_open(media_config_t config);
/** Stop playback when covered or hidden; revealing does not automatically resume it.
 * @param screen A live screen returned by media_open().
 * @param paused True while covered or hidden.
 */
void media_set_paused(lv_obj_t *screen, bool paused);
#endif
