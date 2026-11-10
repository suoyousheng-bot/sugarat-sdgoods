/*
 * Deotaland Agent Link microphone uplink: push-to-talk turns, pre-roll, Opus encoding and the outbox.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "agent_uplink.h"

#include <stdio.h>
#include <string.h>

#include "encoder/impl/esp_opus_enc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"

static const char *TAG = "agent_uplink";

#define FRAME            320                 /* 20 ms at 16 kHz */
#define FRAME_BYTES      (FRAME * 2)
#define RING_FRAMES      40                  /* 800 ms of recent capture */
#define PREROLL_MS       300                 /* captured before the press and sent with the turn */
#define OUTBOX_ITEMS     500                 /* 10 s of 20 ms packets */
#define ITEM_DATA        FRAME_BYTES         /* large enough for raw PCM fallback */
#define SEND_BURST       12                  /* messages per pass, so capture keeps being read */
#define JSON_CAP         (64 + ((ITEM_DATA + 2) / 3) * 4)

typedef enum { ITEM_START = 0, ITEM_AUDIO, ITEM_END } item_kind_t;
typedef struct {
    uint8_t kind;
    uint16_t len;
    uint8_t data[ITEM_DATA];
} item_t;

typedef enum { TALK_IDLE = 0, TALK_PRESSED, TALK_ACTIVE, TALK_RELEASING } talk_t;

typedef struct {
    int16_t pcm[FRAME];
    int64_t stamp_us; /* when the frame was complete */
} ring_frame_t;

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static talk_t s_talk;
static int64_t s_press_us;

static StreamBufferHandle_t s_pcm_in;
static QueueHandle_t s_outbox;
static ring_frame_t *s_ring;
static unsigned s_ring_head, s_ring_count;
static void *s_encoder;
static uint8_t *s_packet;
static int s_packet_size;

static volatile agent_uplink_send_fn s_send;
static volatile bool s_opus = true;
static volatile bool s_client = true;
static volatile bool s_reconfigured;
static volatile bool s_discard;
static bool s_wire_turn_open; /* encoder task only */
static unsigned s_dropped;

static item_t s_item;         /* encoder task scratch */
static char s_json[JSON_CAP];

/* ---- public ----------------------------------------------------------------------------------- */

void agent_uplink_on_mic(const int16_t *samples, size_t count, void *user_data)
{
    (void)user_data;
    if (s_pcm_in) {
        /* Write whole samples only: a full buffer would otherwise accept an odd byte count and
         * shift every later sample by one byte. This is the only writer, so the space cannot
         * shrink before the send. */
        const size_t room = xStreamBufferSpacesAvailable(s_pcm_in) / sizeof(int16_t);
        if (count > room) {
            count = room;
        }
        if (count > 0) {
            (void)xStreamBufferSend(s_pcm_in, samples, count * sizeof(int16_t), 0);
        }
    }
}

void agent_uplink_configure(bool input_opus, bool client_endpointing)
{
    s_opus = input_opus;
    s_client = client_endpointing;
    s_reconfigured = true;
}

void agent_uplink_set_sender(agent_uplink_send_fn send)
{
    s_send = send;
}

bool agent_uplink_talk(bool down)
{
    bool result = true;
    portENTER_CRITICAL(&s_mux);
    if (down) {
        if (s_talk == TALK_IDLE) {
            s_talk = TALK_PRESSED;
            s_press_us = esp_timer_get_time();
        }
    } else if (s_talk == TALK_PRESSED) {
        s_talk = TALK_IDLE;
        result = false;
    } else if (s_talk == TALK_ACTIVE) {
        s_talk = TALK_RELEASING;
    } else {
        result = s_talk == TALK_RELEASING;
    }
    portEXIT_CRITICAL(&s_mux);
    return result;
}

void agent_uplink_discard(void)
{
    portENTER_CRITICAL(&s_mux);
    s_talk = TALK_IDLE;
    portEXIT_CRITICAL(&s_mux);
    s_discard = true;
}

size_t agent_uplink_pending(void)
{
    return s_outbox ? uxQueueMessagesWaiting(s_outbox) : 0;
}

/* ---- encoder task ------------------------------------------------------------------------------- */

static bool open_encoder(void)
{
    if (s_encoder) {
        return true;
    }
    esp_opus_enc_config_t cfg = ESP_OPUS_ENC_CONFIG_DEFAULT();
    cfg.sample_rate = 16000;
    cfg.channel = 1;
    cfg.bits_per_sample = 16;
    cfg.bitrate = 24000;
    cfg.frame_duration = ESP_OPUS_ENC_FRAME_DURATION_20_MS;
    cfg.application_mode = ESP_OPUS_ENC_APPLICATION_VOIP;
    cfg.complexity = 0;
    cfg.enable_vbr = true;
    const size_t before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    int in_size = 0, out_size = 0;
    if (esp_opus_enc_open(&cfg, sizeof(cfg), &s_encoder) != ESP_AUDIO_ERR_OK ||
        esp_opus_enc_get_frame_size(s_encoder, &in_size, &out_size) != ESP_AUDIO_ERR_OK ||
        in_size != FRAME_BYTES || out_size <= 0 ||
        !(s_packet = heap_caps_malloc((size_t)out_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT))) {
        ESP_LOGE(TAG, "Opus encoder unavailable (input %d, output %d)", in_size, out_size);
        if (s_encoder) {
            esp_opus_enc_close(s_encoder);
            s_encoder = NULL;
        }
        return false;
    }
    s_packet_size = out_size;
    ESP_LOGI(TAG, "Opus encoder ready; internal RAM used %d bytes, free %u",
             (int)(before - heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    return true;
}

static void push(item_kind_t kind, const uint8_t *data, size_t len)
{
    s_item.kind = (uint8_t)kind;
    s_item.len = (uint16_t)len;
    if (len) {
        memcpy(s_item.data, data, len);
    }
    if (xQueueSend(s_outbox, &s_item, 0) != pdTRUE) {
        if (s_dropped++ % 50 == 0) {
            ESP_LOGW(TAG, "outbox full; dropped %u messages", s_dropped);
        }
    }
}

static void push_frame(const int16_t *pcm)
{
    if (!s_opus) {
        push(ITEM_AUDIO, (const uint8_t *)pcm, FRAME_BYTES);
        return;
    }
    if (!open_encoder()) {
        return;
    }
    esp_audio_enc_in_frame_t in = { .buffer = (uint8_t *)pcm, .len = FRAME_BYTES };
    esp_audio_enc_out_frame_t out = { .buffer = s_packet, .len = (uint32_t)s_packet_size };
    if (esp_opus_enc_process(s_encoder, &in, &out) != ESP_AUDIO_ERR_OK || out.encoded_bytes == 0 ||
        out.encoded_bytes > ITEM_DATA) {
        ESP_LOGW(TAG, "Opus encode failed");
        return;
    }
    push(ITEM_AUDIO, s_packet, out.encoded_bytes);
}

static void ring_put(const int16_t *pcm, int64_t stamp)
{
    ring_frame_t *f = &s_ring[s_ring_head];
    memcpy(f->pcm, pcm, FRAME_BYTES);
    f->stamp_us = stamp;
    s_ring_head = (s_ring_head + 1) % RING_FRAMES;
    if (s_ring_count < RING_FRAMES) {
        s_ring_count++;
    }
}

/* Encode buffered frames captured at or after `since`, oldest first, then empty the ring. */
static void ring_flush(int64_t since)
{
    const unsigned start = (s_ring_head + RING_FRAMES - s_ring_count) % RING_FRAMES;
    for (unsigned i = 0; i < s_ring_count; i++) {
        const ring_frame_t *f = &s_ring[(start + i) % RING_FRAMES];
        if (f->stamp_us >= since) {
            push_frame(f->pcm);
        }
    }
    s_ring_count = 0;
}

static void handle_frame(const int16_t *pcm, int64_t now)
{
    bool promote = false, finish = false;
    talk_t state;
    int64_t press;
    portENTER_CRITICAL(&s_mux);
    if (s_talk == TALK_PRESSED && now - s_press_us >= AGENT_UPLINK_MIN_HOLD_MS * 1000LL) {
        s_talk = TALK_ACTIVE;
        promote = true;
    } else if (s_talk == TALK_RELEASING) {
        s_talk = TALK_IDLE;
        finish = true;
    }
    state = s_talk;
    press = s_press_us;
    portEXIT_CRITICAL(&s_mux);

    if (promote) {
        if (s_client) {
            push(ITEM_START, NULL, 0);
        }
        ring_put(pcm, now);
        ring_flush(press - PREROLL_MS * 1000LL);
        return;
    }
    if (state == TALK_ACTIVE) {
        push_frame(pcm);
        return;
    }
    if (finish) {
        push_frame(pcm);
        if (s_client) {
            push(ITEM_END, NULL, 0);
        }
        return;
    }
    ring_put(pcm, now);
    if (!s_client && s_send && state == TALK_IDLE) {
        /* Server endpointing needs a continuous stream; silence outside turns ends them. */
        static const int16_t silence[FRAME] = { 0 };
        push_frame(silence);
    }
}

static size_t to_json(const item_t *item)
{
    if (item->kind == ITEM_START) {
        return (size_t)snprintf(s_json, sizeof(s_json), "{\"type\":\"input_audio.activity\",\"kind\":\"start\"}");
    }
    if (item->kind == ITEM_END) {
        return (size_t)snprintf(s_json, sizeof(s_json), "{\"type\":\"input_audio.activity\",\"kind\":\"end\"}");
    }
    static const char prefix[] = "{\"type\":\"input_audio.append\",\"audio\":\"";
    size_t n = sizeof(prefix) - 1;
    memcpy(s_json, prefix, n);
    size_t written = 0;
    if (mbedtls_base64_encode((unsigned char *)s_json + n, sizeof(s_json) - n - 3, &written, item->data, item->len) != 0) {
        return 0;
    }
    n += written;
    s_json[n++] = '"';
    s_json[n++] = '}';
    s_json[n] = 0;
    return n;
}

static bool send_json(agent_uplink_send_fn send, size_t len)
{
    return len > 0 && send(s_json, len);
}

static void drain(void)
{
    const agent_uplink_send_fn send = s_send;
    if (!send) {
        return;
    }
    for (int i = 0; i < SEND_BURST && xQueuePeek(s_outbox, &s_item, 0) == pdTRUE; i++) {
        if (!s_client && s_item.kind != ITEM_AUDIO) {
            (void)xQueueReceive(s_outbox, &s_item, 0); /* the server finds turn edges itself */
            continue;
        }
        if (s_client && s_item.kind != ITEM_START && !s_wire_turn_open) {
            /* The turn started on a connection that was lost: open it again on this one. */
            if (!send_json(send, (size_t)snprintf(s_json, sizeof(s_json),
                                                  "{\"type\":\"input_audio.activity\",\"kind\":\"start\"}"))) {
                return;
            }
            s_wire_turn_open = true;
        }
        if (!send_json(send, to_json(&s_item))) {
            return; /* keep the message; the connection is being replaced */
        }
        if (s_item.kind == ITEM_START) {
            s_wire_turn_open = true;
        } else if (s_item.kind == ITEM_END) {
            s_wire_turn_open = false;
        }
        (void)xQueueReceive(s_outbox, &s_item, 0);
    }
}

static void uplink_task(void *arg)
{
    (void)arg;
    static int16_t frame[FRAME];
    size_t have = 0;
    (void)open_encoder(); /* ready before the first press */
    for (;;) {
        if (s_discard) {
            s_discard = false;
            xQueueReset(s_outbox);
            s_ring_count = 0;
            s_wire_turn_open = false;
            have = 0;
        }
        if (s_reconfigured) {
            s_reconfigured = false;
            s_wire_turn_open = false;
        }
        const size_t got = xStreamBufferReceive(s_pcm_in, (uint8_t *)frame + have, FRAME_BYTES - have, pdMS_TO_TICKS(20));
        have += got;
        if (have == FRAME_BYTES) {
            have = 0;
            handle_frame(frame, esp_timer_get_time());
        }
        drain();
    }
}

esp_err_t agent_uplink_init(void)
{
    s_pcm_in = xStreamBufferCreateWithCaps(32 * 1024, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_outbox = xQueueCreateWithCaps(OUTBOX_ITEMS, sizeof(item_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_ring = heap_caps_calloc(RING_FRAMES, sizeof(ring_frame_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_pcm_in || !s_outbox || !s_ring) {
        return ESP_ERR_NO_MEM;
    }
    /* Opus needs a deep stack; it lives in PSRAM. This task never writes flash. */
    if (xTaskCreatePinnedToCoreWithCaps(uplink_task, "agent_uplink", 32 * 1024, NULL, 5, NULL, 1,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
