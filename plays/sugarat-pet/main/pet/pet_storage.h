/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Alternating private NVS records; unknown formats and unreadable state disable writes. */
#pragma once
#include "pet_save.h"
#include "nvs.h"
typedef struct {
    nvs_handle_t handle;
    uint32_t sequence;
    unsigned next_slot;
    bool writable, opened;
    const char *notice;
} pet_storage_t;

/** Open sugarat_pet and restore the newest valid record without clearing NVS. */
void pet_storage_open(pet_storage_t *storage, pet_model_t *model);
/** Commit and read back the next record; false leaves a visible failure and disables further writes. */
bool pet_storage_flush(pet_storage_t *storage, pet_model_t *model);
/** Close a previously opened NVS handle. */
void pet_storage_close(pet_storage_t *storage);
