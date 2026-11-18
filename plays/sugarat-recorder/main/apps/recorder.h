/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include "clips.h"

#include <stdbool.h>

typedef enum {
    REC_BOOT,
    REC_UNAVAILABLE,
    REC_IDLE,
    REC_STARTING,
    REC_RECORDING,
    REC_FINISHING,
    REC_SAVING,
    REC_UNSAVED,
    REC_LOADING,
    REC_PLAY_STARTING,
    REC_PLAYING,
    REC_PLAY_STOPPING,
    REC_PAUSED,
    REC_DELETING,
} recorder_state_t;

typedef struct {
    recorder_state_t state;
    const char *message;
    size_t count;
    uint32_t clip_id;
    uint32_t elapsed_ms;
    uint8_t level;
    bool storage_ready;
} recorder_view_t;

/* Initialize once after display and touch. Call the remaining APIs on the LVGL thread. */
void recorder_init(void);
void recorder_poll(void);
recorder_view_t recorder_view(void);
voice_clip_t recorder_clip(size_t index);
void recorder_toggle_capture(void);
void recorder_toggle_playback(voice_clip_t clip);
void recorder_stop_playback(void);
void recorder_delete(uint32_t id);
void recorder_pause(void);
void recorder_discard(void);
