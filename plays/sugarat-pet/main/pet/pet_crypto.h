/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** SHA-256, authenticated messages and ephemeral P-256 pairing through maintained crypto libraries. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct { void *implementation; uint8_t public_key[65]; } pet_crypto_t;
/** Fill caller memory with platform cryptographic randomness; false leaves the operation unusable. */
bool pet_random_bytes(void *out, size_t size);
/** Hash exact encoded bytes using SHA-256. */
bool pet_sha256(const void *data, size_t size, uint8_t out[32]);
/** Authenticate encoded messages with a 32-byte session key. */
bool pet_hmac(const uint8_t key[32], const void *data, size_t size, uint8_t out[32]);
/** Generate an ephemeral P-256 keypair; caller must close it on every exit. */
bool pet_crypto_open(pet_crypto_t *crypto);
/** Compute a shared secret and bind it to an agreed transcript hash. */
bool pet_crypto_agree(pet_crypto_t *crypto, const uint8_t peer_key[65], const uint8_t transcript[32], uint8_t out[32]);
/** Release an ephemeral keypair and clear its public copy. */
void pet_crypto_close(pet_crypto_t *crypto);
/** Compare authentication bytes without content-dependent early exit. */
bool pet_secure_equal(const uint8_t *a, const uint8_t *b, size_t size);
/** Encrypt one bounded packet with a caller-unique nonce and authenticated header. */
bool pet_encrypt(const uint8_t key[32], const uint8_t nonce[12], const void *header, size_t header_size, uint8_t *payload, size_t size, uint8_t tag[16]);
/** Authenticate before exposing plaintext; failed packets must be discarded. */
bool pet_decrypt(const uint8_t key[32], const uint8_t nonce[12], const void *header, size_t header_size, uint8_t *payload, size_t size, const uint8_t tag[16]);
