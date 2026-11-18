/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "pet_animation.h"
#include "pet_art.h"
#include <string.h>
unsigned pet_animation_frame(const pet_model_t *m, const pet_profile_t *p)
{
    if (m->pet.asleep) return 6;
    if (m->emote_ms) return (m->emote == PET_FEED ? 8 : 12) + ((2200u - m->emote_ms) / 180u) % 4;
    unsigned t = (unsigned)(m->animation_ms % 6000u);
    if (t >= 5400) return 4 + ((t - 5400) / 150u) % 4;
    return (t / (200u + p->traits[PET_PERSONALITY] * 12u)) % 4;
}
bool pet_animation_decode(pet_kind_t kind, unsigned frame, uint8_t pixels[96 * 96])
{
    if (kind >= PET_KIND_COUNT || frame >= 16) return false;
    const pet_art_frame_t *f = &pet_art_characters[kind].frames[frame];
    size_t used = 0;
    for (size_t i = 0; i + 1 < f->size; i += 2) {
        unsigned count = f->data[i], color = f->data[i + 1];
        if (!count || color > 31 || used + count > 96 * 96) return false;
        memset(pixels + used, color, count); used += count;
    }
    return used == 96 * 96 && !(f->size & 1);
}
