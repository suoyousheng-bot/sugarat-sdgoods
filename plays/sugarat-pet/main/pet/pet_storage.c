/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "pet_storage.h"
#include "esp_log.h"
#include "sdgoods_nvs.h"
#include <limits.h>
#include <string.h>

static const char *TAG = "pet_save";
static const char *keys[] = {"pet_a", "pet_b"};

void pet_storage_open(pet_storage_t *s, pet_model_t *m)
{
    memset(s, 0, sizeof(*s));
    sdgoods_nvs_ensure();
    esp_err_t err = nvs_open("sugarat_pet", NVS_READWRITE, &s->handle);
    if (err != ESP_OK) {
        s->notice = "Not saved - restart device";
        ESP_LOGE(TAG, "Cannot open pet storage: %s; running without saving", esp_err_to_name(err));
        return;
    }
    s->opened = true;
    bool found = false, damaged = false;
    for (unsigned i = 0; i < 2; i++) {
        uint8_t data[PET_SAVE_BYTES];
        size_t size = sizeof(data);
        err = nvs_get_blob(s->handle, keys[i], data, &size);
        if (err == ESP_ERR_NVS_NOT_FOUND) continue;
        if (err == ESP_ERR_NVS_INVALID_LENGTH || err == ESP_ERR_NVS_TYPE_MISMATCH) { damaged = true; continue; }
        if (err != ESP_OK) {
            s->notice = "Save unreadable - restart";
            ESP_LOGE(TAG, "Cannot read %s: %s; preserving saved records", keys[i], esp_err_to_name(err));
            return;
        }
        pet_model_t candidate = *m;
        uint32_t seq;
        pet_record_result_t result = pet_save_decode(&candidate, data, size, &seq);
        if (result == PET_RECORD_VERSION) {
            s->notice = "Save needs newer firmware";
            ESP_LOGE(TAG, "Unsupported pet save version; preserving both records");
            return;
        }
        if (result != PET_RECORD_OK) { damaged = true; continue; }
        if (!found || seq > s->sequence) {
            *m = candidate; s->sequence = seq; s->next_slot = 1 - i; found = true;
        }
    }
    if (damaged && !found) {
        s->notice = "Save damaged - not overwritten";
        ESP_LOGE(TAG, "No valid pet record; temporary companion cannot overwrite saves");
        return;
    }
    s->writable = true;
    m->dirty = !found;
    if (damaged) {
        s->notice = "Recovered previous save";
        ESP_LOGW(TAG, "Recovered valid pet record; damaged record will be replaced");
    }
    ESP_LOGI(TAG, "Pet storage ready; restored=%d sequence=%lu", found, (unsigned long)s->sequence);
}

bool pet_storage_flush(pet_storage_t *s, pet_model_t *m)
{
    if (!s->writable) return false;
    if (!m->dirty) return true;
    if (s->sequence == UINT32_MAX) { s->writable = false; s->notice = "Save limit - keep firmware"; return false; }
    uint8_t data[PET_SAVE_BYTES], check[PET_SAVE_BYTES];
    pet_save_encode(m, s->sequence + 1, data);
    esp_err_t err = nvs_set_blob(s->handle, keys[s->next_slot], data, sizeof(data));
    if (err == ESP_OK) err = nvs_commit(s->handle);
    size_t size = sizeof(check);
    if (err == ESP_OK) err = nvs_get_blob(s->handle, keys[s->next_slot], check, &size);
    if (err == ESP_OK && (size != sizeof(check) || memcmp(check, data, sizeof(check)))) err = ESP_FAIL;
    if (err != ESP_OK) {
        s->writable = false;
        s->notice = "Save failed - restart device";
        ESP_LOGE(TAG, "Save commit/readback failed: %s; previous record retained, latest outcome uncertain", esp_err_to_name(err));
        return false;
    }
    s->sequence++; s->next_slot = 1 - s->next_slot;
    s->notice = NULL;
    m->dirty = false; m->save_elapsed_ms = 0;
    return true;
}

void pet_storage_close(pet_storage_t *s)
{
    if (s->opened) nvs_close(s->handle);
    memset(s, 0, sizeof(*s));
}
