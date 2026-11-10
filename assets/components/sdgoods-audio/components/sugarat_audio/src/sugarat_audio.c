/*
 * Sugarat audio component: tones, PCM/WAV playback, recording, microphone level and streaming.
 * SPDX-License-Identifier: Apache-2.0
 *
 * I2S pin, slot and clock settings follow the SDGOODS BSP (sdgoods_audio.c,
 * Copyright (c) 2026 SDGOODS, Apache-2.0), which documents the board's working setup.
 */
#include "sugarat_audio.h"

#include <math.h>
#include <string.h>

#include "board_pins.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include "sdgoods_audio.h"

static const char *TAG = "sugarat_audio";

#define RATE              SUGARAT_AUDIO_SAMPLE_RATE
#define BLOCK             256              /* frames per I2S transfer, 16 ms */
#define MIC_WARMUP_MS     200
#define PREFILL_MS        80               /* silence before enabling the amplifier */
#define PA_SETTLE_MS      50
#define TAIL_MS           60
#define TONE_AMPLITUDE    0.6f
#define TONE_EDGE_MS      8
#define IO_TIMEOUT_MS     500
#define LEVEL_FLOOR_DB    (-60.0f)
#define PEAK_DECAY        2                /* percent per block */
#define TARGET_RMS        3500             /* normalization target, matches the BSP */
#define MAX_GAIN_Q10      (4 << 10)
#define TWO_PI            6.28318531f
#define STREAM_PA_IDLE_MS 800              /* amplifier off after this long without queued audio */
#define STREAM_ACTIVE_US  120000
#define STREAM_PREBUFFER_MS 200            /* queued audio before a reply starts playing */
#define STREAM_REBUFFER_MS  120            /* queued audio before playback resumes after running dry */
#define STREAM_START_WAIT_MS 250           /* a short reply that never fills the prebuffer starts anyway */
#define STREAM_NEW_REPLY_MS 600            /* silence after which the next audio counts as a new reply */
#define STREAM_FADE         64             /* samples faded at a gap, 4 ms */
#define OUT_FLOOR_DB      (-50.0f)

typedef enum {
    CMD_TONE = 0,
    CMD_SWEEP,
    CMD_PCM,
    CMD_RECORD,
    CMD_MONITOR,
    CMD_STREAM,
} cmd_kind_t;

typedef struct {
    cmd_kind_t kind;
    uint16_t freq_a;
    uint16_t freq_b;
    uint32_t duration_ms;
    const int16_t *pcm;
    size_t frames;
    uint8_t channels;
} audio_cmd_t;

static sugarat_audio_config_t s_cfg;
static sugarat_audio_status_t s_status;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static QueueHandle_t s_queue;
static TaskHandle_t s_task;
static i2s_chan_handle_t s_tx;
static i2s_chan_handle_t s_rx;
static volatile bool s_abort;
static bool s_ready;
static int16_t *s_rec;
static size_t s_rec_cap;
static size_t s_rec_n;
static int32_t s_mic_raw[BLOCK];
static int16_t s_mono[BLOCK];
static int16_t s_stereo[BLOCK * 2];

/* Streaming: the audio task plays the queue, the microphone task delivers capture. */
static StreamBufferHandle_t s_play_queue;
static sugarat_audio_stream_config_t s_stream_cfg;
static volatile bool s_flush;
static volatile bool s_mic_run;
static TaskHandle_t s_mic_task;
static TaskHandle_t s_mic_waiter;
static int16_t s_play[BLOCK];
static int32_t s_gain = -1; /* volume percent applied to the previous speaker block; -1 before the first */

static inline int16_t sat16(int32_t v)
{
    return v > 32767 ? 32767 : v < -32768 ? -32768 : (int16_t)v;
}

static bool interrupted(void)
{
    return s_abort || uxQueueMessagesWaiting(s_queue) > 0;
}

static void set_state(sugarat_audio_state_t state, uint32_t progress_ms)
{
    portENTER_CRITICAL(&s_lock);
    s_status.state = state;
    s_status.progress_ms = progress_ms;
    portEXIT_CRITICAL(&s_lock);
}

static void set_progress(uint32_t progress_ms)
{
    portENTER_CRITICAL(&s_lock);
    s_status.progress_ms = progress_ms;
    portEXIT_CRITICAL(&s_lock);
}

static void emit(sugarat_audio_event_type_t type, sugarat_audio_state_t op, esp_err_t err, uint32_t ms)
{
    if (err != ESP_OK) {
        portENTER_CRITICAL(&s_lock);
        s_status.last_error = err;
        portEXIT_CRITICAL(&s_lock);
    }
    if (s_cfg.on_event) {
        const sugarat_audio_event_t ev = { .type = type, .operation = op, .error = err, .duration_ms = ms };
        s_cfg.on_event(&ev, s_cfg.user_data);
    }
}

/* ---- speaker ---------------------------------------------------------------------------- */

static void pa_set(bool on)
{
    gpio_set_level(BOARD_AUDIO_PA_EN_GPIO, on ? BOARD_AUDIO_PA_EN_ACTIVE_LEVEL : !BOARD_AUDIO_PA_EN_ACTIVE_LEVEL);
}

static uint8_t current_volume(void)
{
    int v = s_cfg.follow_system_volume ? sdgoods_audio_get_volume() : s_cfg.volume_percent;
    v = v < 0 ? 0 : v > 100 ? 100 : v;
    portENTER_CRITICAL(&s_lock);
    s_status.volume_percent = (uint8_t)v;
    portEXIT_CRITICAL(&s_lock);
    return (uint8_t)v;
}

static esp_err_t spk_write_frames(const int16_t *stereo, size_t frames)
{
    size_t written = 0;
    return i2s_channel_write(s_tx, stereo, frames * 2 * sizeof(int16_t), &written, pdMS_TO_TICKS(IO_TIMEOUT_MS));
}

static esp_err_t spk_silence(uint32_t ms)
{
    memset(s_stereo, 0, sizeof(s_stereo));
    size_t left = (RATE * ms) / 1000u;
    while (left > 0) {
        const size_t n = left > BLOCK ? BLOCK : left;
        esp_err_t err = spk_write_frames(s_stereo, n);
        if (err != ESP_OK) {
            return err;
        }
        left -= n;
    }
    return ESP_OK;
}

/* Pop-free amplifier sequence, same order as the BSP: silence, PA on, settle. */
static esp_err_t spk_begin(void)
{
    esp_err_t err = i2s_channel_enable(s_tx);
    if (err != ESP_OK) {
        return err;
    }
    err = spk_silence(PREFILL_MS);
    if (err == ESP_OK) {
        pa_set(true);
        err = spk_silence(PA_SETTLE_MS);
    }
    return err;
}

static void spk_end(void)
{
    (void)spk_silence(TAIL_MS);
    pa_set(false);
    (void)spk_silence(20);
    (void)i2s_channel_disable(s_tx);
}

/* Scale one mono block by the volume and send it. A volume change ramps linearly across the
 * block, so dragging the volume slider does not step the waveform; volume 0 plays silence and
 * leaves the amplifier on. Callers switch the amplifier on before the first block. An unpatched
 * BSP volume setter switches it off while BSP music is idle, so each block switches it back on. */
static esp_err_t spk_play_block(const int16_t *mono, size_t n)
{
    const int32_t vol = current_volume();
    const int32_t from = s_gain < 0 ? vol : s_gain;
    for (size_t i = 0; i < n; i++) {
        const int32_t g = from + (vol - from) * (int32_t)(i + 1) / (int32_t)n;
        const int16_t s = (int16_t)(((int32_t)mono[i] * g) / 100);
        s_stereo[i * 2] = s;
        s_stereo[i * 2 + 1] = s;
    }
    s_gain = vol;
    pa_set(true);
    return spk_write_frames(s_stereo, n);
}

static void run_synth(const audio_cmd_t *cmd)
{
    const sugarat_audio_state_t op = SUGARAT_AUDIO_STATE_PLAYING;
    const size_t total = (size_t)RATE * cmd->duration_ms / 1000u;
    const size_t edge = (size_t)RATE * TONE_EDGE_MS / 1000u;
    set_state(op, 0);
    esp_err_t err = spk_begin();
    float phase = 0.0f;
    size_t pos = 0;
    while (err == ESP_OK && pos < total && !interrupted()) {
        const size_t n = total - pos > BLOCK ? BLOCK : total - pos;
        for (size_t i = 0; i < n; i++, pos++) {
            float f = cmd->freq_a;
            if (cmd->kind == CMD_SWEEP) {
                f = cmd->freq_a + ((float)cmd->freq_b - cmd->freq_a) * (float)pos / (float)total;
            }
            phase += TWO_PI * f / (float)RATE;
            if (phase > TWO_PI) {
                phase -= TWO_PI;
            }
            float env = 1.0f;
            if (pos < edge) {
                env = (float)pos / edge;
            } else if (total - pos < edge) {
                env = (float)(total - pos) / edge;
            }
            s_mono[i] = (int16_t)(sinf(phase) * env * TONE_AMPLITUDE * 32767.0f);
        }
        err = spk_play_block(s_mono, n);
        set_progress((uint32_t)(pos * 1000u / RATE));
    }
    spk_end();
    const uint32_t ms = (uint32_t)(pos * 1000u / RATE);
    set_state(SUGARAT_AUDIO_STATE_IDLE, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "speaker write failed: %s", esp_err_to_name(err));
        emit(SUGARAT_AUDIO_EVENT_ERROR, op, err, ms);
    } else {
        emit(pos < total ? SUGARAT_AUDIO_EVENT_STOPPED : SUGARAT_AUDIO_EVENT_PLAYBACK_DONE, op, ESP_OK, ms);
    }
}

static void run_pcm(const audio_cmd_t *cmd)
{
    const sugarat_audio_state_t op = SUGARAT_AUDIO_STATE_PLAYING;
    set_state(op, 0);
    esp_err_t err = spk_begin();
    size_t pos = 0;
    while (err == ESP_OK && pos < cmd->frames && !interrupted()) {
        const size_t n = cmd->frames - pos > BLOCK ? BLOCK : cmd->frames - pos;
        for (size_t i = 0; i < n; i++) {
            if (cmd->channels == 2) {
                const int32_t l = cmd->pcm[(pos + i) * 2];
                const int32_t r = cmd->pcm[(pos + i) * 2 + 1];
                s_mono[i] = (int16_t)((l + r) / 2);
            } else {
                s_mono[i] = cmd->pcm[pos + i];
            }
        }
        err = spk_play_block(s_mono, n);
        pos += n;
        set_progress((uint32_t)(pos * 1000u / RATE));
    }
    spk_end();
    const uint32_t ms = (uint32_t)(pos * 1000u / RATE);
    set_state(SUGARAT_AUDIO_STATE_IDLE, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "speaker write failed: %s", esp_err_to_name(err));
        emit(SUGARAT_AUDIO_EVENT_ERROR, op, err, ms);
    } else {
        emit(pos < cmd->frames ? SUGARAT_AUDIO_EVENT_STOPPED : SUGARAT_AUDIO_EVENT_PLAYBACK_DONE, op, ESP_OK, ms);
    }
}

/* ---- microphone ------------------------------------------------------------------------- */

static void update_level(const int16_t *pcm, size_t n)
{
    int64_t sum = 0;
    for (size_t i = 0; i < n; i++) {
        sum += (int32_t)pcm[i] * pcm[i];
    }
    const float rms = n ? sqrtf((float)sum / (float)n) : 0.0f;
    float db = rms > 0.5f ? 20.0f * log10f(rms / 32768.0f) : LEVEL_FLOOR_DB;
    db = db < LEVEL_FLOOR_DB ? LEVEL_FLOOR_DB : db;
    const uint8_t pct = (uint8_t)((db - LEVEL_FLOOR_DB) * 100.0f / -LEVEL_FLOOR_DB);
    portENTER_CRITICAL(&s_lock);
    s_status.level_dbfs = db;
    s_status.level_percent = pct;
    s_status.peak_percent = pct > s_status.peak_percent ? pct
        : (s_status.peak_percent > PEAK_DECAY ? s_status.peak_percent - PEAK_DECAY : 0);
    portEXIT_CRITICAL(&s_lock);
}

static void clear_level(void)
{
    portENTER_CRITICAL(&s_lock);
    s_status.level_percent = 0;
    s_status.peak_percent = 0;
    s_status.level_dbfs = LEVEL_FLOOR_DB;
    portEXIT_CRITICAL(&s_lock);
}

/* Read one block of microphone PCM; returns the number of samples converted. */
static esp_err_t mic_read_block(int16_t *out, size_t *n)
{
    size_t bytes = 0;
    esp_err_t err = i2s_channel_read(s_rx, s_mic_raw, sizeof(s_mic_raw), &bytes, pdMS_TO_TICKS(IO_TIMEOUT_MS));
    *n = bytes / sizeof(int32_t);
    for (size_t i = 0; i < *n; i++) {
        out[i] = sat16(s_mic_raw[i] >> s_cfg.mic_shift);
    }
    return err;
}

static esp_err_t mic_warmup(void)
{
    size_t left = (RATE * MIC_WARMUP_MS) / 1000u;
    while (left > 0 && !interrupted()) {
        size_t n = 0;
        esp_err_t err = mic_read_block(s_mono, &n);
        if (err != ESP_OK) {
            return err;
        }
        left = n >= left ? 0 : left - n;
    }
    return ESP_OK;
}

/* DC removal, ~80 Hz high-pass, loudness normalization and short fades. */
static void process_recording(int16_t *pcm, size_t n)
{
    if (n < RATE / 10) {
        return;
    }
    int64_t sum = 0;
    for (size_t i = 0; i < n; i++) {
        sum += pcm[i];
    }
    const int32_t dc = (int32_t)(sum / (int64_t)n);
    int32_t x1 = pcm[0] - dc;
    int32_t y1 = 0;
    pcm[0] = 0;
    for (size_t i = 1; i < n; i++) {
        const int32_t x0 = pcm[i] - dc;
        const int32_t y0 = (31800 * (y1 + x0 - x1)) >> 15;
        pcm[i] = sat16(y0);
        x1 = x0;
        y1 = y0;
    }
    int64_t sq = 0;
    int32_t peak = 0;
    for (size_t i = 0; i < n; i++) {
        const int32_t a = pcm[i] < 0 ? -pcm[i] : pcm[i];
        peak = a > peak ? a : peak;
        sq += (int64_t)pcm[i] * pcm[i];
    }
    const int32_t rms = (int32_t)sqrtf((float)sq / (float)n);
    if (peak >= 40 && rms > 0) {
        int32_t gain = (TARGET_RMS << 10) / rms;
        gain = gain > MAX_GAIN_Q10 ? MAX_GAIN_Q10 : gain < 512 ? 512 : gain;
        const int32_t limit = (20000 << 10) / peak;
        gain = gain > limit ? limit : gain;
        for (size_t i = 0; i < n; i++) {
            pcm[i] = sat16(((int32_t)pcm[i] * gain) >> 10);
        }
    }
    const size_t fade = RATE * 20u / 1000u;
    for (size_t i = 0; i < fade && i < n; i++) {
        pcm[i] = (int16_t)((int32_t)pcm[i] * (int32_t)i / (int32_t)fade);
        pcm[n - 1 - i] = (int16_t)((int32_t)pcm[n - 1 - i] * (int32_t)i / (int32_t)fade);
    }
}

static void run_mic(const audio_cmd_t *cmd)
{
    const bool record = cmd->kind == CMD_RECORD;
    const sugarat_audio_state_t op = record ? SUGARAT_AUDIO_STATE_RECORDING : SUGARAT_AUDIO_STATE_MONITORING;
    const size_t target = record ? (size_t)RATE * cmd->duration_ms / 1000u : 0;
    set_state(op, 0);
    if (record) {
        s_rec_n = 0;
        portENTER_CRITICAL(&s_lock);
        s_status.recorded_ms = 0;
        portEXIT_CRITICAL(&s_lock);
    }
    esp_err_t err = i2s_channel_enable(s_rx);
    if (err == ESP_OK) {
        err = mic_warmup();
    }
    size_t got = 0;
    while (err == ESP_OK && !interrupted() && (!record || got < target)) {
        size_t n = 0;
        err = mic_read_block(s_mono, &n);
        if (err != ESP_OK) {
            break;
        }
        update_level(s_mono, n);
        if (record) {
            const size_t keep = target - got < n ? target - got : n;
            memcpy(s_rec + got, s_mono, keep * sizeof(int16_t));
            got += keep;
            set_progress((uint32_t)(got * 1000u / RATE));
        }
    }
    (void)i2s_channel_disable(s_rx);
    clear_level();
    if (record) {
        if (s_cfg.process_recording) {
            process_recording(s_rec, got);
        }
        s_rec_n = got;
        portENTER_CRITICAL(&s_lock);
        s_status.recorded_ms = (uint32_t)(got * 1000u / RATE);
        portEXIT_CRITICAL(&s_lock);
    }
    set_state(SUGARAT_AUDIO_STATE_IDLE, 0);
    const uint32_t ms = (uint32_t)(got * 1000u / RATE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "microphone read failed: %s", esp_err_to_name(err));
        emit(SUGARAT_AUDIO_EVENT_ERROR, op, err, ms);
    } else if (record && got >= target) {
        emit(SUGARAT_AUDIO_EVENT_RECORD_DONE, op, ESP_OK, ms);
    } else {
        emit(SUGARAT_AUDIO_EVENT_STOPPED, op, ESP_OK, ms);
    }
}

/* ---- streaming -------------------------------------------------------------------------- */

static void mic_task(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        esp_err_t err = ESP_OK;
        while (s_mic_run && err == ESP_OK) {
            size_t n = 0;
            err = mic_read_block(s_mono, &n);
            if (err == ESP_OK && n > 0) {
                update_level(s_mono, n);
                if (s_stream_cfg.on_mic) {
                    s_stream_cfg.on_mic(s_mono, n, s_stream_cfg.user_data);
                }
            }
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "microphone read failed while streaming: %s", esp_err_to_name(err));
            portENTER_CRITICAL(&s_lock);
            s_status.last_error = err;
            portEXIT_CRITICAL(&s_lock);
        }
        TaskHandle_t waiter = s_mic_waiter;
        if (waiter) {
            xTaskNotifyGive(waiter);
        }
    }
}

static void set_out_level(const int16_t *pcm, size_t n, bool active)
{
    uint8_t pct = 0;
    if (n > 0) {
        int64_t sum = 0;
        for (size_t i = 0; i < n; i++) {
            sum += (int32_t)pcm[i] * pcm[i];
        }
        const float rms = sqrtf((float)sum / (float)n);
        float db = rms > 0.5f ? 20.0f * log10f(rms / 32768.0f) : OUT_FLOOR_DB;
        db = db < OUT_FLOOR_DB ? OUT_FLOOR_DB : db > 0.0f ? 0.0f : db;
        pct = (uint8_t)((db - OUT_FLOOR_DB) * 100.0f / -OUT_FLOOR_DB);
    }
    portENTER_CRITICAL(&s_lock);
    s_status.out_level_percent = pct;
    s_status.out_active = active;
    portEXIT_CRITICAL(&s_lock);
}

static void run_stream(void)
{
    const sugarat_audio_state_t op = SUGARAT_AUDIO_STATE_STREAMING;
    set_state(op, 0);
    esp_err_t err = i2s_channel_enable(s_rx);
    if (err == ESP_OK) {
        err = mic_warmup();
    }
    if (err == ESP_OK) {
        s_mic_waiter = NULL;
        s_mic_run = true;
        xTaskNotifyGive(s_mic_task);
        err = i2s_channel_enable(s_tx);
    }
    /* Jitter buffer: network audio arrives in bursts, so playback starts only once enough is
     * queued and, when the queue runs dry, fades out and refills before resuming. Without it
     * every late packet becomes a 16 ms hole, which sounds like crackling. */
    bool pa_on = false;
    bool playing = false;
    bool fade_in = true;
    size_t threshold = (size_t)RATE * STREAM_PREBUFFER_MS / 1000u;
    int64_t last_audio_us = 0;
    int64_t wait_since_us = 0;
    unsigned underruns = 0;
    uint32_t played_blocks = 0;
    const int64_t started_us = esp_timer_get_time();
    while (err == ESP_OK && !interrupted()) {
        if (s_flush) {
            /* Drain instead of xStreamBufferReset(): the writer may be sending on the other core. */
            while (xStreamBufferReceive(s_play_queue, s_play, sizeof(s_play), 0) > 0) {
            }
            s_flush = false;
            playing = false;
            fade_in = true;
            wait_since_us = 0;
            threshold = (size_t)RATE * STREAM_PREBUFFER_MS / 1000u;
        }
        const int64_t now = esp_timer_get_time();
        const size_t queued = xStreamBufferBytesAvailable(s_play_queue) / sizeof(int16_t);
        if (!playing && queued > 0) {
            if (wait_since_us == 0) {
                wait_since_us = now;
            }
            if (queued >= threshold || now - wait_since_us > (int64_t)STREAM_START_WAIT_MS * 1000) {
                if (threshold < (size_t)RATE * STREAM_PREBUFFER_MS / 1000u) {
                    underruns++; /* resumed after running dry inside a reply */
                }
                playing = true;
                wait_since_us = 0;
            }
        }
        size_t n = 0;
        if (playing) {
            n = xStreamBufferReceive(s_play_queue, s_play, sizeof(s_play), 0) / sizeof(int16_t);
        }
        if (n > 0) {
            if (!pa_on) {
                /* The stream has been sending silence; switch the amplifier on and let it settle. */
                pa_set(true);
                pa_on = true;
                err = spk_silence(PA_SETTLE_MS);
            }
            if (fade_in) {
                const size_t k = n < STREAM_FADE ? n : STREAM_FADE;
                for (size_t i = 0; i < k; i++) {
                    s_play[i] = (int16_t)((int32_t)s_play[i] * (int32_t)i / (int32_t)k);
                }
                fade_in = false;
            }
            if (n < BLOCK) {
                /* Ran dry: fade the tail and pad the block with silence. */
                const size_t k = n < STREAM_FADE ? n : STREAM_FADE;
                for (size_t i = 0; i < k; i++) {
                    const size_t at = n - k + i;
                    s_play[at] = (int16_t)((int32_t)s_play[at] * (int32_t)(k - i) / (int32_t)k);
                }
                memset(s_play + n, 0, (BLOCK - n) * sizeof(int16_t));
                playing = false;
                fade_in = true;
                threshold = (size_t)RATE * STREAM_REBUFFER_MS / 1000u;
            }
            if (err == ESP_OK) {
                err = spk_play_block(s_play, BLOCK);
            }
            played_blocks++;
            last_audio_us = now;
            set_out_level(s_play, n, true);
        } else {
            memset(s_stereo, 0, sizeof(s_stereo));
            err = spk_write_frames(s_stereo, BLOCK);
            if (playing) {
                playing = false; /* the queue ended exactly on a block boundary */
                fade_in = true;
                threshold = (size_t)RATE * STREAM_REBUFFER_MS / 1000u;
            }
            if (last_audio_us != 0 && now - last_audio_us > (int64_t)STREAM_NEW_REPLY_MS * 1000) {
                threshold = (size_t)RATE * STREAM_PREBUFFER_MS / 1000u;
            }
            if (pa_on && now - last_audio_us > (int64_t)STREAM_PA_IDLE_MS * 1000) {
                pa_set(false);
                pa_on = false;
                ESP_LOGI(TAG, "stream reply: %u ms played, %u underruns", (unsigned)(played_blocks * BLOCK * 1000u / RATE),
                         underruns);
                played_blocks = 0;
                underruns = 0;
            }
            set_out_level(NULL, 0, now - last_audio_us < STREAM_ACTIVE_US && last_audio_us != 0);
        }
        set_progress((uint32_t)((now - started_us) / 1000));
    }
    /* Stop the microphone task before releasing its channel. */
    if (s_mic_run) {
        s_mic_waiter = xTaskGetCurrentTaskHandle();
        s_mic_run = false;
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(IO_TIMEOUT_MS * 2));
        s_mic_waiter = NULL;
    }
    (void)i2s_channel_disable(s_rx);
    clear_level();
    set_out_level(NULL, 0, false);
    if (pa_on || err == ESP_OK) {
        spk_end();
    } else {
        (void)i2s_channel_disable(s_tx);
    }
    set_state(SUGARAT_AUDIO_STATE_IDLE, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "streaming failed: %s", esp_err_to_name(err));
        emit(SUGARAT_AUDIO_EVENT_ERROR, op, err, 0);
    } else {
        emit(SUGARAT_AUDIO_EVENT_STOPPED, op, ESP_OK, 0);
    }
}

static void audio_task(void *arg)
{
    (void)arg;
    audio_cmd_t cmd;
    for (;;) {
        if (xQueueReceive(s_queue, &cmd, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        s_abort = false;
        switch (cmd.kind) {
        case CMD_TONE:
        case CMD_SWEEP:
            run_synth(&cmd);
            break;
        case CMD_PCM:
            run_pcm(&cmd);
            break;
        case CMD_RECORD:
        case CMD_MONITOR:
            run_mic(&cmd);
            break;
        case CMD_STREAM:
            run_stream();
            break;
        }
    }
}

/* ---- setup ------------------------------------------------------------------------------ */

static esp_err_t create_channels(void)
{
    i2s_chan_config_t tx_chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    tx_chan.auto_clear = true;
    tx_chan.dma_desc_num = 8;   /* 128 ms of DMA, so a busy CPU does not starve the speaker */
    tx_chan.dma_frame_num = 256;
    esp_err_t err = i2s_new_channel(&tx_chan, &s_tx, NULL);
    if (err != ESP_OK) {
        return err;
    }
    i2s_std_config_t tx_std = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = BOARD_SPK_I2S_GPIO_MCLK, .bclk = BOARD_SPK_I2S_GPIO_BCLK, .ws = BOARD_SPK_I2S_GPIO_WS,
            .dout = BOARD_SPK_I2S_GPIO_DOUT, .din = GPIO_NUM_NC,
            .invert_flags = { false, false, false },
        },
    };
    tx_std.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
    err = i2s_channel_init_std_mode(s_tx, &tx_std);
    if (err != ESP_OK) {
        return err;
    }

    i2s_chan_config_t rx_chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    rx_chan.auto_clear = true;
    rx_chan.dma_desc_num = 8;
    rx_chan.dma_frame_num = 256;
    err = i2s_new_channel(&rx_chan, NULL, &s_rx);
    if (err != ESP_OK) {
        return err;
    }
    i2s_std_config_t rx_std = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = GPIO_NUM_NC, .bclk = BOARD_MIC_I2S_GPIO_BCLK, .ws = BOARD_MIC_I2S_GPIO_WS,
            .dout = GPIO_NUM_NC, .din = BOARD_MIC_I2S_GPIO_DIN,
            .invert_flags = { false, false, false },
        },
    };
    rx_std.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    return i2s_channel_init_std_mode(s_rx, &rx_std);
}

static void release_channels(void)
{
    if (s_tx) {
        i2s_del_channel(s_tx);
        s_tx = NULL;
    }
    if (s_rx) {
        i2s_del_channel(s_rx);
        s_rx = NULL;
    }
}

sugarat_audio_config_t sugarat_audio_default_config(void)
{
    return (sugarat_audio_config_t){
        .record_max_ms = 5000,
        .mic_shift = 14,
        .process_recording = true,
        .follow_system_volume = true,
        .volume_percent = 60,
        .on_event = NULL,
        .user_data = NULL,
    };
}

esp_err_t sugarat_audio_init(const sugarat_audio_config_t *config)
{
    if (s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    const sugarat_audio_config_t cfg = config ? *config : sugarat_audio_default_config();
    if (cfg.record_max_ms < 1000 || cfg.record_max_ms > 30000 || cfg.mic_shift > 24 || cfg.volume_percent > 100) {
        return ESP_ERR_INVALID_ARG;
    }
    s_cfg = cfg;
    const gpio_config_t pa = {
        .pin_bit_mask = 1ULL << BOARD_AUDIO_PA_EN_GPIO,
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&pa);
    if (err != ESP_OK) {
        return err;
    }
    pa_set(false);
    err = create_channels();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S ports unavailable (%s); sdgoods_audio_init() must not run in the same firmware",
                 esp_err_to_name(err));
        release_channels();
        return err;
    }
    s_rec_cap = (size_t)RATE * cfg.record_max_ms / 1000u;
    s_rec = heap_caps_malloc(s_rec_cap * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_queue = xQueueCreate(1, sizeof(audio_cmd_t));
    if (!s_rec || !s_queue || xTaskCreate(audio_task, "sugarat_audio", 4096, NULL, 5, &s_task) != pdPASS ||
        xTaskCreate(mic_task, "sugarat_mic", 3072, NULL, 6, &s_mic_task) != pdPASS) {
        heap_caps_free(s_rec);
        s_rec = NULL;
        if (s_queue) {
            vQueueDelete(s_queue);
            s_queue = NULL;
        }
        s_task = NULL;
        release_channels();
        return ESP_ERR_NO_MEM;
    }
    portENTER_CRITICAL(&s_lock);
    memset(&s_status, 0, sizeof(s_status));
    s_status.ready = true;
    s_status.level_dbfs = LEVEL_FLOOR_DB;
    s_status.last_error = ESP_OK;
    portEXIT_CRITICAL(&s_lock);
    s_ready = true;
    ESP_LOGI(TAG, "ready: 16 kHz, recording buffer %lu ms in PSRAM", (unsigned long)cfg.record_max_ms);
    return ESP_OK;
}

static esp_err_t submit(const audio_cmd_t *cmd)
{
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    s_abort = true; /* the running operation stops at its next block */
    xQueueOverwrite(s_queue, cmd);
    return ESP_OK;
}

static bool freq_ok(uint16_t f)
{
    return f >= 50 && f <= 7000;
}

esp_err_t sugarat_audio_tone(uint16_t freq_hz, uint16_t duration_ms)
{
    if (!freq_ok(freq_hz) || duration_ms < 10 || duration_ms > 10000) {
        return ESP_ERR_INVALID_ARG;
    }
    const audio_cmd_t cmd = { .kind = CMD_TONE, .freq_a = freq_hz, .duration_ms = duration_ms };
    return submit(&cmd);
}

esp_err_t sugarat_audio_sweep(uint16_t from_hz, uint16_t to_hz, uint16_t duration_ms)
{
    if (!freq_ok(from_hz) || !freq_ok(to_hz) || duration_ms < 10 || duration_ms > 10000) {
        return ESP_ERR_INVALID_ARG;
    }
    const audio_cmd_t cmd = { .kind = CMD_SWEEP, .freq_a = from_hz, .freq_b = to_hz, .duration_ms = duration_ms };
    return submit(&cmd);
}

esp_err_t sugarat_audio_play_pcm(const int16_t *samples, size_t count)
{
    if (!samples || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    const audio_cmd_t cmd = { .kind = CMD_PCM, .pcm = samples, .frames = count, .channels = 1 };
    return submit(&cmd);
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | p[1] << 8);
}

esp_err_t sugarat_audio_play_wav(const uint8_t *data, size_t len)
{
    if (!data || len < 12 || memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    uint16_t format = 0, channels = 0, bits = 0;
    uint32_t rate = 0;
    const uint8_t *pcm = NULL;
    size_t pcm_bytes = 0;
    size_t off = 12;
    while (off + 8 <= len) {
        const uint32_t size = le32(data + off + 4);
        const uint8_t *body = data + off + 8;
        if (size > len - off - 8) {
            return ESP_ERR_INVALID_ARG; /* chunk runs past the buffer */
        }
        if (memcmp(data + off, "fmt ", 4) == 0 && size >= 16) {
            format = le16(body);
            channels = le16(body + 2);
            rate = le32(body + 4);
            bits = le16(body + 14);
        } else if (memcmp(data + off, "data", 4) == 0) {
            pcm = body;
            pcm_bytes = size;
            break;
        }
        off += 8 + size + (size & 1u);
    }
    if (!pcm || channels == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (format != 1 || bits != 16 || rate != RATE || channels > 2) {
        ESP_LOGW(TAG, "WAV not supported: format=%u bits=%u rate=%lu channels=%u (need PCM 16-bit 16 kHz)",
                 format, bits, (unsigned long)rate, channels);
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (((uintptr_t)pcm & 1u) != 0) {
        return ESP_ERR_INVALID_ARG; /* samples must be 16-bit aligned */
    }
    const size_t frames = pcm_bytes / (2u * channels);
    if (frames == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    const audio_cmd_t cmd = { .kind = CMD_PCM, .pcm = (const int16_t *)pcm, .frames = frames, .channels = (uint8_t)channels };
    return submit(&cmd);
}

esp_err_t sugarat_audio_record(uint32_t duration_ms)
{
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (duration_ms < 100 || duration_ms > s_cfg.record_max_ms) {
        return ESP_ERR_INVALID_ARG;
    }
    const audio_cmd_t cmd = { .kind = CMD_RECORD, .duration_ms = duration_ms };
    return submit(&cmd);
}

esp_err_t sugarat_audio_play_recording(void)
{
    if (!s_ready || s_rec_n == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    return sugarat_audio_play_pcm(s_rec, s_rec_n);
}

esp_err_t sugarat_audio_monitor(void)
{
    const audio_cmd_t cmd = { .kind = CMD_MONITOR };
    return submit(&cmd);
}

esp_err_t sugarat_audio_stream_start(const sugarat_audio_stream_config_t *config)
{
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!config || config->playback_buffer_ms < 200 || config->playback_buffer_ms > 10000) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_play_queue) {
        const size_t bytes = (size_t)RATE * config->playback_buffer_ms / 1000u * sizeof(int16_t);
        s_play_queue = xStreamBufferCreateWithCaps(bytes, sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_play_queue) {
            return ESP_ERR_NO_MEM;
        }
    }
    /* Wait for the current operation to finish, so its microphone task no longer reads the
       previous callback when the new configuration is installed. Must not run on the audio task. */
    sugarat_audio_stop();
    for (int waited = 0; waited < 2 * IO_TIMEOUT_MS; waited += 5) {
        portENTER_CRITICAL(&s_lock);
        const bool idle = s_status.state == SUGARAT_AUDIO_STATE_IDLE;
        portEXIT_CRITICAL(&s_lock);
        if (idle) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    s_stream_cfg = *config;
    s_flush = true;
    const audio_cmd_t cmd = { .kind = CMD_STREAM };
    return submit(&cmd);
}

size_t sugarat_audio_stream_write(const int16_t *samples, size_t count)
{
    if (!s_play_queue || !samples || count == 0) {
        return 0;
    }
    /* Write whole samples only. A buffer created with xStreamBufferCreateWithCaps() holds one
     * byte less than requested, so a full queue would otherwise accept an odd byte count and
     * shift every later sample by one byte, which plays as full-scale noise. One task writes,
     * so the space cannot shrink before the send. */
    const size_t room = xStreamBufferSpacesAvailable(s_play_queue) / sizeof(int16_t);
    if (count > room) {
        count = room;
    }
    return count == 0 ? 0 : xStreamBufferSend(s_play_queue, samples, count * sizeof(int16_t), 0) / sizeof(int16_t);
}

size_t sugarat_audio_stream_queued(void)
{
    return s_play_queue ? xStreamBufferBytesAvailable(s_play_queue) / sizeof(int16_t) : 0;
}

void sugarat_audio_stream_flush(void)
{
    s_flush = true;
}

void sugarat_audio_stop(void)
{
    if (!s_ready) {
        return;
    }
    xQueueReset(s_queue);
    s_abort = true;
}

bool sugarat_audio_get_status(sugarat_audio_status_t *out)
{
    if (!out) {
        return false;
    }
    portENTER_CRITICAL(&s_lock);
    *out = s_status;
    portEXIT_CRITICAL(&s_lock);
    if (s_ready) {
        out->volume_percent = current_volume(); /* live value, also when idle */
    }
    return true;
}

const int16_t *sugarat_audio_get_recording(size_t *count)
{
    if (count) {
        *count = s_rec_n;
    }
    return s_rec_n ? s_rec : NULL;
}

size_t sugarat_audio_write_wav_header(uint8_t *out, size_t samples, uint16_t channels)
{
    if (!out || channels == 0 || channels > 2) {
        return 0;
    }
    const uint32_t data_bytes = (uint32_t)(samples * channels * 2u);
    const uint32_t byte_rate = RATE * channels * 2u;
    const uint16_t align = (uint16_t)(channels * 2u);
    memcpy(out, "RIFF", 4);
    const uint32_t riff = 36u + data_bytes;
    const uint32_t fmt_size = 16, rate = RATE;
    const uint16_t pcm = 1, bits = 16;
    memcpy(out + 4, &riff, 4);
    memcpy(out + 8, "WAVEfmt ", 8);
    memcpy(out + 16, &fmt_size, 4);
    memcpy(out + 20, &pcm, 2);
    memcpy(out + 22, &channels, 2);
    memcpy(out + 24, &rate, 4);
    memcpy(out + 28, &byte_rate, 4);
    memcpy(out + 32, &align, 2);
    memcpy(out + 34, &bits, 2);
    memcpy(out + 36, "data", 4);
    memcpy(out + 40, &data_bytes, 4);
    return SUGARAT_AUDIO_WAV_HEADER_BYTES;
}

const char *sugarat_audio_state_name(sugarat_audio_state_t state)
{
    switch (state) {
    case SUGARAT_AUDIO_STATE_IDLE: return "Idle";
    case SUGARAT_AUDIO_STATE_PLAYING: return "Playing";
    case SUGARAT_AUDIO_STATE_RECORDING: return "Recording";
    case SUGARAT_AUDIO_STATE_MONITORING: return "Listening";
    case SUGARAT_AUDIO_STATE_STREAMING: return "Streaming";
    }
    return "Idle";
}
