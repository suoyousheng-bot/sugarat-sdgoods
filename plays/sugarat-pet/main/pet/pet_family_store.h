/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Verified alternating records with an optional application-owned storage provider. */
#pragma once
#include "pet_family.h"
typedef struct pet_family_store pet_family_store_t;
struct pet_family_store {
    char directory[192];
    uint32_t sequence;
    unsigned next_slot;
    bool writable, loaded;
    const char *notice;
    void *backend_context;
    bool (*backend_save)(void *context, pet_family_store_t *store, const pet_family_t *family);
};
/** Restore a validated family; missing files leave the caller's migrated family intact. */
bool pet_family_store_open(pet_family_store_t *store, const char *directory, pet_family_t *family);
/** Write, sync and read back a complete next generation before reporting success. */
bool pet_family_store_save(pet_family_store_t *store, const pet_family_t *family);
