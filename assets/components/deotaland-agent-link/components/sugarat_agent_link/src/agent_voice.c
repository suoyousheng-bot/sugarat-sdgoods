/*
 * Deotaland Agent Link voice chat: conversation, realtime WebSocket, reply playback and reconnection.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Tasks:
 *   agent_voice control: commands, HTTPS requests, WebSocket lifetime, timers.
 *   agent_rx    parses server messages, decodes reply audio and feeds the speaker stream.
 *   websocket   the client library's task; its handler only assembles messages and posts them.
 * Every connection has an epoch. Events and messages from an older epoch are dropped.
 */
#include "sugarat_agent_voice.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "decoder/impl/esp_opus_dec.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"

#include "sugarat_agent_cloud.h"
#include "agent_protocol.h"
#include "agent_uplink.h"
#include "sugarat_audio.h"

static const char *TAG = "agent_voice";

#define PSRAM_CAPS          (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define READY_TIMEOUT_US    (10 * 1000000LL)
#define THINK_TIMEOUT_US    (20 * 1000000LL)
#define SPEAK_IDLE_US       (4 * 1000000LL)   /* reply audio stopped arriving and playback drained */
#define SLOW_RETRY_US       (15 * 1000000LL)
#define FAST_RETRIES        4                 /* 1, 2, 4, 8 s, then SLOW_RETRY_US */
#define MESSAGE_CAP         (16 * 1024)
#define PLAYBACK_MS         10000
#define RX_QUEUE_ITEMS      128
#define PCM_OUT_CAP         (16 * 120)        /* 120 ms at 16 kHz */
/* The board's amplifier and speaker are driven at about a quarter of full scale (the BSP's own
 * sounds peak near 8000); full-scale speech overdrives them and crackles. */
#define OUT_PEAK            9000.0f
#define OUT_RELEASE         0.99985f          /* per sample: the gain recovers over about 0.4 s */

typedef enum {
    CMD_OPEN = 0,
    CMD_CLOSE,
    CMD_WS_CONNECTED,
    CMD_WS_LOST,
    CMD_READY,
} cmd_type_t;

typedef struct {
    uint8_t type;
    uint8_t terminal; /* CMD_WS_LOST: the server ended the session */
    uint32_t epoch;
} cmd_t;

typedef struct {
    uint32_t epoch;
    uint8_t binary;
    uint32_t len;
    char *data; /* PSRAM, NUL-terminated; freed by the rx task */
} rx_item_t;

static QueueHandle_t s_cmds;
static QueueHandle_t s_rx;
static SemaphoreHandle_t s_ws_lock;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

/* Shared status, guarded by s_mux. */
static sugarat_agent_voice_status_t s_status;
static int64_t s_turn_since_us;   /* when THINKING or SPEAKING began, or the last reply audio */
static bool s_await_new;          /* drop reply audio until the next response.started */
static bool s_reply_done;         /* response.audio.done arrived for the playing reply */
static uint32_t s_replies_on_link;

/* WebSocket handle, guarded by s_ws_lock; s_epoch is written by the control task only. */
static esp_websocket_client_handle_t s_ws;
static bool s_ws_ready;
static volatile uint32_t s_epoch;

/* Control task state. */
static bool s_want_open;
static bool s_audio_on;
static bool s_granted;
static bool s_client_mode = true; /* push-to-talk endpointing; false after the server refused it */
static bool s_link_was_ready;
static int s_agent_id;
static uint32_t s_agent_epoch;
static char s_conversation[96];
static sugarat_agent_cloud_grant_t s_grant;
static int64_t s_ready_deadline_us;
static int64_t s_retry_at_us;
static unsigned s_failures;
static bool s_hello_sent;
static unsigned s_preready_losses; /* connections lost after session.connect but before ready */

/* rx task state. */
static bool s_output_opus = true;
static void *s_decoder;
static int16_t *s_pcm_out;
static int16_t s_rs_prev;
static int16_t s_lp1, s_lp2; /* low-pass history for the 24 kHz input */
static float s_peak_env;
static float s_rs_pos;

/* Message assembly, websocket task only. */
static char *s_msg;
static size_t s_msg_len;
static bool s_msg_binary;
static bool s_msg_overflow;

/* ---- status ------------------------------------------------------------------------------------ */

static void status_set_link(sugarat_agent_link_t link, sugarat_agent_voice_error_t error)
{
    portENTER_CRITICAL(&s_mux);
    s_status.link = link;
    if (error != SUGARAT_AGENT_VOICE_OK || link == SUGARAT_AGENT_LINK_READY) {
        s_status.error = error;
    }
    s_status.revision++;
    portEXIT_CRITICAL(&s_mux);
}

static void status_set_turn(sugarat_agent_turn_t turn, sugarat_agent_voice_error_t error)
{
    portENTER_CRITICAL(&s_mux);
    s_status.turn = turn;
    s_turn_since_us = esp_timer_get_time();
    if (error != SUGARAT_AGENT_VOICE_OK) {
        s_status.error = error;
    }
    s_status.revision++;
    portEXIT_CRITICAL(&s_mux);
}

void sugarat_agent_voice_status(sugarat_agent_voice_status_t *out)
{
    portENTER_CRITICAL(&s_mux);
    *out = s_status;
    portEXIT_CRITICAL(&s_mux);
}

static void post(cmd_type_t type, uint32_t epoch, bool terminal)
{
    const cmd_t cmd = { .type = (uint8_t)type, .terminal = terminal, .epoch = epoch };
    if (xQueueSend(s_cmds, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "command queue full; dropped command %d", (int)type);
    }
}

/* ---- public ------------------------------------------------------------------------------------ */

void sugarat_agent_voice_open(void)
{
    /* Marked open now, so a press right after opening is recorded while the chat connects. */
    portENTER_CRITICAL(&s_mux);
    s_status.open = true;
    s_status.link = SUGARAT_AGENT_LINK_CONNECTING;
    s_status.error = SUGARAT_AGENT_VOICE_OK;
    s_status.reused_conversation = false;
    s_status.revision++;
    portEXIT_CRITICAL(&s_mux);
    post(CMD_OPEN, 0, false);
}

void sugarat_agent_voice_close(void)
{
    portENTER_CRITICAL(&s_mux);
    s_status.open = false;
    s_status.link = SUGARAT_AGENT_LINK_OFF;
    s_status.turn = SUGARAT_AGENT_TURN_IDLE;
    s_status.revision++;
    portEXIT_CRITICAL(&s_mux);
    (void)agent_uplink_talk(false);
    post(CMD_CLOSE, 0, false);
}

void sugarat_agent_voice_talk(bool down)
{
    portENTER_CRITICAL(&s_mux);
    const bool open = s_status.open;
    const sugarat_agent_turn_t turn = s_status.turn;
    portEXIT_CRITICAL(&s_mux);
    if (!open) {
        return;
    }
    if (down) {
        if (turn == SUGARAT_AGENT_TURN_LISTENING) {
            return;
        }
        /* Barge in: silence the reply now and ignore its remaining audio. */
        portENTER_CRITICAL(&s_mux);
        s_await_new = true;
        s_reply_done = false;
        portEXIT_CRITICAL(&s_mux);
        sugarat_audio_stream_flush();
        (void)agent_uplink_talk(true);
        status_set_turn(SUGARAT_AGENT_TURN_LISTENING, SUGARAT_AGENT_VOICE_OK);
        return;
    }
    if (turn != SUGARAT_AGENT_TURN_LISTENING) {
        return;
    }
    if (agent_uplink_talk(false)) {
        status_set_turn(SUGARAT_AGENT_TURN_THINKING, SUGARAT_AGENT_VOICE_OK);
    } else {
        status_set_turn(SUGARAT_AGENT_TURN_IDLE, SUGARAT_AGENT_VOICE_ERR_TOO_SHORT);
    }
}

/* ---- WebSocket --------------------------------------------------------------------------------- */

static bool ws_send(const char *json, size_t len)
{
    bool sent = false;
    if (xSemaphoreTake(s_ws_lock, pdMS_TO_TICKS(200)) != pdTRUE) {
        return false;
    }
    if (s_ws && s_ws_ready && esp_websocket_client_is_connected(s_ws)) {
        sent = esp_websocket_client_send_text(s_ws, json, (int)len, pdMS_TO_TICKS(1000)) >= 0;
    }
    xSemaphoreGive(s_ws_lock);
    return sent;
}

static void post_message(uint32_t epoch)
{
    rx_item_t item = { .epoch = epoch, .binary = s_msg_binary, .len = (uint32_t)s_msg_len };
    item.data = heap_caps_malloc(s_msg_len + 1, PSRAM_CAPS);
    if (!item.data) {
        ESP_LOGW(TAG, "no memory for a %u-byte message", (unsigned)s_msg_len);
        return;
    }
    memcpy(item.data, s_msg, s_msg_len);
    item.data[s_msg_len] = 0;
    if (xQueueSend(s_rx, &item, pdMS_TO_TICKS(500)) != pdTRUE) {
        ESP_LOGW(TAG, "reply queue full; dropped a message");
        heap_caps_free(item.data);
    }
}

static void on_data(const esp_websocket_event_data_t *d, uint32_t epoch)
{
    switch (d->op_code) {
    case 0x1:
    case 0x2:
        if (d->payload_offset == 0) {
            s_msg_len = 0;
            s_msg_overflow = false;
            s_msg_binary = d->op_code == 0x2;
        }
        break;
    case 0x0: /* continuation of the current message */
        break;
    case 0x8:
        post(CMD_WS_LOST, epoch, false);
        return;
    default: /* ping and pong are answered by the library */
        return;
    }
    if (d->data_len > 0) {
        if (s_msg_len + (size_t)d->data_len > MESSAGE_CAP) {
            s_msg_overflow = true;
        } else {
            memcpy(s_msg + s_msg_len, d->data_ptr, (size_t)d->data_len);
            s_msg_len += (size_t)d->data_len;
        }
    }
    const bool frame_done = d->payload_offset + d->data_len >= d->payload_len;
    if (frame_done && d->fin) {
        if (s_msg_overflow) {
            ESP_LOGW(TAG, "dropped a message larger than %d bytes", MESSAGE_CAP);
        } else {
            post_message(epoch);
        }
        s_msg_len = 0;
    }
}

static void ws_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)base;
    const uint32_t epoch = (uint32_t)(uintptr_t)arg;
    if (epoch != s_epoch) {
        return;
    }
    switch (id) {
    case WEBSOCKET_EVENT_CONNECTED:
        post(CMD_WS_CONNECTED, epoch, false);
        break;
    case WEBSOCKET_EVENT_DATA:
        on_data((const esp_websocket_event_data_t *)data, epoch);
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
    case WEBSOCKET_EVENT_CLOSED:
    case WEBSOCKET_EVENT_ERROR:
        post(CMD_WS_LOST, epoch, false);
        break;
    default:
        break;
    }
}

/* ---- reply audio (rx task) ----------------------------------------------------------------------- */

/* Peak limiter: the envelope rises at once with a louder sample, so no sample exceeds OUT_PEAK. */
static void limit(int16_t *pcm, size_t count)
{
    float env = s_peak_env;
    for (size_t i = 0; i < count; i++) {
        const float a = (float)(pcm[i] < 0 ? -pcm[i] : pcm[i]);
        env = a > env ? a : env * OUT_RELEASE;
        if (env > OUT_PEAK) {
            pcm[i] = (int16_t)((float)pcm[i] * (OUT_PEAK / env));
        }
    }
    s_peak_env = env;
}

static void play(int16_t *pcm, size_t count, uint32_t epoch)
{
    limit(pcm, count);
    while (count > 0 && epoch == s_epoch && !s_await_new) {
        const size_t n = sugarat_audio_stream_write(pcm, count);
        pcm += n;
        count -= n;
        if (count > 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    portENTER_CRITICAL(&s_mux);
    if (s_status.turn == SUGARAT_AGENT_TURN_SPEAKING) {
        s_turn_since_us = esp_timer_get_time();
    }
    portEXIT_CRITICAL(&s_mux);
}

/* 24 kHz PCM16 to 16 kHz: a [1 2 1]/4 low-pass against aliasing, then linear interpolation.
 * Filter history and phase carry across packets. */
static void play_pcm24(const uint8_t *bytes, size_t len, uint32_t epoch)
{
    const int16_t *in = (const int16_t *)bytes;
    const size_t count = len / 2;
    size_t out = 0;
    for (size_t i = 0; i < count; i++) {
        const int16_t x = (int16_t)(((int32_t)s_lp2 + 2 * (int32_t)s_lp1 + (int32_t)in[i]) / 4);
        s_lp2 = s_lp1;
        s_lp1 = in[i];
        while (s_rs_pos <= 1.0f) {
            s_pcm_out[out++] = (int16_t)(s_rs_prev + (x - s_rs_prev) * s_rs_pos);
            s_rs_pos += 1.5f;
            if (out == PCM_OUT_CAP) {
                play(s_pcm_out, out, epoch);
                out = 0;
            }
        }
        s_rs_pos -= 1.0f;
        s_rs_prev = x;
    }
    play(s_pcm_out, out, epoch);
}

static bool open_decoder(void)
{
    if (s_decoder) {
        return true;
    }
    esp_opus_dec_cfg_t cfg = ESP_OPUS_DEC_CONFIG_DEFAULT();
    cfg.sample_rate = 16000; /* the codec resamples the 24 kHz stream */
    cfg.channel = 1;
    cfg.frame_duration = ESP_OPUS_DEC_FRAME_DURATION_INVALID;
    cfg.self_delimited = false;
    if (esp_opus_dec_open(&cfg, sizeof(cfg), &s_decoder) != ESP_AUDIO_ERR_OK) {
        s_decoder = NULL;
        ESP_LOGE(TAG, "Opus decoder unavailable");
        return false;
    }
    return true;
}

static void play_packet(uint8_t *packet, size_t len, uint32_t epoch)
{
    if (len == 0 || s_await_new) {
        return;
    }
    if (!s_output_opus) {
        play_pcm24(packet, len, epoch);
        return;
    }
    if (!open_decoder()) {
        return;
    }
    esp_audio_dec_in_raw_t raw = { .buffer = packet, .len = (uint32_t)len };
    esp_audio_dec_out_frame_t frame = { .buffer = (uint8_t *)s_pcm_out, .len = PCM_OUT_CAP * 2 };
    esp_audio_dec_info_t info;
    if (esp_opus_dec_decode(s_decoder, &raw, &frame, &info) != ESP_AUDIO_ERR_OK) {
        ESP_LOGW(TAG, "Opus decode failed (%u bytes)", (unsigned)len);
        return;
    }
    play(s_pcm_out, frame.decoded_size / 2, epoch);
}

static void log_terminal(const cJSON *json, const char *event)
{
    const cJSON *code = cJSON_GetObjectItemCaseSensitive(json, "code");
    const cJSON *err = cJSON_GetObjectItemCaseSensitive(json, "error");
    if (!cJSON_IsString(code) && cJSON_IsObject(err)) {
        code = cJSON_GetObjectItemCaseSensitive(err, "code");
    }
    /* Server messages may echo the connect token, so only the event and its code are logged. */
    ESP_LOGW(TAG, "voice server sent %s (code %s)", event,
             cJSON_IsString(code) && strlen(code->valuestring) < 48 ? code->valuestring : "-");
}

static void handle_text(char *text, size_t len, uint32_t epoch)
{
    cJSON *json = cJSON_ParseWithLength(text, len);
    if (!json) {
        ESP_LOGW(TAG, "unparseable server message (%u bytes)", (unsigned)len);
        return;
    }
    const cJSON *type = cJSON_GetObjectItemCaseSensitive(json, "type");
    const char *event = cJSON_IsString(type) ? type->valuestring : "";

    if (strcmp(event, "session.ready") == 0) {
        bool in = s_grant.input_opus, out = s_grant.output_opus;
        if (!sugarat_agent_cloud_parse_audio(json, &in, &out)) {
            ESP_LOGE(TAG, "session.ready names an unsupported audio format");
            post(CMD_WS_LOST, epoch, true);
        } else {
            s_output_opus = out;
            s_rs_prev = 0;
            s_rs_pos = 0;
            s_lp1 = s_lp2 = 0;
            s_peak_env = 0;
            agent_uplink_configure(in, s_client_mode);
            ESP_LOGI(TAG, "voice ready (%s up, %s down, %s)", in ? "Opus" : "PCM16", out ? "Opus" : "PCM16",
                     s_client_mode ? "push-to-talk" : "server endpointing");
            post(CMD_READY, epoch, false);
        }
    } else if (strcmp(event, "response.started") == 0) {
        portENTER_CRITICAL(&s_mux);
        s_await_new = false;
        s_reply_done = false;
        s_replies_on_link++;
        if (s_status.turn != SUGARAT_AGENT_TURN_LISTENING) {
            s_status.turn = SUGARAT_AGENT_TURN_SPEAKING;
            s_turn_since_us = esp_timer_get_time();
        }
        s_status.error = SUGARAT_AGENT_VOICE_OK;
        s_status.revision++;
        portEXIT_CRITICAL(&s_mux);
    } else if (strcmp(event, "response.audio.delta") == 0) {
        cJSON *audio = cJSON_GetObjectItemCaseSensitive(json, "audio");
        if (cJSON_IsString(audio) && !s_await_new) {
            unsigned char *b64 = (unsigned char *)audio->valuestring;
            const size_t n = strlen(audio->valuestring);
            size_t got = 0;
            /* Decoding in place is safe: output never overtakes input. */
            if (mbedtls_base64_decode(b64, n, &got, b64, n) == 0) {
                play_packet(b64, got, epoch);
            } else {
                ESP_LOGW(TAG, "invalid audio delta");
            }
        }
    } else if (strcmp(event, "response.audio.done") == 0) {
        s_reply_done = true;
    } else if (strcmp(event, "response.interrupted") == 0) {
        if (!s_await_new) {
            sugarat_audio_stream_flush();
        }
        s_reply_done = true;
    } else if (strcmp(event, "transcript.partial") == 0 || strcmp(event, "transcript.final") == 0) {
        ESP_LOGD(TAG, "%s", event);
    } else if (strcmp(event, "error") == 0 || strcmp(event, "session.error") == 0 ||
               strcmp(event, "session.ended") == 0) {
        log_terminal(json, event);
        post(CMD_WS_LOST, epoch, true);
    }
    cJSON_Delete(json);
}

static void rx_task(void *arg)
{
    (void)arg;
    rx_item_t item;
    for (;;) {
        if (xQueueReceive(s_rx, &item, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (item.epoch == s_epoch) {
            if (item.binary) {
                play_packet((uint8_t *)item.data, item.len, item.epoch);
            } else {
                handle_text(item.data, item.len, item.epoch);
            }
        }
        heap_caps_free(item.data);
    }
}

/* ---- connection (control task) ----------------------------------------------------------------- */

static void link_down(void)
{
    agent_uplink_set_sender(NULL);
    xSemaphoreTake(s_ws_lock, portMAX_DELAY);
    esp_websocket_client_handle_t ws = s_ws;
    s_ws = NULL;
    s_ws_ready = false;
    s_epoch++; /* events still in flight belong to the old connection */
    xSemaphoreGive(s_ws_lock);
    if (ws) {
        if (esp_websocket_client_is_connected(ws)) {
            (void)esp_websocket_client_close(ws, pdMS_TO_TICKS(1000));
        }
        (void)esp_websocket_client_destroy(ws);
    }
    if (s_granted) {
        sugarat_agent_cloud_end_grant(s_agent_id, s_conversation);
        s_granted = false;
    }
    memset(s_grant.token, 0, sizeof(s_grant.token));
    s_ready_deadline_us = 0;
    s_hello_sent = false;
}

static void schedule_retry(sugarat_agent_voice_error_t error)
{
    s_failures++;
    const int64_t delay = s_failures <= FAST_RETRIES ? (1000000LL << (s_failures - 1)) : SLOW_RETRY_US;
    s_retry_at_us = esp_timer_get_time() + delay;
    status_set_link(s_failures > FAST_RETRIES ? SUGARAT_AGENT_LINK_FAILED : SUGARAT_AGENT_LINK_RECONNECTING, error);
    ESP_LOGW(TAG, "voice retry %u in %lld ms (error %d)", s_failures, (long long)(delay / 1000), (int)error);
}

/* Wi-Fi power save wakes the radio in bursts at every beacon, and the bursts are audible as a
 * buzz through the speaker amplifier. The radio stays awake while the chat is open. */
static void set_radio_awake(bool awake)
{
    const esp_err_t err = esp_wifi_set_ps(awake ? WIFI_PS_NONE : WIFI_PS_MIN_MODEM);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi power save change failed: %s", esp_err_to_name(err));
    }
}

static bool start_audio(void)
{
    if (s_audio_on) {
        return true;
    }
    const sugarat_audio_stream_config_t cfg = {
        .on_mic = agent_uplink_on_mic,
        .user_data = NULL,
        .playback_buffer_ms = PLAYBACK_MS,
    };
    const esp_err_t err = sugarat_audio_stream_start(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "audio stream failed: %s", esp_err_to_name(err));
        return false;
    }
    s_audio_on = true;
    return true;
}

/* One attempt: conversation (once per chat), grant, WebSocket start. */
static void connect_once(void)
{
    s_retry_at_us = 0;
    sugarat_agent_cloud_status_t cloud;
    sugarat_agent_cloud_status(&cloud);
    if (cloud.phase != SUGARAT_AGENT_CLOUD_ONLINE) {
        status_set_link(s_link_was_ready ? SUGARAT_AGENT_LINK_RECONNECTING : SUGARAT_AGENT_LINK_CONNECTING, SUGARAT_AGENT_VOICE_ERR_OFFLINE);
        s_retry_at_us = esp_timer_get_time() + 1000000LL; /* poll until signed in */
        return;
    }
    if (!s_conversation[0] || s_agent_epoch != cloud.agent_epoch) {
        s_agent_id = cloud.agent_id;
        s_agent_epoch = cloud.agent_epoch;
        bool reused = false;
        if (sugarat_agent_cloud_new_conversation(s_agent_id, s_conversation, sizeof(s_conversation), &reused) != ESP_OK) {
            s_conversation[0] = 0;
            schedule_retry(SUGARAT_AGENT_VOICE_ERR_CONVERSATION);
            return;
        }
        portENTER_CRITICAL(&s_mux);
        s_status.reused_conversation = reused;
        s_status.revision++;
        portEXIT_CRITICAL(&s_mux);
        ESP_LOGI(TAG, "%s conversation for agent %d", reused ? "reusing newest" : "new", s_agent_id);
    }
    if (!start_audio()) {
        schedule_retry(SUGARAT_AGENT_VOICE_ERR_AUDIO);
        return;
    }
    if (sugarat_agent_cloud_grant(s_agent_id, s_conversation, &s_grant) != ESP_OK) {
        schedule_retry(SUGARAT_AGENT_VOICE_ERR_GRANT);
        return;
    }
    s_granted = true;

    const esp_websocket_client_config_t cfg = {
        .uri = s_grant.url,
        .subprotocol = AGENT_VOICE_SUBPROTOCOL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 2048,
        .task_stack = 6144,
        .task_prio = 5,
        .disable_auto_reconnect = true,
        .network_timeout_ms = 10000,
        .ping_interval_sec = 10,
        .pingpong_timeout_sec = 30,
    };
    xSemaphoreTake(s_ws_lock, portMAX_DELAY);
    const uint32_t epoch = ++s_epoch;
    s_ws = esp_websocket_client_init(&cfg);
    esp_websocket_client_handle_t ws = s_ws;
    xSemaphoreGive(s_ws_lock);
    if (!ws ||
        esp_websocket_register_events(ws, WEBSOCKET_EVENT_ANY, ws_event, (void *)(uintptr_t)epoch) != ESP_OK ||
        esp_websocket_client_start(ws) != ESP_OK) {
        link_down();
        schedule_retry(SUGARAT_AGENT_VOICE_ERR_CONNECT);
        return;
    }
    portENTER_CRITICAL(&s_mux);
    s_replies_on_link = 0;
    portEXIT_CRITICAL(&s_mux);
    s_ready_deadline_us = esp_timer_get_time() + READY_TIMEOUT_US;
    status_set_link(s_link_was_ready ? SUGARAT_AGENT_LINK_RECONNECTING : SUGARAT_AGENT_LINK_CONNECTING, SUGARAT_AGENT_VOICE_OK);
    ESP_LOGI(TAG, "voice connecting; internal free %u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

static void send_hello(void)
{
    char hello[sizeof(s_grant.token) * 2 + 128];
    size_t n = (size_t)snprintf(hello, sizeof(hello), "{\"type\":\"session.connect\",\"connect_token\":\"");
    for (const char *p = s_grant.token; *p && n + 4 < sizeof(hello); p++) {
        if (*p == '"' || *p == '\\') {
            hello[n++] = '\\';
        }
        hello[n++] = *p;
    }
    n += (size_t)snprintf(hello + n, sizeof(hello) - n, "\",\"endpointing\":{\"mode\":\"%s\"}}",
                          s_client_mode ? "client" : "server");
    bool sent = false;
    xSemaphoreTake(s_ws_lock, portMAX_DELAY);
    if (s_ws) {
        sent = esp_websocket_client_send_text(s_ws, hello, (int)n, pdMS_TO_TICKS(2000)) >= 0;
    }
    xSemaphoreGive(s_ws_lock);
    memset(hello, 0, sizeof(hello));
    memset(s_grant.token, 0, sizeof(s_grant.token)); /* one use only */
    s_hello_sent = sent;
    if (!sent) {
        ESP_LOGW(TAG, "session.connect could not be sent");
        link_down();
        schedule_retry(SUGARAT_AGENT_VOICE_ERR_CONNECT);
    }
}

static void on_ready(void)
{
    xSemaphoreTake(s_ws_lock, portMAX_DELAY);
    s_ws_ready = true;
    xSemaphoreGive(s_ws_lock);
    s_ready_deadline_us = 0;
    s_failures = 0;
    s_preready_losses = 0;
    s_link_was_ready = true;
    agent_uplink_set_sender(ws_send);
    status_set_link(SUGARAT_AGENT_LINK_READY, SUGARAT_AGENT_VOICE_OK);
}

static void on_lost(bool terminal)
{
    const bool was_ready = s_ws_ready;
    const bool refused = !was_ready && s_hello_sent && (terminal || ++s_preready_losses >= 2);
    link_down();
    if (refused && s_client_mode) {
        /* The server refused push-to-talk endpointing; use the server's turn detection. */
        ESP_LOGW(TAG, "push-to-talk endpointing refused; switching to server endpointing");
        s_client_mode = false;
        s_retry_at_us = esp_timer_get_time();
        return;
    }
    schedule_retry(terminal ? SUGARAT_AGENT_VOICE_ERR_REJECTED : SUGARAT_AGENT_VOICE_ERR_CONNECT);
}

/* Release the conversation, its connection and queued speech. Status is set by the callers. */
static void end_chat(void)
{
    link_down();
    agent_uplink_discard();
    s_conversation[0] = 0;
    s_retry_at_us = 0;
    s_failures = 0;
    s_link_was_ready = false;
}

static void close_chat(void)
{
    if (!s_want_open) {
        return;
    }
    s_want_open = false;
    end_chat();
    portENTER_CRITICAL(&s_mux);
    const bool reopened = s_status.open; /* sugarat_agent_voice_open() already ran again */
    portEXIT_CRITICAL(&s_mux);
    if (s_audio_on && !reopened) {
        sugarat_audio_stop();
        s_audio_on = false;
        set_radio_awake(false);
    }
}

static void open_chat(void)
{
    if (s_want_open) {
        end_chat(); /* every opening is a new conversation */
        status_set_turn(SUGARAT_AGENT_TURN_IDLE, SUGARAT_AGENT_VOICE_OK);
    }
    s_want_open = true;
    portENTER_CRITICAL(&s_mux);
    s_await_new = false;
    s_reply_done = false;
    portEXIT_CRITICAL(&s_mux);
    set_radio_awake(true);
    /* The microphone starts before the network work, so an early press is captured. */
    if (!start_audio()) {
        status_set_link(SUGARAT_AGENT_LINK_CONNECTING, SUGARAT_AGENT_VOICE_ERR_AUDIO);
    }
    connect_once();
}

static void tick(void)
{
    const int64_t now = esp_timer_get_time();
    if (!s_want_open) {
        return;
    }
    if (s_ready_deadline_us && now > s_ready_deadline_us) {
        ESP_LOGW(TAG, "voice session not ready in time");
        on_lost(false);
    }
    sugarat_agent_cloud_status_t cloud;
    sugarat_agent_cloud_status(&cloud);
    if (s_conversation[0] && cloud.phase == SUGARAT_AGENT_CLOUD_ONLINE && cloud.agent_epoch != s_agent_epoch) {
        ESP_LOGI(TAG, "bound agent changed; starting a new conversation");
        link_down();
        s_conversation[0] = 0;
        s_failures = 0;
        s_retry_at_us = now;
    } else if (cloud.phase != SUGARAT_AGENT_CLOUD_ONLINE && (s_ws || s_granted)) {
        link_down();
        status_set_link(SUGARAT_AGENT_LINK_RECONNECTING, SUGARAT_AGENT_VOICE_ERR_OFFLINE);
        s_retry_at_us = now + 1000000LL;
    }
    if (s_retry_at_us && now >= s_retry_at_us && !s_ws) {
        connect_once();
    }

    portENTER_CRITICAL(&s_mux);
    const sugarat_agent_turn_t turn = s_status.turn;
    const int64_t since = s_turn_since_us;
    const bool done = s_reply_done;
    portEXIT_CRITICAL(&s_mux);
    if (turn == SUGARAT_AGENT_TURN_SPEAKING) {
        sugarat_audio_status_t audio;
        const bool active = sugarat_audio_get_status(&audio) && audio.out_active;
        if (sugarat_audio_stream_queued() == 0 && !active && (done || now - since > SPEAK_IDLE_US)) {
            status_set_turn(SUGARAT_AGENT_TURN_IDLE, SUGARAT_AGENT_VOICE_OK);
        }
    } else if (turn == SUGARAT_AGENT_TURN_THINKING && now - since > THINK_TIMEOUT_US) {
        ESP_LOGW(TAG, "no reply within %lld s", (long long)(THINK_TIMEOUT_US / 1000000));
        status_set_turn(SUGARAT_AGENT_TURN_IDLE, SUGARAT_AGENT_VOICE_ERR_NO_REPLY);
        if (s_client_mode && s_replies_on_link == 0 && s_ws_ready) {
            /* The server accepted push-to-talk but never answers it; use server endpointing. */
            ESP_LOGW(TAG, "no reply to push-to-talk; switching to server endpointing");
            s_client_mode = false;
            link_down();
            s_retry_at_us = now;
        }
    }
}

static void control_task(void *arg)
{
    (void)arg;
    cmd_t cmd;
    for (;;) {
        if (xQueueReceive(s_cmds, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            switch ((cmd_type_t)cmd.type) {
            case CMD_OPEN:
                open_chat();
                break;
            case CMD_CLOSE:
                close_chat();
                break;
            case CMD_WS_CONNECTED:
                if (cmd.epoch == s_epoch && s_ws) {
                    send_hello();
                }
                break;
            case CMD_READY:
                if (cmd.epoch == s_epoch && s_ws) {
                    on_ready();
                }
                break;
            case CMD_WS_LOST:
                if (cmd.epoch == s_epoch && s_ws) {
                    on_lost(cmd.terminal);
                }
                break;
            }
        }
        tick();
    }
}

esp_err_t sugarat_agent_voice_init(void)
{
    const esp_err_t err = agent_uplink_init();
    if (err != ESP_OK) {
        return err;
    }
    s_cmds = xQueueCreate(16, sizeof(cmd_t));
    s_rx = xQueueCreateWithCaps(RX_QUEUE_ITEMS, sizeof(rx_item_t), PSRAM_CAPS);
    s_ws_lock = xSemaphoreCreateMutex();
    s_msg = heap_caps_malloc(MESSAGE_CAP, PSRAM_CAPS);
    s_pcm_out = heap_caps_malloc(PCM_OUT_CAP * sizeof(int16_t), PSRAM_CAPS);
    if (!s_cmds || !s_rx || !s_ws_lock || !s_msg || !s_pcm_out) {
        return ESP_ERR_NO_MEM;
    }
    /* Neither task writes flash, so both stacks can live in PSRAM. */
    if (xTaskCreatePinnedToCoreWithCaps(control_task, "agent_voice", 8192, NULL, 4, NULL, tskNO_AFFINITY,
                                        PSRAM_CAPS) != pdPASS ||
        xTaskCreatePinnedToCoreWithCaps(rx_task, "agent_rx", 24 * 1024, NULL, 5, NULL, 1, PSRAM_CAPS) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
