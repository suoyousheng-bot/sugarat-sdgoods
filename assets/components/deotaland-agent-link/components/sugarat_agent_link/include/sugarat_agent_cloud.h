/*
 * Deotaland Agent Link platform client: device identity, activation, authentication, heartbeat and the
 * device-authenticated conversation and voice-grant requests.
 * SPDX-License-Identifier: Apache-2.0
 *
 * One worker task owns activation and the online loop. The request functions at the bottom
 * block on HTTPS and must run on a worker task, never on the LVGL thread.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SUGARAT_AGENT_CLOUD_OFFLINE = 0,  /* no network */
    SUGARAT_AGENT_CLOUD_CLAIMING,     /* requesting an activation code */
    SUGARAT_AGENT_CLOUD_CODE,         /* showing an activation code; waiting for the user to claim it */
    SUGARAT_AGENT_CLOUD_ALREADY_BOUND,/* the platform says another account owns this device; unbind it in the console */
    SUGARAT_AGENT_CLOUD_SIGNING_IN,   /* authenticating the saved device key */
    SUGARAT_AGENT_CLOUD_NO_AGENT,     /* claimed, but no Agent is bound to the device yet */
    SUGARAT_AGENT_CLOUD_ONLINE,       /* authenticated; agent fields are valid */
    SUGARAT_AGENT_CLOUD_FAULT,        /* a request failed; retrying with backoff */
    SUGARAT_AGENT_CLOUD_REJECTED,     /* the platform product does not accept this device's chip type */
} sugarat_agent_cloud_phase_t;

typedef struct {
    sugarat_agent_cloud_phase_t phase;
    uint32_t revision;       /* increments on every change */
    char code[8];            /* SUGARAT_AGENT_CLOUD_CODE: six digits */
    int64_t code_expires_us; /* SUGARAT_AGENT_CLOUD_CODE: esp_timer time when the code expires */
    int agent_id;            /* SUGARAT_AGENT_CLOUD_ONLINE */
    char agent_name[80];
    bool voice_supported;    /* the Agent advertises realtime voice */
    uint32_t agent_epoch;    /* increments when the bound Agent changes */
    int error_code;          /* platform business code or HTTP status of the last failure */
} sugarat_agent_cloud_status_t;

/** Realtime voice grant: a one-use WebSocket token and the negotiated audio formats. */
typedef struct {
    char url[512];
    char token[256];
    bool input_opus;
    bool output_opus;
} sugarat_agent_cloud_grant_t;

/** Create the worker. Call once after NVS initialization and before Wi-Fi reports events. */
esp_err_t sugarat_agent_cloud_start(void);

/** Report network availability (Wi-Fi with an IP address). Safe from any task. */
void sugarat_agent_cloud_set_network(bool up);

/** Copy the current status. Safe from any task. */
void sugarat_agent_cloud_status(sugarat_agent_cloud_status_t *out);

/** Forget the device key and request a new activation code. Safe from any task. */
void sugarat_agent_cloud_forget(void);

/** Device serial sent to the platform (a UUIDv4 stored in NVS), or "" before the worker creates it. */
const char *sugarat_agent_cloud_device_sn(void);

/**
 * Create a new realtime-voice conversation for the bound Agent. When the platform refuses to
 * create one, the newest existing conversation is used and *reused is set.
 * @returns ESP_OK; ESP_ERR_INVALID_STATE when not online; ESP_ERR_NOT_FOUND when no conversation
 *          could be created or found; another error for transport failures.
 */
esp_err_t sugarat_agent_cloud_new_conversation(int agent_id, char *id, size_t id_size, bool *reused);

/** Request a voice grant for a conversation; a busy session (HTTP 409) is refreshed instead. */
esp_err_t sugarat_agent_cloud_grant(int agent_id, const char *conversation_id, sugarat_agent_cloud_grant_t *out);

/**
 * Read audio settings (audio_config/audio, possibly nested below data, grant or
 * realtimeConnection) from a parsed cJSON object and update the formats they name.
 * @returns false when a named format is unsupported; true otherwise, also when none is present.
 */
bool sugarat_agent_cloud_parse_audio(const void *json, bool *input_opus, bool *output_opus);

/** Release a voice grant. Failures are logged and ignored. */
void sugarat_agent_cloud_end_grant(int agent_id, const char *conversation_id);

#ifdef __cplusplus
}
#endif
