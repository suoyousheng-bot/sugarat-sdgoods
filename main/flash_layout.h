/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 *
 * Flash access for the launcher layout. Every write happens inside a touch flash window.
 * Callers run on the install worker; the LVGL thread never calls these functions.
 */
#pragma once
#include "esp_err.h"
#include "slot_table.h"

/** Read and parse the live partition table.
 * @returns ESP_ERR_INVALID_STATE when the table is not the launcher 2.0 layout. */
esp_err_t flash_layout_load(lt_table_t *table);

/** Persist `table`: write the next journal generation to the older pt_guard record, verify it,
 * then rewrite the primary table sector and verify it. If power fails after the journal write,
 * the bootloader restores the table from the journal.
 * @returns ESP_ERR_INVALID_STATE when pt_guard holds no valid record to continue from. */
esp_err_t flash_layout_commit(const lt_table_t *table);

/** Select ota_`index` for the next boot by writing a new otadata entry. Does not restart. */
esp_err_t flash_layout_boot_slot(int index);

/** Erase `len` bytes at `offset` (4 KiB aligned) inside a touch flash window. */
esp_err_t flash_layout_erase(uint32_t offset, uint32_t len);

/** Write `len` bytes at `offset` inside a touch flash window. */
esp_err_t flash_layout_write(uint32_t offset, const void *data, size_t len);
