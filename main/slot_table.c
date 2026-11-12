/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 */
#include "slot_table.h"
#include <string.h>

#define ROW_MAGIC0 0xAA
#define ROW_MAGIC1 0x50
#define MD5_MAGIC  0xEB
#define TYPE_APP   0x00
#define SUB_FACTORY 0x00
#define SUB_OTA0   0x10
#define JOURNAL_MAGIC "ALPTJ001"
#define OTA_STATE_VALID 2u

static uint32_t get32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static void put32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static uint8_t *row(lt_table_t *t, int i) { return t->sector + i * LT_ROW_BYTES; }
static const uint8_t *crow(const lt_table_t *t, int i) { return t->sector + i * LT_ROW_BYTES; }
static uint32_t align_up(uint32_t v, uint32_t a) { return (v + a - 1) / a * a; }

static void write_md5(lt_table_t *t) {
    uint8_t *md5 = row(t, t->rows);
    lt_md5(t->sector, t->rows * LT_ROW_BYTES, md5 + 16);
}

lt_status_t lt_parse(lt_table_t *t, const uint8_t *sector) {
    memcpy(t->sector, sector, LT_TABLE_BYTES);
    int rows = 0;
    while ((rows + 1) * LT_ROW_BYTES <= LT_TABLE_MAX_BYTES && row(t, rows)[0] == ROW_MAGIC0 && row(t, rows)[1] == ROW_MAGIC1) rows++;
    if (rows == 0 || (rows + 1) * LT_ROW_BYTES > LT_TABLE_MAX_BYTES) return LT_ERR_FORMAT;
    const uint8_t *md5 = row(t, rows);
    if (md5[0] != MD5_MAGIC || md5[1] != MD5_MAGIC) return LT_ERR_FORMAT;
    t->rows = rows;
    uint8_t digest[16];
    lt_md5(t->sector, rows * LT_ROW_BYTES, digest);
    if (memcmp(digest, md5 + 16, 16) != 0) return LT_ERR_MD5;

    int factory = -1;
    for (int i = 0; i < LT_OTA_SLOTS; i++) t->ota_row[i] = -1;
    for (int i = 0; i < rows; i++) {
        const uint8_t *r = crow(t, i);
        if (r[2] != TYPE_APP) continue;
        if (r[3] == SUB_FACTORY) factory = i;
        else if (r[3] >= SUB_OTA0 && r[3] < SUB_OTA0 + LT_OTA_SLOTS) {
            if (t->ota_row[r[3] - SUB_OTA0] >= 0) return LT_ERR_LAYOUT;
            t->ota_row[r[3] - SUB_OTA0] = i;
        }
    }
    if (factory < 0) return LT_ERR_LAYOUT;
    for (int i = 0; i < LT_OTA_SLOTS; i++) if (t->ota_row[i] < 0) return LT_ERR_LAYOUT;

    t->pool_start = align_up(get32(crow(t, factory) + 4) + get32(crow(t, factory) + 8), LT_SLOT_ALIGN);
    t->pool_end = 0;
    for (int i = 0; i < rows; i++) {
        const uint8_t *r = crow(t, i);
        uint32_t offset = get32(r + 4), size = get32(r + 8);
        if (size == 0 || offset < t->pool_start) continue;
        if (r[2] == TYPE_APP && r[3] >= SUB_OTA0 && r[3] < SUB_OTA0 + LT_OTA_SLOTS) continue;
        if (t->pool_end == 0 || offset < t->pool_end) t->pool_end = offset;
    }
    if (t->pool_end <= t->pool_start) return LT_ERR_LAYOUT;
    for (int i = 0; i < LT_OTA_SLOTS; i++) {
        lt_slot_t s = lt_slot(t, i);
        if (s.size == 0) continue;
        if (s.offset < t->pool_start || s.offset + s.size > t->pool_end || s.offset % LT_SLOT_ALIGN) return LT_ERR_LAYOUT;
        for (int j = 0; j < i; j++) {
            lt_slot_t o = lt_slot(t, j);
            if (o.size && s.offset < o.offset + o.size && o.offset < s.offset + s.size) return LT_ERR_LAYOUT;
        }
    }
    return LT_OK;
}

lt_slot_t lt_slot(const lt_table_t *t, int index) {
    const uint8_t *r = crow(t, t->ota_row[index]);
    return (lt_slot_t){ get32(r + 4), get32(r + 8) };
}

uint32_t lt_slot_bytes_for(uint32_t image_bytes) {
    return align_up(image_bytes + LT_SLOT_RESERVE, LT_SLOT_ALIGN);
}

/* Used slots sorted by offset; returns their count. */
static int used_slots(const lt_table_t *t, lt_slot_t used[LT_OTA_SLOTS]) {
    int n = 0;
    for (int i = 0; i < LT_OTA_SLOTS; i++) {
        lt_slot_t s = lt_slot(t, i);
        if (!s.size) continue;
        int j = n++;
        while (j > 0 && used[j - 1].offset > s.offset) { used[j] = used[j - 1]; j--; }
        used[j] = s;
    }
    return n;
}

lt_status_t lt_allocate(const lt_table_t *t, uint32_t slot_bytes, int *index, uint32_t *offset) {
    if (slot_bytes == 0 || slot_bytes % LT_SLOT_ALIGN) return LT_ERR_ARG;
    int free_index = -1;
    for (int i = 0; i < LT_OTA_SLOTS && free_index < 0; i++) if (lt_slot(t, i).size == 0) free_index = i;
    if (free_index < 0) return LT_ERR_NO_SLOT;
    lt_slot_t used[LT_OTA_SLOTS];
    int n = used_slots(t, used);
    uint32_t cursor = t->pool_start;
    for (int i = 0; i <= n; i++) {
        uint32_t limit = i < n ? used[i].offset : t->pool_end;
        if (limit >= cursor && limit - cursor >= slot_bytes) {
            *index = free_index;
            *offset = cursor;
            return LT_OK;
        }
        if (i < n) cursor = align_up(used[i].offset + used[i].size, LT_SLOT_ALIGN);
    }
    return LT_ERR_NO_SPACE;
}

lt_status_t lt_set_slot(lt_table_t *t, int index, uint32_t offset, uint32_t size) {
    if (index < 0 || index >= LT_OTA_SLOTS) return LT_ERR_ARG;
    if (size == 0) offset = t->pool_end;
    else {
        if (offset < t->pool_start || offset > t->pool_end || size > t->pool_end - offset) return LT_ERR_ARG;
        if (offset % LT_SLOT_ALIGN || size % LT_SLOT_ALIGN) return LT_ERR_ARG;
        for (int i = 0; i < LT_OTA_SLOTS; i++) {
            lt_slot_t o = lt_slot(t, i);
            if (i != index && o.size && offset < o.offset + o.size && o.offset < offset + size) return LT_ERR_ARG;
        }
    }
    uint8_t *r = row(t, t->ota_row[index]);
    put32(r + 4, offset);
    put32(r + 8, size);
    write_md5(t);
    return LT_OK;
}

uint32_t lt_free_bytes(const lt_table_t *t) {
    uint32_t used = 0;
    for (int i = 0; i < LT_OTA_SLOTS; i++) used += lt_slot(t, i).size;
    return t->pool_end - t->pool_start - used;
}

uint32_t lt_largest_gap(const lt_table_t *t) {
    lt_slot_t used[LT_OTA_SLOTS];
    int n = used_slots(t, used);
    uint32_t cursor = t->pool_start, best = 0;
    for (int i = 0; i <= n; i++) {
        uint32_t limit = i < n ? used[i].offset : t->pool_end;
        if (limit > cursor && limit - cursor > best) best = limit - cursor;
        if (i < n) cursor = align_up(used[i].offset + used[i].size, LT_SLOT_ALIGN);
    }
    return best;
}

lt_journal_t lt_journal_inspect(const uint8_t *record) {
    lt_journal_t j = {0};
    if (memcmp(record, JOURNAL_MAGIC, 8) != 0 || get32(record + 12) != LT_TABLE_BYTES) return j;
    if (get32(record + 16) != lt_crc32(0, record + LT_JOURNAL_HEADER_BYTES, LT_TABLE_BYTES)) return j;
    if (get32(record + 20) != lt_crc32(0, record, 20)) return j;
    j.valid = true;
    j.generation = get32(record + 8);
    j.reserved[0] = get32(record + 24);
    j.reserved[1] = get32(record + 28);
    return j;
}

void lt_journal_build(uint8_t *record, const uint8_t *sector, uint32_t generation, const uint32_t reserved[2]) {
    memset(record, 0xFF, LT_JOURNAL_RECORD_BYTES);
    memcpy(record, JOURNAL_MAGIC, 8);
    put32(record + 8, generation);
    put32(record + 12, LT_TABLE_BYTES);
    put32(record + 16, lt_crc32(0, sector, LT_TABLE_BYTES));
    put32(record + 20, lt_crc32(0, record, 20));
    put32(record + 24, reserved[0]);
    put32(record + 28, reserved[1]);
    memcpy(record + LT_JOURNAL_HEADER_BYTES, sector, LT_TABLE_BYTES);
}

/* ESP-IDF esp_ota_select_entry_t: ota_seq, seq_label[20], ota_state, crc = crc32_le(UINT32_MAX, &ota_seq, 4). */
bool lt_otadata_read(const uint8_t *entry, uint32_t *sequence) {
    uint32_t seq = get32(entry);
    if (seq == 0xFFFFFFFFu || seq == 0) return false;
    if (get32(entry + 28) != lt_crc32(0xFFFFFFFFu, entry, 4)) return false;
    *sequence = seq;
    return true;
}

uint32_t lt_otadata_next(uint32_t current, int index) {
    uint32_t seq = (uint32_t)index + 1;
    while (seq <= current) seq += LT_OTA_SLOTS;
    return seq;
}

void lt_otadata_build(uint8_t *entry, uint32_t sequence) {
    memset(entry, 0xFF, LT_OTADATA_ENTRY_BYTES);
    put32(entry, sequence);
    put32(entry + 24, OTA_STATE_VALID);
    put32(entry + 28, lt_crc32(0xFFFFFFFFu, entry, 4));
}
