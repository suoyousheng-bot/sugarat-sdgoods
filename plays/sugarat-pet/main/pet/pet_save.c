/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "pet_save.h"
#include <math.h>
#include <string.h>

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}
static uint32_t checksum(const uint8_t *p, size_t n)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u;
    return h;
}

void pet_save_encode(const pet_model_t *m, uint32_t sequence, uint8_t out[PET_SAVE_BYTES])
{
    memset(out, 0, PET_SAVE_BYTES);
    memcpy(out, "SGPT", 4);
    put32(out + 4, 1);
    put32(out + 8, sequence);
    put32(out + 12, m->pet.age_ms);
    float stats[] = {m->pet.hunger, m->pet.happiness, m->pet.energy, m->pet.hygiene, m->pet.health};
    _Static_assert(sizeof(float) == 4, "Save records require 32-bit IEEE floats");
    for (unsigned i = 0; i < 5; i++) {
        uint32_t bits;
        memcpy(&bits, &stats[i], 4);
        put32(out + 16 + 4 * i, bits);
    }
    out[36] = m->pet.asleep;
    out[37] = m->pet.poop;
    memcpy(out + 40, "EMBER", 5);
    put32(out + 52, checksum(out, 52));
}

pet_record_result_t pet_save_decode(pet_model_t *m, const uint8_t *data, size_t size, uint32_t *sequence)
{
    if (size != PET_SAVE_BYTES || memcmp(data, "SGPT", 4) || get32(data + 52) != checksum(data, 52)) return PET_RECORD_CORRUPT;
    if (get32(data + 4) != 1) return PET_RECORD_VERSION;
    if (data[36] > 1 || data[37] > 4 || memcmp(data + 40, "EMBER\0\0\0\0\0\0\0", 12)) return PET_RECORD_CORRUPT;
    float stats[5];
    for (unsigned i = 0; i < 5; i++) {
        uint32_t bits = get32(data + 16 + 4 * i);
        memcpy(&stats[i], &bits, 4);
        if (!isfinite(stats[i]) || stats[i] < 0 || stats[i] > 100) return PET_RECORD_CORRUPT;
    }
    if (stats[4] < 20) return PET_RECORD_CORRUPT;
    m->pet.age_ms = get32(data + 12);
    m->pet.ev_minute = m->pet.age_ms / 60000;
    m->pet.hunger = stats[0]; m->pet.happiness = stats[1]; m->pet.energy = stats[2];
    m->pet.hygiene = stats[3]; m->pet.health = stats[4];
    m->pet.asleep = data[36] != 0; m->pet.poop = data[37];
    pet_model_restored(m);
    *sequence = get32(data + 8);
    return PET_RECORD_OK;
}
