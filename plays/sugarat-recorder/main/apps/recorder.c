/* SPDX-License-Identifier: Apache-2.0 */
/* Coordinates audio jobs and a single file worker from the LVGL thread. */

#include "recorder.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sugarat_audio.h"
#include "voice_enhance.h"

#include <stdlib.h>

typedef enum { OPEN_STORE, SAVE_CLIP, LOAD_CLIP, DELETE_CLIP } job_kind_t;

typedef struct {
    job_kind_t kind;
    voice_clip_t clip;
    const int16_t *pcm;
    size_t samples;
    bool enhance;
} file_job_t;

typedef struct {
    job_kind_t kind;
    esp_err_t error;
    uint8_t *wav;
    size_t bytes;
} file_result_t;

static const char *TAG = "voice_recorder";
static QueueHandle_t s_jobs, s_results, s_audio_events;
static clip_store_t s_store;
static recorder_view_t s_view = { .state = REC_BOOT, .message = "Opening storage..." };
static bool s_audio_ready, s_cancel;
static uint8_t *s_playback;
static size_t s_play_frames, s_play_offset;
static bool s_pause_playback;
static voice_clip_t s_next_clip;

static void audio_event(const sugarat_audio_event_t *event, void *unused)
{
    (void)unused;
    xQueueOverwrite(s_audio_events, event);
}

static void file_worker(void *unused)
{
    (void)unused;
    for (;;) {
        file_job_t job;
        xQueueReceive(s_jobs, &job, portMAX_DELAY);
        file_result_t result = { .kind = job.kind, .error = ESP_FAIL };
        switch (job.kind) {
        case OPEN_STORE: result.error = clips_open(&s_store); break;
        case SAVE_CLIP:
            /* sugarat_audio leaves its recording untouched until the next capture, and a retry
             * after a failed save passes enhance = false so the audio is cleaned only once. */
            if (job.enhance) {
                const int64_t start = esp_timer_get_time();
                if (voice_enhance((int16_t *)job.pcm, job.samples)) {
                    ESP_LOGI(TAG, "Cleaned %u samples in %lld ms", (unsigned)job.samples,
                             (long long)(esp_timer_get_time() - start) / 1000);
                } else {
                    ESP_LOGW(TAG, "Saving without noise reduction: no working memory");
                }
            }
            result.error = clips_save(&s_store, job.pcm, job.samples);
            break;
        case LOAD_CLIP: result.error = clips_read(&s_store, job.clip, &result.wav, &result.bytes); break;
        case DELETE_CLIP: result.error = clips_delete(&s_store, job.clip.id); break;
        }
        xQueueSend(s_results, &result, portMAX_DELAY);
    }
}

static void set_state(recorder_state_t state, const char *message)
{
    s_view.state = state;
    s_view.message = message;
}

void recorder_init(void)
{
    s_jobs = xQueueCreate(1, sizeof(file_job_t));
    s_results = xQueueCreate(1, sizeof(file_result_t));
    s_audio_events = xQueueCreate(1, sizeof(sugarat_audio_event_t));
    if (!s_jobs || !s_results || !s_audio_events ||
        xTaskCreate(file_worker, "voice_files", 4096, NULL, 3, NULL) != pdPASS) {
        set_state(REC_UNAVAILABLE, "Restart to open recorder");
        return;
    }
    sugarat_audio_config_t config = sugarat_audio_default_config();
    config.record_max_ms = CLIP_MAX_MS;
    /* Loud speech and knocks saturated at the default shift of 14; voice_enhance restores level. */
    config.mic_shift = 15;
    config.process_recording = false;
    config.on_event = audio_event;
    esp_err_t error = sugarat_audio_init(&config);
    s_audio_ready = error == ESP_OK;
    if (!s_audio_ready) ESP_LOGE(TAG, "Audio initialization failed: %s", esp_err_to_name(error));
    const file_job_t job = { .kind = OPEN_STORE };
    xQueueSend(s_jobs, &job, 0);
}

static void save_capture(bool enhance)
{
    size_t samples;
    const int16_t *pcm = sugarat_audio_get_recording(&samples);
    if (!samples) {
        set_state(REC_IDLE, "No audio captured. Try again");
        return;
    }
    const file_job_t job = { .kind = SAVE_CLIP, .pcm = pcm, .samples = samples, .enhance = enhance };
    s_view.elapsed_ms = samples * 1000u / SUGARAT_AUDIO_SAMPLE_RATE;
    set_state(REC_SAVING, NULL);
    xQueueSend(s_jobs, &job, 0);
}

void recorder_toggle_capture(void)
{
    if (s_view.state == REC_RECORDING) {
        set_state(REC_FINISHING, "Finishing...");
        sugarat_audio_stop();
        return;
    }
    if (s_view.state == REC_UNSAVED) {
        save_capture(false);
        return;
    }
    if (s_view.state != REC_IDLE) return;
    if (s_view.count >= CLIP_LIMIT) {
        set_state(REC_IDLE, "Library full. Delete a recording");
        return;
    }
    esp_err_t error = sugarat_audio_record(CLIP_MAX_MS);
    if (error == ESP_OK) {
        s_view.elapsed_ms = 0;
        s_cancel = false;
        set_state(REC_STARTING, "Starting...");
    } else {
        ESP_LOGE(TAG, "Start recording failed: %s", esp_err_to_name(error));
        set_state(REC_IDLE, "Cannot start. Tap to retry");
    }
}

static void clear_playback(void)
{
    free(s_playback);
    s_playback = NULL;
    s_view.clip_id = 0;
    s_play_frames = s_play_offset = 0;
}

static void play_buffer(void)
{
    const int16_t *pcm = (const int16_t *)(s_playback + SUGARAT_AUDIO_WAV_HEADER_BYTES);
    const esp_err_t error = sugarat_audio_play_pcm(pcm + s_play_offset, s_play_frames - s_play_offset);
    if (error == ESP_OK) {
        s_cancel = s_pause_playback = false;
        set_state(REC_PLAY_STARTING, "Starting playback...");
    } else {
        ESP_LOGE(TAG, "Playback start failed: %s", esp_err_to_name(error));
        clear_playback();
        set_state(REC_IDLE, "Cannot play recording");
    }
}

static void load_clip(voice_clip_t clip)
{
    s_view.clip_id = clip.id;
    s_cancel = false;
    set_state(REC_LOADING, "Loading...");
    const file_job_t job = { .kind = LOAD_CLIP, .clip = clip };
    xQueueSend(s_jobs, &job, 0);
}

void recorder_toggle_playback(voice_clip_t clip)
{
    if (!clip.id) return;
    if (s_view.state == REC_PLAYING) {
        s_pause_playback = s_view.clip_id == clip.id;
        s_next_clip = s_pause_playback ? (voice_clip_t){0} : clip;
        set_state(REC_PLAY_STOPPING, "Stopping playback...");
        sugarat_audio_stop();
    } else if (s_view.state == REC_PAUSED && s_view.clip_id == clip.id) {
        play_buffer();
    } else if (s_view.state == REC_IDLE || s_view.state == REC_PAUSED) {
        clear_playback();
        load_clip(clip);
    }
}

void recorder_stop_playback(void)
{
    s_next_clip = (voice_clip_t){0};
    s_pause_playback = false;
    switch (s_view.state) {
    case REC_LOADING:
    case REC_PLAY_STARTING:
        s_cancel = true;
        break;
    case REC_PLAYING:
        set_state(REC_PLAY_STOPPING, "Stopping playback...");
        sugarat_audio_stop();
        break;
    case REC_PAUSED:
        clear_playback();
        set_state(REC_IDLE, NULL);
        break;
    default: break;
    }
}

void recorder_delete(uint32_t id)
{
    if (!id || (s_view.state != REC_IDLE && s_view.state != REC_UNSAVED)) return;
    s_cancel = s_view.state == REC_UNSAVED;
    const file_job_t job = { .kind = DELETE_CLIP, .clip.id = id };
    set_state(REC_DELETING, "Deleting...");
    xQueueSend(s_jobs, &job, 0);
}

void recorder_pause(void)
{
    recorder_stop_playback();
}

static void finish_audio(sugarat_audio_event_t event)
{
    s_view.level = 0;
    if (event.operation == SUGARAT_AUDIO_STATE_RECORDING) {
        s_view.elapsed_ms = event.duration_ms;
        if (event.error == ESP_OK) {
            save_capture(true);
        } else {
            set_state(REC_IDLE, "Recording failed. Try again");
        }
    } else if (event.operation == SUGARAT_AUDIO_STATE_PLAYING) {
        s_play_offset += event.duration_ms * (SUGARAT_AUDIO_SAMPLE_RATE / 1000u);
        if (s_pause_playback && event.type == SUGARAT_AUDIO_EVENT_STOPPED && s_play_offset < s_play_frames) {
            s_pause_playback = false;
            set_state(REC_PAUSED, NULL);
            return;
        }
        clear_playback();
        const voice_clip_t next = s_next_clip;
        s_next_clip = (voice_clip_t){0};
        if (next.id && event.error == ESP_OK) load_clip(next);
        else set_state(REC_IDLE, event.error == ESP_OK ? NULL : "Playback failed. Try again");
    }
}

static void finish_file(file_result_t result)
{
    if (result.error != ESP_OK) {
        ESP_LOGE(TAG, "File operation %d failed: %s", (int)result.kind, esp_err_to_name(result.error));
    }
    switch (result.kind) {
    case OPEN_STORE:
        s_view.storage_ready = result.error == ESP_OK;
        s_view.count = s_view.storage_ready ? s_store.count : 0;
        if (!s_view.storage_ready) {
            set_state(REC_UNAVAILABLE, "Storage unavailable");
        } else {
            set_state(s_audio_ready ? REC_IDLE : REC_UNAVAILABLE,
                      s_audio_ready ? NULL : "Audio unavailable. Restart");
        }
        break;
    case SAVE_CLIP:
        if (result.error == ESP_OK) {
            s_view.count = s_store.count;
            s_view.elapsed_ms = 0;
            set_state(REC_IDLE, "Recording saved");
        } else {
            set_state(REC_UNSAVED, result.error == ESP_ERR_NO_MEM || result.error == ESP_ERR_INVALID_SIZE
                                     ? "Storage full. Free space to save" : "Save failed. Tap to retry");
        }
        break;
    case LOAD_CLIP:
        if (!s_cancel && result.error == ESP_OK) {
            s_playback = result.wav;
            s_play_frames = (result.bytes - SUGARAT_AUDIO_WAV_HEADER_BYTES) / sizeof(int16_t);
            play_buffer();
            break;
        }
        free(result.wav);
        s_view.clip_id = 0;
        set_state(REC_IDLE, s_cancel ? NULL : "Cannot play recording");
        break;
    case DELETE_CLIP:
        s_view.count = s_store.count;
        set_state(s_cancel ? REC_UNSAVED : REC_IDLE, result.error == ESP_OK
                  ? (s_cancel ? "Tap to retry saving" : "Recording deleted") : "Cannot delete recording");
        break;
    }
}

void recorder_poll(void)
{
    if (!s_results || !s_audio_events) return;
    file_result_t file;
    if (xQueueReceive(s_results, &file, 0) == pdTRUE) finish_file(file);
    sugarat_audio_event_t audio;
    if (xQueueReceive(s_audio_events, &audio, 0) == pdTRUE) finish_audio(audio);
    if (!s_audio_ready) return;

    sugarat_audio_status_t status;
    sugarat_audio_get_status(&status);
    if (status.state == SUGARAT_AUDIO_STATE_RECORDING) {
        if (s_view.state == REC_STARTING) set_state(REC_RECORDING, NULL);
        if (s_view.state == REC_RECORDING) {
            s_view.elapsed_ms = status.progress_ms;
            s_view.level = status.level_percent;
        }
    } else if (status.state == SUGARAT_AUDIO_STATE_PLAYING && s_view.state == REC_PLAY_STARTING) {
        set_state(REC_PLAYING, NULL);
        if (s_cancel) recorder_stop_playback();
    }
}

recorder_view_t recorder_view(void)
{
    return s_view;
}

voice_clip_t recorder_clip(size_t index)
{
    /* The worker can mutate the catalog only during open, save and delete. */
    if (s_view.state == REC_BOOT || s_view.state == REC_SAVING || s_view.state == REC_DELETING ||
        index >= s_view.count) return (voice_clip_t){0};
    return s_store.clips[index];
}

void recorder_discard(void)
{
    if (s_view.state != REC_UNSAVED) return;
    s_view.elapsed_ms = 0;
    set_state(REC_IDLE, "Unsaved audio discarded");
}
