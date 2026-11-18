/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Highest-score persistence, with a visible failure flag and no destructive recovery. */
#include "bird_storage.h"
#include "sdgoods_nvs.h"
#include "esp_log.h"
uint32_t bird_storage_open(bird_storage_t *s)
{
    sdgoods_nvs_ensure();
    esp_err_t err = nvs_open("sugarat_bird", NVS_READWRITE, &s->handle);
    if (err != ESP_OK) { s->failed = true; return 0; }
    s->open = true;
    err = nvs_get_u32(s->handle, "best", &s->saved);
    if (err == ESP_ERR_NVS_NOT_FOUND) s->saved = 0;
    else if (err != ESP_OK || s->saved > 9999) {
        s->saved = 0; s->failed = true; return 0;
    }
    s->writable = true;
    return s->saved;
}
bool bird_storage_save(bird_storage_t *s, uint32_t best)
{
    if (!s->writable) return false;
    if (best <= s->saved) return true;
    esp_err_t err = nvs_set_u32(s->handle, "best", best);
    if (err == ESP_OK) err = nvs_commit(s->handle);
    uint32_t readback = 0;
    if (err == ESP_OK) err = nvs_get_u32(s->handle, "best", &readback);
    if (err == ESP_OK && readback == best) { s->saved = best; s->failed = false; return true; }
    s->failed = true;
    ESP_LOGE("sugarat_bird", "Best score not confirmed saved; play continues. Retry after the next run.");
    return false;
}
void bird_storage_close(bird_storage_t *s)
{
    if (s->open) nvs_close(s->handle);
    s->open = s->writable = false;
}
