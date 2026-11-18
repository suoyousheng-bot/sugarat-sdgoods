/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Version-two records use explicit fields; V1 Ember records remain owned by pet_save.c. */
#include "pet_family.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
_Static_assert(32 + 10 + PET_MEMBERS_MAX * 94 + PET_FRIENDS_MAX * 102 + PET_RECEIPTS_MAX * 16 + 68 + 203 < PET_FAMILY_BYTES - 4,
               "Maximum family record must fit before the checksum");

static void put32(uint8_t *p, uint32_t value) { for (unsigned i = 0; i < 4; i++) p[i] = (uint8_t)(value >> (8 * i)); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t checksum(const uint8_t *p, size_t size)
{
    uint32_t value = 2166136261u;
    for (size_t i = 0; i < size; i++) value = (value ^ p[i]) * 16777619u;
    return value;
}
void pet_profile_encode(const pet_profile_t *p, uint8_t out[68])
{
    memset(out, 0, 68); memcpy(out, p->id.bytes, 16); memcpy(out + 16, p->parents[0].bytes, 16); memcpy(out + 32, p->parents[1].bytes, 16);
    memcpy(out + 48, p->blood, 2); memcpy(out + 50, p->traits, PET_TRAITS_COUNT);
    out[60] = (uint8_t)p->generation; out[61] = (uint8_t)(p->generation >> 8); put32(out + 62, p->age_ms);
}
bool pet_profile_decode(pet_profile_t *p, const uint8_t data[68])
{
    pet_profile_t decoded = {0};
    memcpy(decoded.id.bytes, data, 16); memcpy(decoded.parents[0].bytes, data + 16, 16); memcpy(decoded.parents[1].bytes, data + 32, 16);
    memcpy(decoded.blood, data + 48, 2); memcpy(decoded.traits, data + 50, PET_TRAITS_COUNT);
    decoded.generation = (uint16_t)(data[60] | data[61] << 8); decoded.age_ms = get32(data + 62);
    if (pet_id_empty(decoded.id) || !decoded.generation || decoded.blood[0] > PET_CLOUD || decoded.blood[1] > PET_CLOUD || data[66] || data[67] ||
        pet_id_equal(decoded.id, decoded.parents[0]) || pet_id_equal(decoded.id, decoded.parents[1])) return false;
    if (pet_id_empty(decoded.parents[0]) != pet_id_empty(decoded.parents[1])) return false;
    if (!pet_id_empty(decoded.parents[0]) && pet_id_equal(decoded.parents[0], decoded.parents[1])) return false;
    for (unsigned i = 0; i < PET_TRAITS_COUNT; i++) if (decoded.traits[i] > 15) return false;
    *p = decoded; return true;
}
typedef struct { uint8_t *p; } writer_t;
typedef struct { const uint8_t *p; bool valid; } reader_t;
static void write_profile(writer_t *w, const pet_profile_t *p) { pet_profile_encode(p, w->p); w->p += 68; }
static void write32(writer_t *w, uint32_t value) { put32(w->p, value); w->p += 4; }
static void write_id(writer_t *w, pet_id_t id) { memcpy(w->p, id.bytes, 16); w->p += 16; }
static void read_profile(reader_t *r, pet_profile_t *p) { if (!pet_profile_decode(p, r->p)) r->valid = false; r->p += 68; }
static uint32_t read32(reader_t *r) { uint32_t value = get32(r->p); r->p += 4; return value; }
static pet_id_t read_id(reader_t *r) { pet_id_t id; memcpy(id.bytes, r->p, 16); r->p += 16; return id; }
static bool counts_valid(const pet_family_t *f)
{
    return f->count > 0 && f->count <= PET_MEMBERS_MAX && f->active < f->count && f->friend_count <= PET_FRIENDS_MAX &&
        f->receipt_count <= PET_RECEIPTS_MAX && f->egg_ms <= PET_EGG_TIME_MS && (!f->tending_egg || f->has_egg) &&
        (!f->has_egg || f->count < PET_MEMBERS_MAX) && f->pending.phase <= PET_TX_ABORT;
}
bool pet_family_encode(const pet_family_t *f, uint32_t sequence, uint8_t out[PET_FAMILY_BYTES])
{
    if (!counts_valid(f) || !sequence) return false;
    memset(out, 0, PET_FAMILY_BYTES); memcpy(out, "SGF2", 4); put32(out + 4, 2); put32(out + 8, sequence); put32(out + 12, PET_FAMILY_BYTES);
    writer_t w = {out + 32};
    *w.p++ = f->count; *w.p++ = f->active; *w.p++ = f->friend_count; *w.p++ = f->receipt_count;
    *w.p++ = f->has_egg; *w.p++ = f->tending_egg; write32(&w, f->egg_ms);
    for (unsigned i = 0; i < f->count; i++) {
        const pet_member_t *m = &f->members[i]; write_profile(&w, &m->profile);
        _Static_assert(sizeof(float) == 4, "Family records require IEEE float32");
        for (unsigned j = 0; j < 5; j++) { uint32_t bits; memcpy(&bits, &m->care[j], 4); write32(&w, bits); }
        write32(&w, m->breed_rest_ms); *w.p++ = m->poop; *w.p++ = m->asleep;
    }
    for (unsigned i = 0; i < f->friend_count; i++) {
        const pet_friend_t *friend = &f->friends[i]; write_profile(&w, &friend->profile);
        *w.p++ = friend->interactions; *w.p++ = friend->bonded; memcpy(w.p, friend->key, 32); w.p += 32;
    }
    for (unsigned i = 0; i < f->receipt_count; i++) write_id(&w, f->receipts[i]);
    if (f->has_egg) write_profile(&w, &f->egg);
    *w.p++ = f->pending.phase;
    if (f->pending.phase != PET_TX_NONE) {
        write_id(&w, f->pending.id); write_id(&w, f->pending.peer);
        write_profile(&w, &f->pending.children[0]); write_profile(&w, &f->pending.children[1]);
        memcpy(w.p, f->pending.seed, 32); w.p += 32;
        *w.p++ = f->pending.own_child; *w.p++ = f->pending.coordinator;
    }
    put32(out + 16, (uint32_t)(w.p - out)); put32(out + PET_FAMILY_BYTES - 4, checksum(out, PET_FAMILY_BYTES - 4));
    pet_family_t *check = malloc(sizeof(*check)); uint32_t check_sequence;
    if (!check) return false;
    bool valid = pet_family_decode(check, &check_sequence, out, PET_FAMILY_BYTES) == PET_FAMILY_OK;
    free(check); return valid;
}
pet_family_result_t pet_family_decode(pet_family_t *family, uint32_t *sequence, const uint8_t *data, size_t size)
{
    if (size != PET_FAMILY_BYTES || memcmp(data, "SGF2", 4) || get32(data + size - 4) != checksum(data, size - 4)) return PET_FAMILY_CORRUPT;
    if (get32(data + 4) != 2) return PET_FAMILY_VERSION;
    if (get32(data + 12) != PET_FAMILY_BYTES || !get32(data + 8)) return PET_FAMILY_CORRUPT;
    pet_family_t f = {0}; reader_t r = {data + 32, true};
    f.count = *r.p++; f.active = *r.p++; f.friend_count = *r.p++; f.receipt_count = *r.p++;
    uint8_t has_egg = *r.p++, tending = *r.p++; f.has_egg = has_egg != 0; f.tending_egg = tending != 0; f.egg_ms = read32(&r);
    if (has_egg > 1 || tending > 1 || !counts_valid(&f)) return PET_FAMILY_CORRUPT;
    for (unsigned i = 0; i < f.count; i++) {
        pet_member_t *m = &f.members[i]; read_profile(&r, &m->profile);
        for (unsigned j = 0; j < 5; j++) {
            uint32_t bits = read32(&r); memcpy(&m->care[j], &bits, 4);
            if (!isfinite(m->care[j]) || m->care[j] < 0 || m->care[j] > 100) r.valid = false;
        }
        if (m->care[4] < 20) r.valid = false;
        m->breed_rest_ms = read32(&r); m->poop = *r.p++; uint8_t asleep = *r.p++; m->asleep = asleep != 0;
        if (m->breed_rest_ms > PET_BREED_REST_MS || m->poop > 4 || asleep > 1) r.valid = false;
        for (unsigned j = 0; j < i; j++) if (pet_id_equal(m->profile.id, f.members[j].profile.id)) r.valid = false;
    }
    for (unsigned i = 0; i < f.friend_count; i++) {
        pet_friend_t *friend = &f.friends[i]; read_profile(&r, &friend->profile);
        friend->interactions = *r.p++; uint8_t bonded = *r.p++; friend->bonded = bonded != 0;
        memcpy(friend->key, r.p, 32); r.p += 32;
        if (friend->interactions > 7 || bonded > 1) r.valid = false;
        for (unsigned j = 0; j < i; j++) if (pet_id_equal(friend->profile.id, f.friends[j].profile.id)) r.valid = false;
    }
    for (unsigned i = 0; i < f.receipt_count; i++) {
        f.receipts[i] = read_id(&r); if (pet_id_empty(f.receipts[i])) r.valid = false;
        for (unsigned j = 0; j < i; j++) if (pet_id_equal(f.receipts[i], f.receipts[j])) r.valid = false;
    }
    if (f.has_egg) {
        read_profile(&r, &f.egg);
        for (unsigned i = 0; i < f.count; i++) if (pet_id_equal(f.egg.id, f.members[i].profile.id)) r.valid = false;
    }
    uint8_t phase = *r.p++; if (phase > PET_TX_ABORT) r.valid = false; f.pending.phase = (pet_tx_phase_t)phase;
    if (phase != PET_TX_NONE) {
        f.pending.id = read_id(&r); f.pending.peer = read_id(&r);
        read_profile(&r, &f.pending.children[0]); read_profile(&r, &f.pending.children[1]);
        memcpy(f.pending.seed, r.p, 32); r.p += 32;
        f.pending.own_child = *r.p++; uint8_t coordinator = *r.p++; f.pending.coordinator = coordinator != 0;
        if (f.pending.own_child > 1 || coordinator > 1 || pet_id_empty(f.pending.id) || pet_id_empty(f.pending.peer) ||
            pet_id_equal(f.pending.children[0].id, f.pending.children[1].id)) r.valid = false;
    }
    if (!r.valid || (uint32_t)(r.p - data) != get32(data + 16)) return PET_FAMILY_CORRUPT;
    *family = f; *sequence = get32(data + 8); return PET_FAMILY_OK;
}
