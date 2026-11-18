/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "pet_family.h"
#include "pet_crypto.h"
#include <limits.h>
#include <string.h>

bool pet_id_equal(pet_id_t a, pet_id_t b) { return memcmp(a.bytes, b.bytes, 16) == 0; }
int pet_id_compare(pet_id_t a, pet_id_t b) { return memcmp(a.bytes, b.bytes, 16); }
bool pet_id_empty(pet_id_t id) { const pet_id_t zero = {{0}}; return pet_id_equal(id, zero); }
pet_kind_t pet_profile_kind(const pet_profile_t *p)
{
    uint8_t a = p->blood[0], b = p->blood[1];
    if (a == b) return (pet_kind_t)a;
    return a != PET_CLOUD && b != PET_CLOUD ? PET_SPROUT : a == PET_FIRE || b == PET_FIRE ? PET_EMBERCLOUD : PET_LEAFCLOUD;
}
const char *pet_kind_name(pet_kind_t kind)
{
    static const char *const names[] = {"Ember", "Leaf", "Cloud", "Sprout", "Glow", "Willow"};
    return kind < PET_KIND_COUNT ? names[kind] : "Unknown";
}
void pet_family_capture(pet_family_t *f, const pet_model_t *m)
{
    pet_member_t *member = &f->members[f->active];
    member->profile.age_ms = m->pet.age_ms;
    const float care[] = {m->pet.hunger, m->pet.happiness, m->pet.energy, m->pet.hygiene, m->pet.health};
    memcpy(member->care, care, sizeof(care)); member->asleep = m->pet.asleep; member->poop = m->pet.poop;
}
void pet_family_init(pet_family_t *f, const pet_model_t *m, pet_id_t id)
{
    memset(f, 0, sizeof(*f)); f->count = 1;
    f->members[0].profile.id = id; f->members[0].profile.generation = 1;
    pet_family_capture(f, m);
}
bool pet_family_select(pet_family_t *f, pet_model_t *m, unsigned index)
{
    if (index >= f->count || (f->pending.phase != PET_TX_NONE && index != f->active)) return false;
    pet_family_capture(f, m); f->active = (uint8_t)index; f->tending_egg = false;
    pet_family_restore(f, m);
    return true;
}
void pet_family_restore(const pet_family_t *f, pet_model_t *m)
{
    const pet_member_t *member = &f->members[f->active];
    m->pet.age_ms = member->profile.age_ms; m->pet.ev_minute = m->pet.age_ms / 60000;
    m->pet.hunger = member->care[0]; m->pet.happiness = member->care[1]; m->pet.energy = member->care[2];
    m->pet.hygiene = member->care[3]; m->pet.health = member->care[4]; m->pet.asleep = member->asleep; m->pet.poop = member->poop;
    m->cooldown_ms = m->feedback_ms = m->emote_ms = m->remainder_ms = 0; m->dirty = true; pet_model_restored(m);
}
void pet_family_tick(pet_family_t *f, uint32_t elapsed)
{
    if (f->has_egg && f->tending_egg) f->egg_ms = elapsed >= PET_EGG_TIME_MS - f->egg_ms ? PET_EGG_TIME_MS : f->egg_ms + elapsed;
    else {
        pet_member_t *m = &f->members[f->active];
        m->breed_rest_ms = elapsed >= m->breed_rest_ms ? 0 : m->breed_rest_ms - elapsed;
    }
}
bool pet_family_hatch(pet_family_t *f, pet_model_t *m)
{
    if (!f->has_egg || f->egg_ms < PET_EGG_TIME_MS || f->count >= PET_MEMBERS_MAX || f->pending.phase != PET_TX_NONE) return false;
    pet_family_capture(f, m);
    unsigned index = f->count++; pet_member_t *member = &f->members[index]; memset(member, 0, sizeof(*member));
    member->profile = f->egg; member->care[0] = 20; member->care[1] = 85; member->care[2] = 90; member->care[3] = member->care[4] = 100;
    f->has_egg = f->tending_egg = false; f->egg_ms = 0; memset(&f->egg, 0, sizeof(f->egg));
    return pet_family_select(f, m, index);
}
pet_friend_t *pet_family_friend(pet_family_t *f, const pet_profile_t *p, bool add)
{
    for (unsigned i = 0; i < f->friend_count; i++) if (pet_id_equal(f->friends[i].profile.id, p->id)) return &f->friends[i];
    if (!add || f->friend_count >= PET_FRIENDS_MAX) return NULL;
    pet_friend_t *friend = &f->friends[f->friend_count++]; memset(friend, 0, sizeof(*friend)); friend->profile = *p; return friend;
}
bool pet_family_can_breed(const pet_family_t *f, const pet_profile_t *p)
{
    const pet_member_t *m = &f->members[f->active]; const pet_profile_t *own = &m->profile;
    if (f->has_egg || f->count >= PET_MEMBERS_MAX || f->receipt_count >= PET_RECEIPTS_MAX ||
        f->pending.phase != PET_TX_NONE || m->breed_rest_ms || m->asleep || m->care[2] < 30 ||
        own->age_ms < 900000u || p->age_ms < 900000u || pet_id_equal(own->id, p->id) ||
        own->generation == UINT16_MAX || p->generation == UINT16_MAX) return false;
    for (unsigned i = 0; i < f->count; i++) if (pet_id_equal(f->members[i].profile.id, p->id)) return false;
    for (unsigned i = 0; i < 2; i++) {
        if (pet_id_equal(own->parents[i], p->id) || pet_id_equal(p->parents[i], own->id)) return false;
        for (unsigned j = 0; j < 2; j++) if (!pet_id_empty(own->parents[i]) && pet_id_equal(own->parents[i], p->parents[j])) return false;
    }
    for (unsigned i = 0; i < f->friend_count; i++) if (pet_id_equal(f->friends[i].profile.id, p->id)) return f->friends[i].interactions == 7;
    return false;
}
bool pet_genetics(const pet_profile_t input[2], const uint8_t seed[32], pet_profile_t children[2])
{
    const pet_profile_t *a = &input[0], *b = &input[1];
    if (pet_id_compare(a->id, b->id) > 0) { a = &input[1]; b = &input[0]; }
    for (unsigned k = 0; k < 2; k++) {
        pet_profile_t *child = &children[k]; memset(child, 0, sizeof(*child));
        uint8_t bytes[32 + 68 * 2 + 1], digest[32]; memcpy(bytes, seed, 32);
        pet_profile_encode(a, bytes + 32); pet_profile_encode(b, bytes + 100); bytes[168] = (uint8_t)k;
        /* Care age changes between sessions; hereditary inputs remain immutable. */
        memset(bytes + 94, 0, 4); memset(bytes + 162, 0, 4);
        if (!pet_sha256(bytes, sizeof(bytes), digest)) { memset(children, 0, sizeof(*children) * 2); return false; }
        memcpy(child->id.bytes, digest, 16); child->parents[0] = a->id; child->parents[1] = b->id;
        child->generation = (a->generation > b->generation ? a->generation : b->generation) + 1;
        child->blood[0] = a->blood[digest[16] & 1]; child->blood[1] = b->blood[digest[17] & 1];
        for (unsigned i = 0; i < PET_TRAITS_COUNT; i++) {
            child->traits[i] = (digest[18 + i] & 1) ? a->traits[i] : b->traits[i];
            if (digest[18 + i] % 100 < 5) child->traits[i] = (child->traits[i] + 1 + digest[i] % 3) % 8;
        }
    }
    if (pet_profile_kind(&children[0]) == pet_profile_kind(&children[1]) &&
        children[0].traits[PET_BODY] % 3 == children[1].traits[PET_BODY] % 3 &&
        children[0].traits[PET_COLOR] % 4 == children[1].traits[PET_COLOR] % 4 &&
        children[0].traits[PET_PATTERN] % 3 == children[1].traits[PET_PATTERN] % 3)
        children[1].traits[PET_PATTERN] = (children[0].traits[PET_PATTERN] + 1) % 3;
    return true;
}
bool pet_family_apply_commit(pet_family_t *f)
{
    pet_pending_t *p = &f->pending;
    for (unsigned i = 0; i < f->receipt_count; i++) if (pet_id_equal(f->receipts[i], p->id)) { p->phase = PET_TX_APPLIED; return true; }
    if (p->phase != PET_TX_COMMIT || f->has_egg || f->count >= PET_MEMBERS_MAX || f->receipt_count >= PET_RECEIPTS_MAX) return false;
    f->egg = p->children[p->own_child]; f->has_egg = true; f->tending_egg = false; f->egg_ms = 0;
    f->members[f->active].breed_rest_ms = PET_BREED_REST_MS;
    f->receipts[f->receipt_count++] = p->id; p->phase = PET_TX_APPLIED;
    return true;
}
