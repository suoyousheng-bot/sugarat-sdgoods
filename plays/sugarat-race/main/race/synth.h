/* SPDX-License-Identifier: Apache-2.0 */
/** Portable chip-style sound synthesis: continuous engine and rolling layers plus one-shot effects. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/** Highest absolute sample before volume; the badge speaker crackles well below full scale. */
#define RACE_SYNTH_PEAK 9000
#define RACE_SYNTH_VOICES 6
typedef enum {
    RACE_SFX_TICK,        /* menu tap */
    RACE_SFX_COUNT,       /* countdown 3, 2, 1 */
    RACE_SFX_GO,
    RACE_SFX_BOOST,
    RACE_SFX_BUMP,        /* strength: closing speed / 15 m/s */
    RACE_SFX_OVERTAKE,
    RACE_SFX_LAP,
    RACE_SFX_FINAL_LAP,
    RACE_SFX_WALL,        /* strength: impact speed / 250 px/s */
    RACE_SFX_CHECKPOINT,
    RACE_SFX_WIN,
    RACE_SFX_FINISH,
    RACE_SFX_LOSE,        /* nearby peer reached the finish first */
} race_sfx;
/** Continuous layers; values are targets the synthesizer slews towards. */
typedef struct {
    bool engine;      /* racing car audible */
    float engine_speed; /* car speed / cruise speed, 0..1.5 */
    bool boost, offroad;
    float roll;       /* marble speed / 300 px/s, 0..1.3; 0 is silent */
} race_synth_params;
typedef struct {
    const uint16_t (*notes)[2]; /* frequency Hz, duration ms; NULL for a single sweep */
    int note_count, note;
    uint32_t pos, len;
    float f0, f1, tone, noise, decay, lowpass, phase, env, filtered;
    uint8_t wave;
} race_voice;
typedef struct {
    unsigned rate;
    uint32_t noise;
    race_voice voices[RACE_SYNTH_VOICES];
    float engine_phase, engine_phase2, engine_freq, engine_amp, hiss_amp, rumble_amp, rumble_phase;
    float roll_amp, roll_lp, roll_filtered, roll_phase;
} race_synth;
/**
 * Reset all layers.
 * @param synth State owned by the caller.
 * @param rate Output sample rate in Hz.
 */
void race_synth_init(race_synth *synth,unsigned rate);
/**
 * Start a one-shot effect, replacing the oldest voice when all are busy.
 * @param synth Initialized state.
 * @param sfx Effect.
 * @param strength 0..1 scale for impact effects; ignored by the others.
 */
void race_synth_trigger(race_synth *synth,race_sfx sfx,float strength);
/**
 * Render mono samples whose magnitude never exceeds RACE_SYNTH_PEAK.
 * @param synth Initialized state.
 * @param params Continuous layer targets.
 * @param out Destination.
 * @param count Number of samples.
 */
void race_synth_render(race_synth *synth,const race_synth_params *params,int16_t *out,size_t count);
