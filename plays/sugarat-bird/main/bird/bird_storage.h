/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Highest-score NVS owner; unavailable or malformed data is never erased or overwritten. */
#pragma once
#include "nvs.h"
#include <stdbool.h>
#include <stdint.h>
typedef struct { nvs_handle_t handle; uint32_t saved; bool open, writable, failed; } bird_storage_t;
/** Open the private namespace and read best; on read failure return zero for this run and disable writes.
 * @param storage Zero-initialized owner, closed by bird_storage_close.
 * @returns Stored best or zero when absent/unavailable; failed reports unavailable data.
 */
uint32_t bird_storage_open(bird_storage_t *storage);
/** Commit a higher score and read it back; failures retain the in-memory best and set failed.
 * @param storage Open owner; unreadable data remains write-protected.
 * @param best Highest score of this run, in 0..9999.
 * @returns True when already durable or a successful commit/readback confirms it.
 */
bool bird_storage_save(bird_storage_t *storage, uint32_t best);
/** Release an open NVS handle; idempotent.
 * @param storage Storage owner.
 */
void bird_storage_close(bird_storage_t *storage);
