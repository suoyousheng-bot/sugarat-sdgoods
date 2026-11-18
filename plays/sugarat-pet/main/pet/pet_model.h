/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Offline care and real-time animation over the pinned epet simulation; LVGL-thread owned. */
#pragma once
#include "epet.h"

typedef enum { PET_HOME, PET_FEEDING, PET_STATS, PET_SLEEPING } pet_page_t;
typedef enum { PET_TOUCH, PET_FEED, PET_PLAY, PET_CLEAN, PET_SLEEP, PET_WAKE, PET_STATUS, PET_BACK,
    PET_FAMILY, PET_NEARBY, PET_NEXT, PET_SELECT, PET_EGG, PET_CONFIRM, PET_GREET, PET_GIFT, PET_TOGETHER, PET_BREED } pet_action_t;
typedef struct {
    epet_t pet;
    pet_page_t page;
    uint32_t feedback_ms, cooldown_ms, remainder_ms;
    uint64_t last_ms, save_elapsed_ms;
    uint64_t animation_ms;
    uint32_t emote_ms;
    pet_action_t emote;
    bool paused, dirty;
    const char *message;
} pet_model_t;

/** Register the bundled character once before initializing any models. */
void pet_species_register(void);
/** Initialize a new companion; now_ms is a monotonic real-time clock. */
void pet_model_init(pet_model_t *model, uint64_t now_ms);
/** Advance foreground time only, with a bounded catch-up after stalled frames. */
void pet_model_tick(pet_model_t *model, uint64_t now_ms);
/** Apply a touch action; return false for paused, cooling-down or refused care. */
bool pet_model_action(pet_model_t *model, pet_action_t action);
/** Freeze elapsed care time and rebase its clock when pausing or resuming. */
void pet_model_pause(pet_model_t *model, bool paused, uint64_t now_ms);
/** Restore timing and presentation after validating persisted epet stats. */
void pet_model_restored(pet_model_t *model);
/** Return the English life-stage label for the current epet age level. */
const char *pet_model_stage(const pet_model_t *model);
