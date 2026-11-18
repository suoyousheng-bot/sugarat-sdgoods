/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Individual identities, paused family members and deterministic inherited appearances. */
#pragma once
#include "pet_model.h"
#include <stddef.h>

#define PET_MEMBERS_MAX 6
#define PET_FRIENDS_MAX 16
#define PET_RECEIPTS_MAX 16
#define PET_TRAITS_COUNT 10
#define PET_FAMILY_BYTES 8192
#define PET_EGG_TIME_MS (30u * 60u * 1000u)
#define PET_BREED_REST_MS (120u * 60u * 1000u)

typedef struct { uint8_t bytes[16]; } pet_id_t;
typedef enum { PET_FIRE, PET_LEAF, PET_CLOUD, PET_SPROUT, PET_EMBERCLOUD, PET_LEAFCLOUD, PET_KIND_COUNT } pet_kind_t;
typedef enum { PET_BODY, PET_COLOR, PET_EYES, PET_EYE_COLOR, PET_EARS, PET_MOUTH, PET_TAIL, PET_PATTERN, PET_FEATURE, PET_PERSONALITY } pet_trait_t;
typedef struct {
    pet_id_t id, parents[2];
    uint8_t blood[2], traits[PET_TRAITS_COUNT];
    uint16_t generation;
    uint32_t age_ms;
} pet_profile_t;
typedef struct {
    pet_profile_t profile;
    float care[5];
    uint32_t breed_rest_ms;
    uint8_t poop;
    bool asleep;
} pet_member_t;
typedef struct {
    pet_profile_t profile;
    uint8_t interactions;
    uint8_t key[32];
    bool bonded;
} pet_friend_t;
typedef enum { PET_TX_NONE, PET_TX_PREPARED, PET_TX_COMMIT, PET_TX_APPLIED, PET_TX_ABORT } pet_tx_phase_t;
typedef struct {
    pet_id_t id, peer;
    pet_profile_t children[2];
    uint8_t seed[32];
    uint8_t own_child;
    pet_tx_phase_t phase;
    bool coordinator;
} pet_pending_t;
typedef struct {
    pet_member_t members[PET_MEMBERS_MAX];
    pet_friend_t friends[PET_FRIENDS_MAX];
    pet_id_t receipts[PET_RECEIPTS_MAX];
    pet_profile_t egg;
    pet_pending_t pending;
    uint32_t egg_ms;
    uint8_t count, active, friend_count, receipt_count;
    bool has_egg, tending_egg;
} pet_family_t;
typedef enum { PET_FAMILY_OK, PET_FAMILY_CORRUPT, PET_FAMILY_VERSION } pet_family_result_t;

/** Compare all 128 bits; IDs never derive from truncated device MACs. */
bool pet_id_equal(pet_id_t a, pet_id_t b);
/** Return a stable byte ordering for protocol roles and canonical parents. */
int pet_id_compare(pet_id_t a, pet_id_t b);
/** Return whether the reserved zero identity is absent. */
bool pet_id_empty(pet_id_t id);
/** Resolve two symmetric base alleles to one of the six character families. */
pet_kind_t pet_profile_kind(const pet_profile_t *profile);
/** Return the catalog name of a character family. */
const char *pet_kind_name(pet_kind_t kind);
/** Import the current companion with a caller-supplied random identity. */
void pet_family_init(pet_family_t *family, const pet_model_t *model, pet_id_t id);
/** Capture active care statistics without moving other family members' clocks. */
void pet_family_capture(pet_family_t *family, const pet_model_t *model);
/** Select a family member and restore its saved care into the existing simulation. */
bool pet_family_select(pet_family_t *family, pet_model_t *model, unsigned index);
/** Restore the active member after loading a complete family record. */
void pet_family_restore(const pet_family_t *family, pet_model_t *model);
/** Advance active-parent cooldown or egg progress by effective foreground time only. */
void pet_family_tick(pet_family_t *family, uint32_t elapsed_ms);
/** Hatch a ready egg into a reserved free member slot and select the child. */
bool pet_family_hatch(pet_family_t *family, pet_model_t *model);
/** Find or add a friend; return NULL when the fixed friend capacity is exhausted. */
pet_friend_t *pet_family_friend(pet_family_t *family, const pet_profile_t *profile, bool add);
/** Reject self, known parents and siblings, immature pets and occupied family slots. */
bool pet_family_can_breed(const pet_family_t *family, const pet_profile_t *peer);
/** Compute canonical sibling plans from both parents and 32 agreed random bytes. */
bool pet_genetics(const pet_profile_t parents[2], const uint8_t seed[32], pet_profile_t children[2]);
/** Apply a durable commit once; reserve the egg and record a receipt before acknowledging. */
bool pet_family_apply_commit(pet_family_t *family);
/** Encode an explicit little-endian versioned family record; return false on invalid state. */
bool pet_family_encode(const pet_family_t *family, uint32_t sequence, uint8_t out[PET_FAMILY_BYTES]);
/** Validate an entire record before replacing the destination family. */
pet_family_result_t pet_family_decode(pet_family_t *family, uint32_t *sequence, const uint8_t *data, size_t size);
/** Encode public profile fields into 68 bytes for authenticated protocol messages. */
void pet_profile_encode(const pet_profile_t *profile, uint8_t out[68]);
/** Validate public profile fields and replace the output only on success. */
bool pet_profile_decode(pet_profile_t *profile, const uint8_t data[68]);
