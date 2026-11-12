/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 */
#include "flash_layout.h"
#include <string.h>
#include "esp_check.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_rom_crc.h"
#include "esp_rom_md5.h"
#include "sdgoods_board.h"

static const char *TAG = "layout";
#define SECTOR 0x1000u

void lt_md5(const uint8_t *data, size_t len, uint8_t out[16]) {
    md5_context_t ctx;
    esp_rom_md5_init(&ctx);
    esp_rom_md5_update(&ctx, data, len);
    esp_rom_md5_final(out, &ctx);
}

uint32_t lt_crc32(uint32_t seed, const uint8_t *data, size_t len) { return esp_rom_crc32_le(seed, data, len); }

static void *scratch(size_t len) { return heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }

/* Expected durations are generous upper bounds; the window ends as soon as the operation returns. */
static int window_ms(uint32_t erase_len, size_t write_len) {
    int ms = 60 + (int)(erase_len / SECTOR) * 60 + (int)(write_len / 1024) * 4;
    return ms > SDGOODS_TOUCH_FLASH_WIN_MAX_MS ? SDGOODS_TOUCH_FLASH_WIN_MAX_MS : ms;
}

esp_err_t flash_layout_erase(uint32_t offset, uint32_t len) {
    sdgoods_touch_flash_window(window_ms(len, 0));
    esp_err_t err = esp_flash_erase_region(NULL, offset, len);
    sdgoods_touch_flash_window_end();
    return err;
}

esp_err_t flash_layout_write(uint32_t offset, const void *data, size_t len) {
    sdgoods_touch_flash_window(window_ms(0, len));
    esp_err_t err = esp_flash_write(NULL, data, offset, len);
    sdgoods_touch_flash_window_end();
    return err;
}

/* Erase, write and read back one region; the comparison buffer is freed before returning. */
static esp_err_t replace_region(uint32_t offset, const uint8_t *data, size_t len) {
    ESP_RETURN_ON_ERROR(flash_layout_erase(offset, (uint32_t)len), TAG, "erase 0x%lx", (unsigned long)offset);
    ESP_RETURN_ON_ERROR(flash_layout_write(offset, data, len), TAG, "write 0x%lx", (unsigned long)offset);
    uint8_t *check = scratch(len);
    if (!check) return ESP_ERR_NO_MEM;
    esp_err_t err = esp_flash_read(NULL, check, offset, len);
    if (err == ESP_OK && memcmp(check, data, len) != 0) err = ESP_ERR_INVALID_CRC;
    free(check);
    if (err != ESP_OK) ESP_LOGE(TAG, "verify 0x%lx failed: %s", (unsigned long)offset, esp_err_to_name(err));
    return err;
}

esp_err_t flash_layout_load(lt_table_t *table) {
    uint8_t *sector = scratch(LT_TABLE_BYTES);
    if (!sector) return ESP_ERR_NO_MEM;
    esp_err_t err = esp_flash_read(NULL, sector, LT_TABLE_OFFSET, LT_TABLE_BYTES);
    lt_status_t status = err == ESP_OK ? lt_parse(table, sector) : LT_ERR_FORMAT;
    free(sector);
    if (err != ESP_OK) return err;
    if (status != LT_OK) {
        ESP_LOGE(TAG, "partition table is not the launcher layout (problem %d)", status);
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "pool 0x%lx..0x%lx, free %lu", (unsigned long)table->pool_start, (unsigned long)table->pool_end,
             (unsigned long)lt_free_bytes(table));
    return ESP_OK;
}

esp_err_t flash_layout_commit(const lt_table_t *table) {
    const esp_partition_t *guard = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "pt_guard");
    if (!guard || guard->size < LT_JOURNAL_RECORD_BYTES * LT_JOURNAL_RECORDS) {
        ESP_LOGE(TAG, "pt_guard partition missing; refusing to rewrite the table");
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t *record = scratch(LT_JOURNAL_RECORD_BYTES);
    if (!record) return ESP_ERR_NO_MEM;
    lt_journal_t journals[LT_JOURNAL_RECORDS];
    esp_err_t err = ESP_OK;
    for (int i = 0; i < LT_JOURNAL_RECORDS && err == ESP_OK; i++) {
        err = esp_partition_read(guard, i * LT_JOURNAL_RECORD_BYTES, record, LT_JOURNAL_HEADER_BYTES + LT_TABLE_BYTES);
        journals[i] = lt_journal_inspect(record);
    }
    int newest = -1;
    for (int i = 0; i < LT_JOURNAL_RECORDS; i++)
        if (journals[i].valid && (newest < 0 || journals[i].generation > journals[newest].generation)) newest = i;
    if (err == ESP_OK && newest < 0) {
        ESP_LOGE(TAG, "pt_guard holds no valid journal record; refusing to rewrite the table");
        err = ESP_ERR_INVALID_STATE;
    }
    if (err == ESP_OK) {
        int target = (newest + 1) % LT_JOURNAL_RECORDS;
        uint32_t generation = journals[newest].generation + 1;
        lt_journal_build(record, table->sector, generation, journals[newest].reserved);
        err = replace_region(guard->address + target * LT_JOURNAL_RECORD_BYTES, record, LT_JOURNAL_RECORD_BYTES);
        if (err == ESP_OK) err = replace_region(LT_TABLE_OFFSET, table->sector, LT_TABLE_BYTES);
        if (err == ESP_OK) ESP_LOGI(TAG, "table generation %lu committed (record %d)", (unsigned long)generation, target);
    }
    free(record);
    return err;
}

esp_err_t flash_layout_boot_slot(int index) {
    const esp_partition_t *otadata = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
    if (!otadata || otadata->size < 2 * SECTOR) return ESP_ERR_NOT_FOUND;
    uint8_t entry[LT_OTADATA_ENTRY_BYTES];
    uint32_t newest = 0;
    int newest_sector = -1;
    for (int i = 0; i < 2; i++) {
        uint32_t seq;
        ESP_RETURN_ON_ERROR(esp_partition_read(otadata, i * SECTOR, entry, sizeof entry), TAG, "read otadata");
        if (lt_otadata_read(entry, &seq) && seq > newest) { newest = seq; newest_sector = i; }
    }
    uint32_t seq = lt_otadata_next(newest, index);
    lt_otadata_build(entry, seq);
    int target = newest_sector == 0 ? 1 : 0;
    sdgoods_touch_flash_window(400);
    esp_err_t err = esp_partition_erase_range(otadata, target * SECTOR, SECTOR);
    if (err == ESP_OK) err = esp_partition_write(otadata, target * SECTOR, entry, sizeof entry);
    sdgoods_touch_flash_window_end();
    ESP_LOGI(TAG, "next boot ota_%d (sequence %lu): %s", index, (unsigned long)seq, esp_err_to_name(err));
    return err;
}
