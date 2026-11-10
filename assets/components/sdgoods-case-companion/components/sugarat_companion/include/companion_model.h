/* SPDX-License-Identifier: Apache-2.0 */
/** Local companion interactions and explicit voice-demo timing; no audio or network access. */
#ifndef SUGARAT_COMPANION_MODEL_H
#define SUGARAT_COMPANION_MODEL_H
#include "sp_chars.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum { SC_VOICE_IDLE, SC_VOICE_LISTENING, SC_VOICE_THINKING, SC_VOICE_SPEAKING, SC_VOICE_OFFLINE } sc_voice_t;
typedef struct { uint32_t reaction_ms, thinking_ms, speaking_ms; } sc_timing_t;
typedef struct {
    sc_timing_t timing;
    uint64_t reaction_until, voice_until;
    uint32_t interactions;
    sp_mood_t pet_mood, selected_mood;
    sc_voice_t voice;
    bool sleeping, animated;
} sc_model_t;

/** Initialize a local idle character with explicit demonstration durations in milliseconds. */
void sc_model_init(sc_model_t *model, sc_timing_t timing);
/** Apply elapsed reaction/demo deadlines; return whether visible state changed. */
bool sc_model_tick(sc_model_t *model, uint64_t now_ms);
/** Wake and react to a touch or play request at the supplied monotonic time. */
void sc_model_interact(sc_model_t *model, uint64_t now_ms, bool play);
/** Toggle sleep, clearing any temporary pet reaction. */
void sc_model_sleep(sc_model_t *model);
/** Start listening unless offline; return whether a voice request started. */
bool sc_model_voice_start(sc_model_t *model);
/** End listening; demo mode schedules local transitions, external mode awaits provider state. */
void sc_model_voice_end(sc_model_t *model, uint64_t now_ms, bool demo);
/** Set provider or offline state and cancel every pending demo transition. */
void sc_model_voice_set(sc_model_t *model, sc_voice_t state);
#endif
