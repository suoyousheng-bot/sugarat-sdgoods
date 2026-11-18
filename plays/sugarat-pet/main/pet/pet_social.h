/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** LVGL-thread social state machine. Radio callbacks must only enqueue datagrams. */
#pragma once
#include "pet_family.h"
#include "pet_crypto.h"
#define PET_NEARBY_MAX 4
#define PET_PACKET_MAX 250
typedef bool (*pet_social_send_fn)(void *context, const uint8_t mac[6], const uint8_t *data, size_t size);
typedef bool (*pet_social_save_fn)(void *context);
typedef enum { PET_SOCIAL_OFF, PET_SOCIAL_SEARCH, PET_SOCIAL_KEY, PET_SOCIAL_VERIFY, PET_SOCIAL_LINKED, PET_SOCIAL_FAULT } pet_social_state_t;
typedef struct { uint8_t mac[6], nonce[16], kind; uint64_t seen_ms; } pet_nearby_t;
typedef struct {
    pet_family_t *family;
    pet_social_send_fn send;
    pet_social_save_fn save;
    void *context;
    pet_crypto_t crypto;
    pet_profile_t self, peer;
    pet_nearby_t nearby[PET_NEARBY_MAX];
    uint8_t nonce[16], peer_nonce[16], peer_mac[6], peer_public[65], sid[16], tx_key[32], rx_key[32], bond_key[32];
    uint64_t now_ms, broadcast_ms, transmit_ms, received_ms;
    uint32_t tx_sequence, rx_sequence, rx_window, code;
    unsigned nearby_count;
    uint8_t offered_interactions;
    pet_social_state_t state;
    bool selected, agreed, confirmed, peer_confirmed, peer_identified, known_friend, wants_egg, peer_wants_egg;
    const char *notice;
} pet_social_t;
/** Open an ephemeral nearby session with a frozen active-parent profile. */
bool pet_social_open(pet_social_t *social, pet_family_t *family, pet_social_send_fn send, pet_social_save_fn save, void *context, uint64_t now_ms);
/** Release ephemeral keys; durable pending transactions remain in the family. */
void pet_social_close(pet_social_t *social);
/** Select a recently observed device; both devices must choose each other. */
bool pet_social_select(pet_social_t *social, unsigned candidate);
/** Accept the six-digit code after checking it on the other screen. */
bool pet_social_confirm(pet_social_t *social);
/** Share one of three cumulative interaction bits, persisting before transmission. */
bool pet_social_interact(pet_social_t *social, unsigned bit);
/** Record explicit session consent to breed; both users must request it. */
bool pet_social_breed(pet_social_t *social);
/** Process one bounded radio datagram on the owner's thread. */
void pet_social_receive(pet_social_t *social, const uint8_t mac[6], const uint8_t *data, size_t size);
/** Retry idempotent state messages without blocking the render loop. */
void pet_social_tick(pet_social_t *social, uint64_t now_ms);
