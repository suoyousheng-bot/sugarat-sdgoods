/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Portable versioned records for alternating NVS blobs; invalid records never mutate the pet. */
#pragma once
#include "pet_model.h"
#include <stddef.h>
#define PET_SAVE_BYTES 56
typedef enum { PET_RECORD_OK, PET_RECORD_CORRUPT, PET_RECORD_VERSION } pet_record_result_t;

/** Encode stats and a monotonic sequence into a fixed little-endian record. */
void pet_save_encode(const pet_model_t *model, uint32_t sequence, uint8_t out[PET_SAVE_BYTES]);
/** Validate all durable fields, then restore the pet and return its sequence. */
pet_record_result_t pet_save_decode(pet_model_t *model, const uint8_t *data, size_t size, uint32_t *sequence);
