/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "pet_model.h"
#include <limits.h>
#include <string.h>

/* One simulation second per real minute; pose animation keeps real time. */
#define CARE_TIME_DIVISOR 60u
#define AGE_LEVEL_MS 30000u

void pet_model_restored(pet_model_t *m)
{
    m->pet.display_timeout_ms = 0; /* The BSP owns physical display power. */
    m->pet.display_on = true;
    m->pet.age_level_ms = AGE_LEVEL_MS;
    m->pet.alive = true;
    if (m->pet.health < 20) m->pet.health = 20;
    epet_actor_set_age(&m->pet.actor, epet_age_level(&m->pet));
    m->page = m->pet.asleep ? PET_SLEEPING : PET_HOME;
}

void pet_model_init(pet_model_t *m, uint64_t now_ms)
{
    memset(m, 0, sizeof(*m));
    epet_init(&m->pet);
    pet_model_restored(m);
    m->last_ms = now_ms;
    m->message = "Hello, friend!";
    m->feedback_ms = 2400;
    m->dirty = true;
}

void pet_model_tick(pet_model_t *m, uint64_t now_ms)
{
    uint64_t elapsed = now_ms >= m->last_ms ? now_ms - m->last_ms : 0;
    m->last_ms = now_ms;
    if (m->paused) return;
    uint32_t dt = elapsed > 1000 ? 1000 : (uint32_t)elapsed;
    m->animation_ms += dt;
    m->emote_ms = m->emote_ms > dt ? m->emote_ms - dt : 0;
    m->cooldown_ms = m->cooldown_ms > dt ? m->cooldown_ms - dt : 0;
    m->feedback_ms = m->feedback_ms > dt ? m->feedback_ms - dt : 0;
    if (!m->feedback_ms && m->page == PET_FEEDING) m->page = PET_HOME;
    uint32_t care = (dt + m->remainder_ms) / CARE_TIME_DIVISOR;
    m->remainder_ms = (dt + m->remainder_ms) % CARE_TIME_DIVISOR;
    bool asleep = m->pet.asleep;
    static const bool none[EPET_BTN_COUNT] = {false};
    /* Saturation preserves the last life stage after months of active play. */
    if (m->pet.age_ms > UINT32_MAX - 1000u) m->pet.age_ms = UINT32_MAX - 1000u;
    epet_advance(&m->pet, care, none, none);
    epet_actor_tick(&m->pet.actor, dt - care);
    /* Gentle care keeps neglect recoverable; sickness remains visible. */
    if (m->pet.health < 20) m->pet.health = 20;
    m->pet.alive = true;
    if (asleep != m->pet.asleep) {
        m->page = m->pet.asleep ? PET_SLEEPING : PET_HOME;
        m->dirty = true;
    }
    if (m->pet.asleep && m->page == PET_HOME) m->page = PET_SLEEPING;
    m->save_elapsed_ms += dt;
    if (care) m->dirty = true;
}

void pet_model_pause(pet_model_t *m, bool paused, uint64_t now_ms)
{
    m->paused = paused;
    m->last_ms = now_ms;
}

bool pet_model_action(pet_model_t *m, pet_action_t action)
{
    if (m->paused) return false;
    if (action == PET_STATUS) { m->page = PET_STATS; return true; }
    if (action == PET_BACK) { m->page = m->pet.asleep ? PET_SLEEPING : PET_HOME; return true; }
    if (m->cooldown_ms) return false;
    if (action == PET_SLEEP || action == PET_WAKE) {
        m->pet.asleep = action == PET_SLEEP;
        m->page = m->pet.asleep ? PET_SLEEPING : PET_HOME;
        m->message = m->pet.asleep ? "Sweet dreams" : "Good morning!";
    } else {
        if (m->pet.asleep) return false;
        bool accepted = true;
        switch (action) {
        case PET_FEED:
            if (m->pet.hunger < 5) { m->message = "Already full!"; accepted = false; }
            else { accepted = epet_apply_action(&m->pet, EPET_ACT_FEED); m->page = PET_FEEDING; m->message = "Yummy! Thank you"; }
            break;
        case PET_PLAY:
            if (m->pet.energy < 15) { m->message = "Let's rest first"; accepted = false; }
            else { accepted = epet_apply_action(&m->pet, EPET_ACT_PLAY); m->message = "That was fun!"; }
            break;
        case PET_CLEAN:
            accepted = epet_apply_action(&m->pet, EPET_ACT_CLEAN);
            m->message = accepted ? "Fresh and clean!" : "Already clean!";
            break;
        case PET_TOUCH:
            m->pet.happiness += m->pet.happiness <= 98 ? 2 : 100 - m->pet.happiness;
            epet_pet_emote(&m->pet, EPET_POSE_HAPPY);
            m->message = "Happy to see you!";
            break;
        default: return false;
        }
        m->feedback_ms = 2200;
        m->cooldown_ms = 650;
        if (!accepted) return false;
    }
    m->dirty = true;
    m->emote = action; m->emote_ms = 2200;
    m->feedback_ms = 2200;
    m->cooldown_ms = 650;
    return true;
}

const char *pet_model_stage(const pet_model_t *m)
{
    uint8_t age = epet_age_level(&m->pet);
    return age < 11 ? "Baby" : age < 31 ? "Growing" : "Adult";
}
