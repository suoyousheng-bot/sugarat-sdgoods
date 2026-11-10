/* SPDX-License-Identifier: Apache-2.0 */
/** Deterministic companion reactions driven by the caller's monotonic clock. */
#include "companion_model.h"

void sc_model_init(sc_model_t *model, sc_timing_t timing)
{
    *model = (sc_model_t){.timing = timing, .animated = true};
}

bool sc_model_tick(sc_model_t *model, uint64_t now_ms)
{
    bool changed = false;
    if (model->reaction_until && now_ms >= model->reaction_until) {
        model->reaction_until = 0;
        model->pet_mood = SP_MOOD_IDLE;
        changed = true;
    }
    if (model->voice == SC_VOICE_THINKING && model->voice_until && now_ms >= model->voice_until) {
        model->voice = SC_VOICE_SPEAKING;
        model->voice_until += model->timing.speaking_ms;
        changed = true;
    }
    if (model->voice == SC_VOICE_SPEAKING && model->voice_until && now_ms >= model->voice_until) {
        model->voice = SC_VOICE_IDLE;
        model->voice_until = 0;
        changed = true;
    }
    return changed;
}

void sc_model_interact(sc_model_t *model, uint64_t now_ms, bool play)
{
    ++model->interactions;
    model->sleeping = false;
    model->pet_mood = play ? SP_MOOD_HAPPY : SP_MOOD_LOVE;
    model->reaction_until = now_ms + model->timing.reaction_ms;
}

void sc_model_sleep(sc_model_t *model)
{
    model->sleeping = !model->sleeping;
    model->pet_mood = model->sleeping ? SP_MOOD_SLEEP : SP_MOOD_IDLE;
    model->reaction_until = 0;
}

bool sc_model_voice_start(sc_model_t *model)
{
    if (model->voice == SC_VOICE_OFFLINE || model->voice == SC_VOICE_LISTENING) return false;
    sc_model_voice_set(model, SC_VOICE_LISTENING);
    return true;
}

void sc_model_voice_end(sc_model_t *model, uint64_t now_ms, bool demo)
{
    if (model->voice != SC_VOICE_LISTENING) return;
    model->voice = SC_VOICE_THINKING;
    model->voice_until = demo ? now_ms + model->timing.thinking_ms : 0;
}

void sc_model_voice_set(sc_model_t *model, sc_voice_t state)
{
    model->voice = state;
    model->voice_until = 0;
}
