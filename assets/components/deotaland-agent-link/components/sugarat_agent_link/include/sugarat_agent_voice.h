/*
 * Deotaland Agent Link voice chat: one realtime conversation per opened chat, kept connected in the
 * background so the talk button works the moment it is pressed.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Opening a chat creates a new conversation, starts the microphone and speaker stream and
 * connects the realtime WebSocket. A press made before the connection is ready is recorded
 * and sent once it is. A lost connection is replaced with a fresh grant automatically.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SUGARAT_AGENT_LINK_OFF = 0,      /* chat closed */
    SUGARAT_AGENT_LINK_CONNECTING,   /* first connection of this chat */
    SUGARAT_AGENT_LINK_READY,        /* the voice session accepts audio */
    SUGARAT_AGENT_LINK_RECONNECTING, /* replacing a lost connection; speech is kept */
    SUGARAT_AGENT_LINK_FAILED,       /* repeated failures; still retrying slowly */
} sugarat_agent_link_t;

typedef enum {
    SUGARAT_AGENT_TURN_IDLE = 0,
    SUGARAT_AGENT_TURN_LISTENING, /* talk button held */
    SUGARAT_AGENT_TURN_THINKING,  /* turn sent; waiting for the reply */
    SUGARAT_AGENT_TURN_SPEAKING,  /* reply audio playing */
} sugarat_agent_turn_t;

typedef enum {
    SUGARAT_AGENT_VOICE_OK = 0,
    SUGARAT_AGENT_VOICE_ERR_OFFLINE,      /* the device is not signed in to the platform */
    SUGARAT_AGENT_VOICE_ERR_CONVERSATION, /* no conversation could be created */
    SUGARAT_AGENT_VOICE_ERR_GRANT,        /* the platform refused a voice grant */
    SUGARAT_AGENT_VOICE_ERR_CONNECT,      /* the WebSocket did not connect or was lost */
    SUGARAT_AGENT_VOICE_ERR_REJECTED,     /* the voice server ended the session */
    SUGARAT_AGENT_VOICE_ERR_AUDIO,        /* the audio stream could not start */
    SUGARAT_AGENT_VOICE_ERR_TOO_SHORT,    /* the press was released before it became a turn */
    SUGARAT_AGENT_VOICE_ERR_NO_REPLY,     /* no reply arrived for a turn */
} sugarat_agent_voice_error_t;

typedef struct {
    bool open;
    sugarat_agent_link_t link;
    sugarat_agent_turn_t turn;
    sugarat_agent_voice_error_t error;   /* last problem; cleared when the link becomes ready or a reply starts */
    bool reused_conversation; /* the platform refused a new conversation; the newest one is used */
    uint32_t revision;        /* increments on every change */
} sugarat_agent_voice_status_t;

/**
 * Create the voice and microphone-uplink tasks. Call once after sugarat_audio_init() and
 * sugarat_agent_cloud_start(); the queues and task stacks are allocated in PSRAM.
 * @returns ESP_ERR_NO_MEM when a queue, buffer or task cannot be created.
 */
esp_err_t sugarat_agent_voice_init(void);

/** Open a chat with a new conversation. Returns immediately; safe from any task. */
void sugarat_agent_voice_open(void);

/** Close the chat, dropping unsent speech and stopping audio. Returns immediately. */
void sugarat_agent_voice_close(void);

/** Press (true) or release (false) the talk button. A press interrupts reply playback. */
void sugarat_agent_voice_talk(bool down);

/** Copy the current status. Safe from any task. */
void sugarat_agent_voice_status(sugarat_agent_voice_status_t *out);

#ifdef __cplusplus
}
#endif
