/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** ESP-IDF transport/audio owner. UI calls are nonblocking and never touch worker-owned sessions. */
#pragma once
#include "talk_core.h"
#include "esp_err.h"
typedef enum { TALK_FAULT_NONE, TALK_FAULT_AUDIO, TALK_FAULT_RADIO, TALK_FAULT_MEMORY } talk_fault_t;
typedef struct {
    talk_phase_t phase;
    talk_fault_t fault;
    uint8_t channel, level;
    bool online, link_failed;
    uint32_t received, lost, queue_drops;
} talk_status_t;
/** Start the single radio worker after display/touch initialization; failures remain visible in status. */
esp_err_t talk_radio_start(void);
/** Copy a synchronized status for the LVGL thread. */
void talk_radio_status(talk_status_t *status);
/** Set hold, logical channel and participation. A release cannot be lost to a full network queue. */
void talk_radio_input(bool held, uint8_t channel, bool paused);
/** Stop participation and request worker-owned radio/audio teardown; safe before initialization. */
void talk_radio_stop(void);
