/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Compact family records for devices whose appdata filesystem is unavailable. */
#pragma once
#include "pet_family_store.h"
#include "nvs.h"
#define PET_NVS_RECORD_MAX 3072
typedef struct { nvs_handle_t handle; bool opened, present; } pet_family_nvs_t;
/** Probe existing compact records; the caller keeps this owner alive while the store uses it. */
bool pet_family_nvs_open(pet_family_nvs_t *backend, pet_family_store_t *store, pet_family_t *family);
/** Release the handle after the store is no longer used. Never erases keys. */
void pet_family_nvs_close(pet_family_nvs_t *backend);
