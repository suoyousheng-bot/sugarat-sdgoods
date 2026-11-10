/* SPDX-License-Identifier: Apache-2.0 */
/** One worker serializes audio commands and consumes confirmed terminal events before reuse. */
#include "media_device.h"
#include "media_tracks.h"
#include "sugarat_audio.h"
#include "sdgoods_board.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

typedef struct { uint32_t id; media_terminal_t terminal; } terminal_event_t;
typedef struct {
    media_transport_t transport;
    media_snapshot_t published;
    QueueHandle_t commands, events;
    portMUX_TYPE lock;
    uint32_t driver_id;
    bool ready, overflow;
} device_t;
static device_t device = {.lock = portMUX_INITIALIZER_UNLOCKED};

static uint64_t now_ms(void) { return (uint64_t)esp_timer_get_time() / 1000; }
static bool start(void *context, media_request_t request)
{
    device_t *d = context;
    portENTER_CRITICAL(&d->lock); d->driver_id = request.id; portEXIT_CRITICAL(&d->lock);
    size_t offset = (size_t)request.position_ms * 16;
    esp_err_t result = sugarat_audio_play_pcm(media_tracks[request.track].samples + offset,
                                             media_tracks[request.track].count - offset);
    if (result != ESP_OK) ESP_LOGE("media", "PCM request failed: %s", esp_err_to_name(result));
    return result == ESP_OK;
}
static void stop(void *context) { (void)context; sugarat_audio_stop(); }
static void audio_event(const sugarat_audio_event_t *event, void *context)
{
    device_t *d = context;
    portENTER_CRITICAL(&d->lock); uint32_t id = d->driver_id; portEXIT_CRITICAL(&d->lock);
    terminal_event_t terminal = {.id = id};
    switch (event->type) {
    case SUGARAT_AUDIO_EVENT_PLAYBACK_DONE: terminal.terminal = MEDIA_FINISHED; break;
    case SUGARAT_AUDIO_EVENT_STOPPED: terminal.terminal = MEDIA_STOPPED; break;
    case SUGARAT_AUDIO_EVENT_ERROR:
        terminal.terminal = MEDIA_BROKEN; ESP_LOGE("media", "Audio failed: %s", esp_err_to_name(event->error)); break;
    case SUGARAT_AUDIO_EVENT_RECORD_DONE: return;
    }
    if (xQueueSend(d->events, &terminal, 0) != pdTRUE) {
        portENTER_CRITICAL(&d->lock); d->overflow = true; portEXIT_CRITICAL(&d->lock);
        ESP_LOGE("media", "Completion queue overflow; playback blocked until restart");
    }
}
static void worker(void *context)
{
    device_t *d = context;
    for (;;) {
        terminal_event_t event;
        while (xQueueReceive(d->events, &event, 0) == pdTRUE) {
            media_transport_terminal(&d->transport, event.id, event.terminal);
            portENTER_CRITICAL(&d->lock);
            if (event.id == d->driver_id) d->driver_id = 0;
            portEXIT_CRITICAL(&d->lock);
        }
        media_request_t command;
        if (xQueueReceive(d->commands, &command, 0) == pdTRUE) {
            if (!command.id || command.track >= MEDIA_TRACK_COUNT || command.position_ms > media_durations_ms[command.track]) {
                d->transport.state.phase = MEDIA_FAILED; d->transport.state.error = MEDIA_ERROR_DEVICE;
                ESP_LOGE("media", "Invalid queued playback request");
            } else media_transport_submit(&d->transport, command);
        }
        sugarat_audio_status_t audio;
        sugarat_audio_get_status(&audio);
        if (audio.state == SUGARAT_AUDIO_STATE_PLAYING)
            media_transport_progress(&d->transport, d->transport.active_id, audio.progress_ms);
        portENTER_CRITICAL(&d->lock); bool overflow = d->overflow; portEXIT_CRITICAL(&d->lock);
        if (overflow && !d->transport.fault_latched) {
            d->transport.fault_latched = true; d->transport.state.phase = MEDIA_FAILED;
            d->transport.state.error = MEDIA_ERROR_STOP_TIMEOUT; sugarat_audio_stop();
        }
        media_transport_tick(&d->transport, now_ms());
        portENTER_CRITICAL(&d->lock); d->published = d->transport.state; portEXIT_CRITICAL(&d->lock);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
static bool request(void *context, media_request_t command)
{
    device_t *d = context;
    if (!d->ready) return false;
    portENTER_CRITICAL(&d->lock); media_error_t error = d->published.error; portEXIT_CRITICAL(&d->lock);
    if (error == MEDIA_ERROR_STOP_TIMEOUT || error == MEDIA_ERROR_START_TIMEOUT) return false;
    return xQueueOverwrite(d->commands, &command) == pdTRUE;
}
static bool volume(void *context, uint8_t percent)
{
    device_t *d = context; if (!d->ready) return false;
    sdgoods_audio_set_volume(percent); return true;
}
static media_snapshot_t read(void *context)
{
    device_t *d = context;
    portENTER_CRITICAL(&d->lock); media_snapshot_t state = d->published; portEXIT_CRITICAL(&d->lock);
    state.volume = sdgoods_audio_get_volume(); return state;
}
void media_device_init(void)
{
    device_t *d = &device;
    d->published.phase = MEDIA_FAILED; d->published.error = MEDIA_ERROR_DEVICE;
    d->commands = xQueueCreate(1, sizeof(media_request_t)); d->events = xQueueCreate(8, sizeof(terminal_event_t));
    if (!d->commands || !d->events) { ESP_LOGE("media", "Audio queues unavailable"); return; }
    media_transport_init(&d->transport, (media_driver_t){start, stop, d}, media_durations_ms, MEDIA_TRACK_COUNT, 2000, 2000);
    sugarat_audio_config_t config = sugarat_audio_default_config();
    config.on_event = audio_event; config.user_data = d;
    esp_err_t result = sugarat_audio_init(&config);
    if (result != ESP_OK) { ESP_LOGE("media", "Audio initialization failed: %s", esp_err_to_name(result)); return; }
    sdgoods_audio_set_volume(40);
    if (xTaskCreate(worker, "media_owner", 4096, d, 4, NULL) != pdPASS) {
        ESP_LOGE("media", "Audio owner task unavailable"); return;
    }
    d->ready = true;
    ESP_LOGI("media", "Audio owner ready: 3 original 16 kHz tracks; no autoplay");
}
media_backend_t media_device_backend(void)
{
    return (media_backend_t){request, volume, read, &device};
}
