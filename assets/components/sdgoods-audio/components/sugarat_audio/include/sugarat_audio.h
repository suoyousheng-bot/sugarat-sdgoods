/*
 * Sugarat audio component for the SDGOODS ESP32-S3 round badge.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Ownership: this component owns the speaker (I2S0 TX + PA enable GPIO) and the
 * microphone (I2S1 RX). It replaces sdgoods_audio_init(): an application calls
 * one of them, never both, because the ESP32-S3 has only these two I2S ports.
 * BSP calls that remain safe: sdgoods_audio_get_volume/set_volume (the control
 * center volume; playback follows it by default) and sdgoods_audio_bgm_stop
 * (a no-op without sdgoods_audio_init). sdgoods_audio_bgm_start, record and play
 * fail while this component owns the ports. The foundation BSP's set_volume switches
 * the PA off while BSP music is idle; playback switches it back on at the next block,
 * so a volume change during playback pops unless the BSP limits that call to its own
 * music (see README.md).
 *
 * All operations run on one audio task. A new request stops the current one.
 * Callbacks run on that task: never call LVGL from them.
 *
 * Format: 16 kHz, 16-bit PCM. Playback accepts mono or stereo (mixed to mono);
 * recordings are mono.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Fixed I2S sample rate matching the board's speaker and microphone setup. */
#define SUGARAT_AUDIO_SAMPLE_RATE 16000u
/** Size of the header written by sugarat_audio_write_wav_header(). */
#define SUGARAT_AUDIO_WAV_HEADER_BYTES 44u

/** What the audio task is doing. */
typedef enum {
    SUGARAT_AUDIO_STATE_IDLE = 0,
    SUGARAT_AUDIO_STATE_PLAYING,
    SUGARAT_AUDIO_STATE_RECORDING,
    SUGARAT_AUDIO_STATE_MONITORING, /* microphone level meter, no storage */
    SUGARAT_AUDIO_STATE_STREAMING,  /* continuous microphone capture plus queued playback */
} sugarat_audio_state_t;

/** Completion events. */
typedef enum {
    SUGARAT_AUDIO_EVENT_PLAYBACK_DONE = 0,
    SUGARAT_AUDIO_EVENT_RECORD_DONE,
    SUGARAT_AUDIO_EVENT_STOPPED, /* stopped by sugarat_audio_stop() or a newer request */
    SUGARAT_AUDIO_EVENT_ERROR,
} sugarat_audio_event_type_t;

/** Event payload. */
typedef struct {
    sugarat_audio_event_type_t type;
    sugarat_audio_state_t operation; /* the operation that finished */
    esp_err_t error;                 /* ESP_OK except for ERROR */
    uint32_t duration_ms;            /* audio played or recorded */
} sugarat_audio_event_t;

/** Event callback; runs on the audio task. */
typedef void (*sugarat_audio_event_cb_t)(const sugarat_audio_event_t *event, void *user_data);

/** Configuration; obtain defaults from sugarat_audio_default_config(). */
typedef struct {
    /** Recording buffer capacity in PSRAM (1000..30000 ms; 32 KB per second). */
    uint32_t record_max_ms;
    /** Right shift from 32-bit microphone slots to 16-bit PCM; 14 matches the BSP. */
    uint8_t mic_shift;
    /** Remove DC/rumble and normalize loudness after recording. */
    bool process_recording;
    /** Scale playback by sdgoods_audio_get_volume(); otherwise by volume_percent. */
    bool follow_system_volume;
    uint8_t volume_percent;
    sugarat_audio_event_cb_t on_event;
    void *user_data;
} sugarat_audio_config_t;

/** Snapshot of the audio task. */
typedef struct {
    bool ready;
    sugarat_audio_state_t state;
    uint8_t level_percent;   /* microphone RMS level mapped from -60..0 dBFS */
    uint8_t peak_percent;    /* decaying peak of level_percent */
    float level_dbfs;
    uint32_t recorded_ms;    /* length of the stored recording */
    uint32_t progress_ms;    /* position in the current operation */
    uint8_t volume_percent;  /* effective playback volume */
    esp_err_t last_error;
    uint8_t out_level_percent; /* streaming: RMS of the block last sent to the speaker, -50..0 dBFS */
    bool out_active;           /* streaming: queued audio reached the speaker within the last 120 ms */
} sugarat_audio_status_t;

/** Streaming microphone callback: 16 kHz mono samples, called on the microphone task every 16 ms. */
typedef void (*sugarat_audio_mic_cb_t)(const int16_t *samples, size_t count, void *user_data);

/** Streaming configuration. */
typedef struct {
    sugarat_audio_mic_cb_t on_mic; /* may be NULL for playback only */
    void *user_data;
    uint32_t playback_buffer_ms;   /* PSRAM playback queue, 200..10000 ms */
} sugarat_audio_stream_config_t;

/**
 * Default configuration: 5 s recording buffer, BSP microphone scaling, system volume.
 * @returns Configuration with no callback.
 */
sugarat_audio_config_t sugarat_audio_default_config(void);

/**
 * Take ownership of I2S0/I2S1 and the PA pin and start the audio task.
 * Call after LVGL and touch initialization (internal RAM budget), instead of sdgoods_audio_init().
 * @param config Configuration copied by the component; NULL uses defaults.
 * @returns ESP_OK; ESP_ERR_INVALID_STATE if already initialized; ESP_ERR_INVALID_ARG;
 *          ESP_ERR_NO_MEM; or the I2S driver error when another owner holds a port.
 */
esp_err_t sugarat_audio_init(const sugarat_audio_config_t *config);

/**
 * Play a sine tone.
 * @param freq_hz 50..7000 Hz.
 * @param duration_ms 10..10000 ms.
 * @returns ESP_OK when queued; ESP_ERR_INVALID_STATE before init; ESP_ERR_INVALID_ARG.
 */
esp_err_t sugarat_audio_tone(uint16_t freq_hz, uint16_t duration_ms);

/**
 * Play a linear frequency sweep, useful for checking speaker response.
 * @param from_hz Start frequency, 50..7000 Hz.
 * @param to_hz End frequency, 50..7000 Hz.
 * @param duration_ms 10..10000 ms.
 * @returns ESP_OK when queued; ESP_ERR_INVALID_STATE before init; ESP_ERR_INVALID_ARG.
 */
esp_err_t sugarat_audio_sweep(uint16_t from_hz, uint16_t to_hz, uint16_t duration_ms);

/**
 * Play 16 kHz 16-bit mono PCM. The caller keeps the buffer valid until a completion
 * event, a newer request, or sugarat_audio_stop() returns.
 * @param samples PCM samples.
 * @param count Number of samples.
 * @returns ESP_OK when queued; ESP_ERR_INVALID_STATE before init; ESP_ERR_INVALID_ARG.
 */
esp_err_t sugarat_audio_play_pcm(const int16_t *samples, size_t count);

/**
 * Validate and play an in-memory WAV file (PCM, 16-bit, 16 kHz, 1 or 2 channels).
 * The buffer lifetime rule of sugarat_audio_play_pcm() applies.
 * @param data WAV file bytes.
 * @param len Number of bytes.
 * @returns ESP_OK when queued; ESP_ERR_INVALID_STATE before init; ESP_ERR_INVALID_ARG
 *          for malformed files; ESP_ERR_NOT_SUPPORTED for other encodings or rates.
 */
esp_err_t sugarat_audio_play_wav(const uint8_t *data, size_t len);

/**
 * Record from the microphone into the internal buffer, replacing the previous recording.
 * @param duration_ms 100 ms up to record_max_ms.
 * @returns ESP_OK when queued; ESP_ERR_INVALID_STATE before init; ESP_ERR_INVALID_ARG.
 */
esp_err_t sugarat_audio_record(uint32_t duration_ms);

/**
 * Play the stored recording.
 * @returns ESP_OK when queued; ESP_ERR_INVALID_STATE before init or with no recording.
 */
esp_err_t sugarat_audio_play_recording(void);

/**
 * Run the microphone level meter until stopped or replaced by another request.
 * @returns ESP_OK when queued; ESP_ERR_INVALID_STATE before init.
 */
esp_err_t sugarat_audio_monitor(void);

/** Stop the current operation; the speaker is muted and released before the task idles. */
void sugarat_audio_stop(void);

/**
 * Start streaming: the microphone runs continuously and calls config->on_mic, while the speaker
 * plays whatever sugarat_audio_stream_write() queues and sends silence otherwise. Playback starts
 * once 200 ms are queued (or after 250 ms for a shorter burst); a queue that runs dry fades out and
 * resumes after 120 ms are queued again. The amplifier switches on when queued audio arrives and
 * off after 800 ms without audio.
 * Runs until sugarat_audio_stop() or another request. The config is copied; the playback queue
 * is allocated once, on the first call, and its size cannot change afterwards.
 * @param config Callbacks and queue size.
 * @returns ESP_OK when queued; ESP_ERR_INVALID_STATE before init; ESP_ERR_INVALID_ARG;
 *          ESP_ERR_NO_MEM when the queue cannot be allocated.
 */
esp_err_t sugarat_audio_stream_start(const sugarat_audio_stream_config_t *config);

/**
 * Queue 16 kHz mono samples for playback without blocking. Call from one task only.
 * @param samples PCM samples.
 * @param count Number of samples.
 * @returns Number of samples accepted: whole samples only, fewer when the queue is full, 0 before
 *          the first start. Write the remainder later.
 */
size_t sugarat_audio_stream_write(const int16_t *samples, size_t count);

/** Samples waiting in the playback queue. */
size_t sugarat_audio_stream_queued(void);

/** Discard queued playback; the audio task clears the queue at its next 16 ms block. */
void sugarat_audio_stream_flush(void);

/**
 * Copy the current status.
 * @param out Destination snapshot.
 * @returns false when out is NULL; otherwise true.
 */
bool sugarat_audio_get_status(sugarat_audio_status_t *out);

/**
 * Access the stored recording. The buffer is replaced by the next sugarat_audio_record().
 * @param count Receives the number of samples; may be NULL.
 * @returns Pointer to mono 16 kHz samples, or NULL when nothing is recorded.
 */
const int16_t *sugarat_audio_get_recording(size_t *count);

/**
 * Write a canonical 44-byte PCM WAV header.
 * @param out Destination of SUGARAT_AUDIO_WAV_HEADER_BYTES bytes.
 * @param samples Samples per channel that will follow the header.
 * @param channels 1 or 2.
 * @returns Bytes written (44), or 0 for invalid arguments.
 */
size_t sugarat_audio_write_wav_header(uint8_t *out, size_t samples, uint16_t channels);

/**
 * Short English name for a state, suitable for logs and ASCII UI.
 * @param state State value.
 * @returns Static string.
 */
const char *sugarat_audio_state_name(sugarat_audio_state_t state);

#ifdef __cplusplus
}
#endif
