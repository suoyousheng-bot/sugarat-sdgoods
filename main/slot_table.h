/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 *
 * In-memory model of the SDGOODS launcher 2.0 flash layout: the partition table sector at 0x8000,
 * the A/B table journal in the `pt_guard` partition and the OTA boot selection records.
 * No flash access happens here; flash_layout.c reads and writes the bytes this module produces.
 *
 * Layout facts (read from a launcher 2.0 badge on 2026-11-11):
 * - ota_0..ota_15 always exist. An empty slot is a size-0 row whose offset is the pool end.
 * - Installed slots live in the pool between the end of the factory partition and the first
 *   non-application partition after it, start on 64 KiB boundaries and include a 0xE000 cover reserve.
 * - Journal record: "ALPTJ001", generation, body length 0x1000, CRC-32 of the body, CRC-32 of the
 *   first 20 header bytes, two preserved words, then the full table sector.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LT_TABLE_OFFSET         0x8000u
#define LT_TABLE_BYTES          0x1000u
#define LT_TABLE_MAX_BYTES      0xC00u
#define LT_ROW_BYTES            32u
#define LT_OTA_SLOTS            16
#define LT_SLOT_ALIGN           0x10000u
#define LT_SLOT_RESERVE         0xE000u
#define LT_JOURNAL_HEADER_BYTES 32u
#define LT_JOURNAL_RECORD_BYTES 0x2000u
#define LT_JOURNAL_RECORDS      2
#define LT_OTADATA_ENTRY_BYTES  32u

typedef enum {
    LT_OK = 0,
    LT_ERR_FORMAT,   /**< Not a partition table, or no MD5 row. */
    LT_ERR_MD5,      /**< MD5 row does not match the rows. */
    LT_ERR_LAYOUT,   /**< Not the launcher 2.0 layout (factory, 16 OTA rows, pool). */
    LT_ERR_NO_SPACE, /**< No pool gap is large enough. */
    LT_ERR_NO_SLOT,  /**< All 16 OTA rows are in use. */
    LT_ERR_ARG,
} lt_status_t;

/** One OTA slot; size 0 means the slot is empty. */
typedef struct {
    uint32_t offset;
    uint32_t size;
} lt_slot_t;

/** Parsed table sector. The sector bytes are the source of truth; the indexes point into them. */
typedef struct {
    uint8_t sector[LT_TABLE_BYTES];
    int rows;                    /**< Partition rows before the MD5 row. */
    int ota_row[LT_OTA_SLOTS];   /**< Row index of ota_N. */
    uint32_t pool_start, pool_end;
} lt_table_t;

/** Journal record header facts. */
typedef struct {
    bool valid;
    uint32_t generation;
    uint32_t reserved[2]; /**< Header words 6 and 7, preserved into the next generation. */
} lt_journal_t;

/** Platform hooks: MD5 digest and zlib-compatible CRC-32 (`seed` is the running CRC, 0 for a new one). */
void lt_md5(const uint8_t *data, size_t len, uint8_t out[16]);
uint32_t lt_crc32(uint32_t seed, const uint8_t *data, size_t len);

/** Parse a table sector and check it is the launcher 2.0 layout.
 * @param table - Receives a copy of the sector and its indexes.
 * @param sector - LT_TABLE_BYTES bytes read from LT_TABLE_OFFSET.
 * @returns LT_OK, or the first problem found. */
lt_status_t lt_parse(lt_table_t *table, const uint8_t *sector);

/** @returns ota_N's offset and size; size 0 when empty. */
lt_slot_t lt_slot(const lt_table_t *table, int index);

/** @returns Slot bytes needed for an image: image plus cover reserve, rounded up to LT_SLOT_ALIGN. */
uint32_t lt_slot_bytes_for(uint32_t image_bytes);

/** Choose the lowest empty OTA row and the first pool gap that holds `slot_bytes`.
 * @param slot_bytes - Multiple of LT_SLOT_ALIGN.
 * @param index - Receives the OTA index.
 * @param offset - Receives the slot start.
 * @returns LT_OK, LT_ERR_NO_SLOT or LT_ERR_NO_SPACE. */
lt_status_t lt_allocate(const lt_table_t *table, uint32_t slot_bytes, int *index, uint32_t *offset);

/** Point ota_N at a slot (or empty it with size 0) and rewrite the MD5 row.
 * @returns LT_ERR_ARG when the slot leaves the pool or overlaps another slot. */
lt_status_t lt_set_slot(lt_table_t *table, int index, uint32_t offset, uint32_t size);

/** @returns Total pool bytes not used by any slot. */
uint32_t lt_free_bytes(const lt_table_t *table);

/** @returns Largest slot size lt_allocate() can currently place. */
uint32_t lt_largest_gap(const lt_table_t *table);

/** Read one journal record header and check both CRCs against its body.
 * @param record - At least LT_JOURNAL_HEADER_BYTES + LT_TABLE_BYTES bytes. */
lt_journal_t lt_journal_inspect(const uint8_t *record);

/** Build a full journal record for `sector` (LT_JOURNAL_RECORD_BYTES, unused bytes 0xFF). */
void lt_journal_build(uint8_t *record, const uint8_t *sector, uint32_t generation, const uint32_t reserved[2]);

/** @returns True when a 32-byte otadata entry is valid; `sequence` receives its sequence number. */
bool lt_otadata_read(const uint8_t *entry, uint32_t *sequence);

/** @returns The smallest sequence above `current` (0 = none) that the bootloader maps to ota_`index`. */
uint32_t lt_otadata_next(uint32_t current, int index);

/** Build a 32-byte otadata entry marked valid for `sequence`. */
void lt_otadata_build(uint8_t *entry, uint32_t sequence);
