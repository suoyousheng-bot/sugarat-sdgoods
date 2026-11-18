/* SPDX-License-Identifier: Apache-2.0 */
/* Offline speech cleanup for saved recordings: Wiener noise suppression, then leveling and a
 * look-ahead peak limit. Everything runs on the file worker after capture, not in real time. */

#include "voice_enhance.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#define work_alloc(bytes) heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#else
#define work_alloc(bytes) malloc(bytes)
#endif

#define FFT_N        256                 /* 16 ms analysis window at 16 kHz */
#define HOP          (FFT_N / 2)
#define BINS         (FFT_N / 2 + 1)
#define NOISE_SHARE  0.15f               /* quietest share of frames that defines the noise */
#define OVER_SUBTRACT 1.5f
#define DD_ALPHA     0.98f               /* decision-directed a-priori SNR smoothing */
#define GAIN_FLOOR   0.1f                /* -20 dB; deeper floors produce musical noise */
#define LEVEL_FRAME  256                 /* 16 ms frames for leveling */
#define LEVEL_SPAN   31                  /* frames on each side when measuring speech level */
#define SPEECH_OVER_NOISE 4.0f           /* frame RMS 12 dB above the residual floor is speech */
#define SPEECH_LEAD  3
#define SPEECH_HANG  8
#define TARGET_RMS   2200.0f
#define MIN_GAIN     0.5f
#define MAX_GAIN     12.0f
#define PAUSE_GAIN   0.35f               /* -9 dB between phrases */
#define PEAK_LIMIT   9000.0f             /* the speaker crackles near full scale */
#define FADE_SAMPLES 160

typedef struct {
    float re, im;
} cpx_t;

static float s_window[FFT_N];
static cpx_t s_twiddle[FFT_N / 2];
static uint8_t s_reverse[FFT_N];
static bool s_tables_ready;
static cpx_t s_spectrum[FFT_N];
static float s_noise[BINS], s_prev_gain[BINS], s_prev_snr[BINS];
static float s_overlap[HOP];

static void init_tables(void)
{
    if (s_tables_ready) return;
    const float pi = 3.14159265358979f;
    for (int i = 0; i < FFT_N; ++i) {
        /* Sine analysis and synthesis windows overlap-add to exactly 1 at 50% overlap. */
        s_window[i] = sinf(pi * ((float)i + 0.5f) / FFT_N);
        int r = 0;
        for (int bit = 1, j = i; bit < FFT_N; bit <<= 1, j >>= 1) r = (r << 1) | (j & 1);
        s_reverse[i] = (uint8_t)r;
    }
    for (int i = 0; i < FFT_N / 2; ++i) {
        s_twiddle[i] = (cpx_t){cosf(2.0f * pi * i / FFT_N), -sinf(2.0f * pi * i / FFT_N)};
    }
    s_tables_ready = true;
}

static void fft(cpx_t *x)
{
    for (int i = 0; i < FFT_N; ++i) {
        const int j = s_reverse[i];
        if (j > i) {
            const cpx_t t = x[i];
            x[i] = x[j];
            x[j] = t;
        }
    }
    for (int size = 2; size <= FFT_N; size <<= 1) {
        const int half = size / 2, step = FFT_N / size;
        for (int start = 0; start < FFT_N; start += size) {
            for (int k = 0; k < half; ++k) {
                const cpx_t w = s_twiddle[k * step];
                cpx_t *a = &x[start + k], *b = &x[start + k + half];
                const cpx_t t = {b->re * w.re - b->im * w.im, b->re * w.im + b->im * w.re};
                b->re = a->re - t.re;
                b->im = a->im - t.im;
                a->re += t.re;
                a->im += t.im;
            }
        }
    }
}

/* Windowed spectrum of the frame starting at sample `start`; samples outside the clip are zero. */
static void analyze(const int16_t *pcm, size_t samples, long start, float dc)
{
    for (int i = 0; i < FFT_N; ++i) {
        const long at = start + i;
        const float x = at >= 0 && (size_t)at < samples ? (float)pcm[at] - dc : 0.0f;
        s_spectrum[i] = (cpx_t){x * s_window[i], 0.0f};
    }
    fft(s_spectrum);
}

static int compare_float(const void *first, const void *second)
{
    const float a = *(const float *)first, b = *(const float *)second;
    return (a > b) - (a < b);
}

static float quantile(float *values, size_t count, float share)
{
    qsort(values, count, sizeof(*values), compare_float);
    size_t at = (size_t)((float)count * share);
    return values[at < count ? at : count - 1];
}

static float frame_energy(const int16_t *pcm, size_t start, float dc)
{
    float energy = 0.0f;
    for (int i = 0; i < FFT_N; ++i) {
        const float x = ((float)pcm[start + i] - dc) * s_window[i];
        energy += x * x;
    }
    return energy;
}

/* Estimate the noise spectrum from the quietest whole frames, then suppress it frame by frame.
 * Output trails the frame being read by one hop, so the clip is rewritten in place. */
static void suppress_noise(int16_t *pcm, size_t samples, float *scratch, size_t frames)
{
    int64_t sum = 0;
    for (size_t i = 0; i < samples; ++i) sum += pcm[i];
    const float dc = (float)sum / (float)samples;

    const size_t whole = (samples - FFT_N) / HOP + 1;
    for (size_t f = 0; f < whole; ++f) scratch[f] = frame_energy(pcm, f * HOP, dc);
    memcpy(scratch + frames, scratch, whole * sizeof(*scratch));
    const float threshold = quantile(scratch + frames, whole, NOISE_SHARE);

    memset(s_noise, 0, sizeof(s_noise));
    int used = 0;
    for (size_t f = 0; f < whole; ++f) {
        if (scratch[f] > threshold) continue;
        analyze(pcm, samples, (long)(f * HOP), dc);
        for (int b = 0; b < BINS; ++b) {
            s_noise[b] += s_spectrum[b].re * s_spectrum[b].re + s_spectrum[b].im * s_spectrum[b].im;
        }
        ++used;
    }
    for (int b = 0; b < BINS; ++b) {
        s_noise[b] = s_noise[b] / (float)used * OVER_SUBTRACT + 1e-3f;
        s_prev_gain[b] = 1.0f;
        s_prev_snr[b] = 1.0f;
    }

    memset(s_overlap, 0, sizeof(s_overlap));
    for (long start = -HOP; start < (long)samples; start += HOP) {
        analyze(pcm, samples, start, dc);
        for (int b = 0; b < BINS; ++b) {
            const float power = s_spectrum[b].re * s_spectrum[b].re + s_spectrum[b].im * s_spectrum[b].im;
            const float snr = power / s_noise[b];
            const float prior_raw = snr - 1.0f > 0.0f ? snr - 1.0f : 0.0f;
            float prior = DD_ALPHA * s_prev_gain[b] * s_prev_gain[b] * s_prev_snr[b] + (1.0f - DD_ALPHA) * prior_raw;
            prior = prior < 0.003f ? 0.003f : prior;
            float gain = prior / (1.0f + prior);
            gain = gain < GAIN_FLOOR ? GAIN_FLOOR : gain;
            s_prev_gain[b] = gain;
            s_prev_snr[b] = snr;
            /* Bins below 94 Hz carry DC, handling rumble and table knocks, not speech. */
            if (b < 2) gain = 0.0f;
            else if (b == 2) gain *= 0.5f;
            s_spectrum[b].re *= gain;
            s_spectrum[b].im *= gain;
            if (b > 0 && b < FFT_N / 2) {
                s_spectrum[FFT_N - b].re *= gain;
                s_spectrum[FFT_N - b].im *= gain;
            }
        }
        /* Inverse transform through the forward FFT: ifft(X) = conj(fft(conj(X))) / N. */
        for (int i = 0; i < FFT_N; ++i) s_spectrum[i].im = -s_spectrum[i].im;
        fft(s_spectrum);
        for (int i = 0; i < HOP; ++i) {
            const long at = start + i;
            const float out = s_overlap[i] + s_spectrum[i].re / FFT_N * s_window[i];
            if (at >= 0 && (size_t)at < samples) {
                pcm[at] = (int16_t)(out > 32767.0f ? 32767.0f : out < -32768.0f ? -32768.0f : out);
            }
            s_overlap[i] = s_spectrum[i + HOP].re / FFT_N * s_window[i + HOP];
        }
    }
}

/* Level speech frames toward TARGET_RMS, lower pauses, and cap every frame's gain by its peak.
 * Gains change linearly between frame boundaries, and each boundary takes the smaller of its two
 * frames' gains, so no sample is driven past PEAK_LIMIT. */
static void level_speech(int16_t *pcm, size_t samples, float *scratch)
{
    const size_t frames = (samples + LEVEL_FRAME - 1) / LEVEL_FRAME;
    float *rms = scratch, *peak = scratch + frames, *gain = scratch + 2 * frames;
    float *sorted = scratch + 3 * frames, *pause = scratch + 4 * frames;
    for (size_t k = 0; k < frames; ++k) {
        const size_t begin = k * LEVEL_FRAME, end = begin + LEVEL_FRAME < samples ? begin + LEVEL_FRAME : samples;
        float energy = 0.0f, top = 0.0f;
        for (size_t i = begin; i < end; ++i) {
            const float x = pcm[i];
            energy += x * x;
            top = fabsf(x) > top ? fabsf(x) : top;
        }
        rms[k] = sqrtf(energy / (float)(end - begin));
        peak[k] = top;
    }
    memcpy(sorted, rms, frames * sizeof(*rms));
    const float floor_rms = quantile(sorted, frames, 0.2f);
    const float threshold = fmaxf(floor_rms * SPEECH_OVER_NOISE, 8.0f);

    /* pause[k] holds 1 for speech frames (with lead-in and hangover) and PAUSE_GAIN elsewhere. */
    for (size_t k = 0; k < frames; ++k) pause[k] = PAUSE_GAIN;
    for (size_t k = 0; k < frames; ++k) {
        if (rms[k] <= threshold) continue;
        const size_t from = k > SPEECH_LEAD ? k - SPEECH_LEAD : 0;
        const size_t to = k + SPEECH_HANG < frames ? k + SPEECH_HANG : frames - 1;
        for (size_t j = from; j <= to; ++j) pause[j] = 1.0f;
    }
    for (size_t k = 1; k < frames; ++k) pause[k] = fmaxf(pause[k], pause[k - 1] - 0.13f);
    for (size_t k = frames - 1; k-- > 0;) pause[k] = fmaxf(pause[k], pause[k + 1] - 0.33f);

    /* Speech level: RMS over speech frames within LEVEL_SPAN frames; pauses keep the last gain. */
    double energy = 0.0;
    size_t count = 0;
    for (size_t k = 0; k < frames && k < LEVEL_SPAN; ++k) {
        if (rms[k] > threshold) energy += (double)rms[k] * rms[k], ++count;
    }
    float held = 1.0f;
    for (size_t k = 0; k < frames; ++k) {
        const size_t enter = k + LEVEL_SPAN;
        if (enter < frames && rms[enter] > threshold) energy += (double)rms[enter] * rms[enter], ++count;
        if (k > LEVEL_SPAN && rms[k - LEVEL_SPAN - 1] > threshold) {
            energy -= (double)rms[k - LEVEL_SPAN - 1] * rms[k - LEVEL_SPAN - 1];
            --count;
        }
        if (count) {
            const float level = sqrtf((float)(energy > 0.0 ? energy : 0.0) / (float)count);
            held = fminf(fmaxf(TARGET_RMS / level, MIN_GAIN), MAX_GAIN);
        }
        gain[k] = held * pause[k];
        if (peak[k] * gain[k] > PEAK_LIMIT) gain[k] = PEAK_LIMIT / peak[k];
    }

    for (size_t k = 0; k < frames; ++k) {
        const size_t begin = k * LEVEL_FRAME, end = begin + LEVEL_FRAME < samples ? begin + LEVEL_FRAME : samples;
        const float from = k ? fminf(gain[k - 1], gain[k]) : gain[k];
        const float to = k + 1 < frames ? fminf(gain[k], gain[k + 1]) : gain[k];
        for (size_t i = begin; i < end; ++i) {
            const float g = from + (to - from) * (float)(i - begin) / (float)LEVEL_FRAME;
            pcm[i] = (int16_t)lrintf((float)pcm[i] * g);
        }
    }
    for (size_t i = 0; i < FADE_SAMPLES && i < samples; ++i) {
        pcm[i] = (int16_t)((int32_t)pcm[i] * (int32_t)i / FADE_SAMPLES);
        pcm[samples - 1 - i] = (int16_t)((int32_t)pcm[samples - 1 - i] * (int32_t)i / FADE_SAMPLES);
    }
}

bool voice_enhance(int16_t *pcm, size_t samples)
{
    if (samples < 4000) return true;
    init_tables();
    const size_t stft_frames = samples / HOP + 2;
    const size_t level_frames = (samples + LEVEL_FRAME - 1) / LEVEL_FRAME;
    const size_t capacity = 2 * stft_frames > 5 * level_frames ? 2 * stft_frames : 5 * level_frames;
    float *scratch = work_alloc(capacity * sizeof(float));
    if (!scratch) return false;
    suppress_noise(pcm, samples, scratch, stft_frames);
    level_speech(pcm, samples, scratch);
    free(scratch);
    return true;
}
