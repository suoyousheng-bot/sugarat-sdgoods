/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Validated UTF-8 profiles and a versioned, application-private contact store. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CARD_CONTACTS_MAX 12
#define CARD_JSON_MAX 1024
#define CARD_STORE_MAX 16384
typedef enum {CARD_EMAIL, CARD_PHONE, CARD_WECHAT} card_contact_kind_t;
typedef struct {
    char company[65], name[49], role[65], contact[97], link[161];
    card_contact_kind_t kind;
} card_profile_t;
typedef struct {uint8_t id[16]; card_profile_t profile;} card_contact_t;
typedef struct {
    uint8_t id[16];
    card_profile_t profile;
    unsigned count;
    card_contact_t contacts[CARD_CONTACTS_MAX];
} card_state_t;
typedef enum {CARD_OK, CARD_INVALID, CARD_VERSION, CARD_FULL, CARD_IO, CARD_READONLY} card_result_t;
/** Return whether all fields fit their encoded limits and contain valid, single-line UTF-8. */
bool card_profile_valid(const card_profile_t *profile, bool require_name);
/** Decode exactly six fields from a bounded JSON object. No destination changes on error. */
card_result_t card_profile_parse(const char *json, size_t size, card_profile_t *out);
/** Encode a validated profile; returns bytes excluding NUL, or zero on insufficient capacity. */
size_t card_profile_json(const card_profile_t *profile, char *out, size_t capacity);
/** Encode an escaped vCard 3.0; a WeChat identifier uses NOTE rather than a phone number. */
size_t card_profile_vcard(const card_profile_t *profile, char *out, size_t capacity);
/** Encode the complete version-one state as JSON. */
size_t card_state_json(const card_state_t *state, char *out, size_t capacity);
/** Unknown versions and invalid records leave the caller's state untouched. */
card_result_t card_state_parse(const char *json, size_t size, card_state_t *out);
/** Update an existing peer or append a new one; full storage never evicts another contact. */
card_result_t card_state_receive(card_state_t *state, const uint8_t peer[16], const card_profile_t *profile);
/** Convert a 16-byte identity to a lower-case 32-character string. */
void card_id_hex(const uint8_t id[16], char out[33]);
