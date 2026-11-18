/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Private appdata records or NVS; failed or unconfirmed writes disable later mutations. */
#pragma once
#include "card_data.h"
typedef card_result_t (*card_read_fn)(void *context,char *out,size_t *size);
typedef bool (*card_write_fn)(void *context,const char *data,size_t size);
typedef struct {
    void *context;
    card_read_fn read;
    card_write_fn write;
    bool writable;
    card_result_t result;
} card_store_t;
/** Read an existing record or commit a supplied fresh identity when the backend is empty. */
card_result_t card_store_load(card_store_t *store,card_state_t *state,const uint8_t fresh_id[16]);
/** Commit and read back the exact candidate. The caller publishes it only after CARD_OK. */
card_result_t card_store_save(card_store_t *store,const card_state_t *candidate);
/** Open two checksummed files in an existing private directory; no directory or filesystem is formatted. */
bool card_store_files_open(card_store_t *store,const char *directory);
/** Release the backend after all readers and writers stop. */
void card_store_close(card_store_t *store);
#ifdef ESP_PLATFORM
/** Prefer existing NVS state, otherwise appdata when mounted; never format shared storage. */
bool card_store_open(card_store_t *store);
#endif
