/*
 * Deotaland Agent Link microphone uplink: push-to-talk turns, pre-roll, Opus encoding and an outbox that
 * holds speech until the voice connection can send it.
 * SPDX-License-Identifier: Apache-2.0
 *
 * The microphone runs continuously while a chat is open, so pressing the talk button never
 * waits for capture to start. The last 300 ms before the press are part of the turn. A press
 * becomes a turn after it has been held for AGENT_UPLINK_MIN_HOLD_MS; shorter taps send nothing.
 * Encoded speech waits in the outbox while the connection is (re)established.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AGENT_UPLINK_MIN_HOLD_MS 250

/** Sends one complete WebSocket text message; returns false when it was not sent. */
typedef bool (*agent_uplink_send_fn)(const char *json, size_t len);

/** Create the queues and the encoder task. Called once by sugarat_agent_voice_init(). */
esp_err_t agent_uplink_init(void);

/** Microphone callback for sugarat_audio streaming (16 kHz mono). Never blocks. */
void agent_uplink_on_mic(const int16_t *samples, size_t count, void *user_data);

/**
 * Select the uplink for a new connection. With client endpointing, turns are framed by
 * input_audio.activity start/end; with server endpointing, audio is continuous and silence is
 * sent outside turns. Clears the record of a turn started on the previous connection.
 */
void agent_uplink_configure(bool input_opus, bool client_endpointing);

/** Install the sender once the voice session is ready; NULL while disconnected (speech is kept). */
void agent_uplink_set_sender(agent_uplink_send_fn send);

/**
 * Press or release the talk button. Safe from any task; returns immediately.
 * @returns On release: true when the press became a turn and its end is queued; false for a
 *          tap shorter than AGENT_UPLINK_MIN_HOLD_MS. On press: true.
 */
bool agent_uplink_talk(bool down);

/** Drop queued speech and any turn in progress (the chat closed). */
void agent_uplink_discard(void);

/** Number of messages waiting in the outbox. */
size_t agent_uplink_pending(void);

#ifdef __cplusplus
}
#endif
