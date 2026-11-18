/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "pet_family_nvs.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *keys[] = {"family_a", "family_b"};
static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static bool space_available(void)
{
    nvs_stats_t stats;
    /* Leave 128 available entries to other users, in addition to NVS's own reserve.
     * Budget for a maximum next record, including blob metadata. */
    return nvs_get_stats(NULL, &stats) == ESP_OK && stats.available_entries >= (PET_NVS_RECORD_MAX + 31) / 32 + 4 + 128;
}
static bool future(const uint8_t *bytes, size_t size)
{ return size >= 8 && !memcmp(bytes, "SGF", 3) && (bytes[3] != '2' || get32(bytes + 4) != 2); }
static pet_family_result_t unpack(pet_family_t *family, uint32_t *sequence, uint8_t full[PET_FAMILY_BYTES], size_t size)
{
    if (future(full, size)) return PET_FAMILY_VERSION;
    if (size < 37 || size > PET_NVS_RECORD_MAX || memcmp(full, "SGF2", 4)) return PET_FAMILY_CORRUPT;
    uint32_t used = get32(full + 16);
    if (used < 33 || used + 4 != size) return PET_FAMILY_CORRUPT;
    uint8_t checksum[4]; memcpy(checksum, full + used, 4);
    memset(full + used, 0, PET_FAMILY_BYTES - used);
    memcpy(full + PET_FAMILY_BYTES - 4, checksum, 4);
    return pet_family_decode(family, sequence, full, PET_FAMILY_BYTES);
}
static bool save(void *context, pet_family_store_t *s, const pet_family_t *family)
{
    pet_family_nvs_t *nvs = context;
    if (!space_available()) { s->writable = false; s->notice = "Shared storage full: save stopped"; return false; }
    uint8_t *full = malloc(PET_FAMILY_BYTES), *check = malloc(PET_NVS_RECORD_MAX);
    bool ok = full && check && s->sequence < UINT32_MAX && pet_family_encode(family, s->sequence + 1u, full);
    size_t size = ok ? get32(full + 16) + 4u : 0;
    ok = ok && size <= PET_NVS_RECORD_MAX;
    if (ok) {
        memcpy(full + size - 4, full + PET_FAMILY_BYTES - 4, 4);
        esp_err_t err = nvs_set_blob(nvs->handle, keys[s->next_slot], full, size);
        if (err == ESP_OK) err = nvs_commit(nvs->handle);
        size_t read_size = PET_NVS_RECORD_MAX;
        if (err == ESP_OK) err = nvs_get_blob(nvs->handle, keys[s->next_slot], check, &read_size);
        ok = err == ESP_OK && read_size == size && !memcmp(full, check, size);
    }
    free(full); free(check);
    if (!ok) { s->writable = false; s->notice = "Save unconfirmed: restart to recover"; return false; }
    s->sequence++; s->next_slot ^= 1u; s->loaded = nvs->present = true; s->notice = NULL;
    if (s->sequence == UINT32_MAX) { s->writable = false; s->notice = "Family save limit reached"; }
    return true;
}
bool pet_family_nvs_open(pet_family_nvs_t *nvs, pet_family_store_t *s, pet_family_t *family)
{
    memset(nvs, 0, sizeof(*nvs)); memset(s, 0, sizeof(*s));
    s->backend_context = nvs; s->backend_save = save;
    if (nvs_open("sugarat_pet", NVS_READWRITE, &nvs->handle) != ESP_OK) {
        nvs->present = true; s->notice = "Family storage unreadable"; return false;
    }
    nvs->opened = true;
    uint8_t *bytes = malloc(PET_FAMILY_BYTES); pet_family_t *candidate = malloc(sizeof(*candidate));
    if (!bytes || !candidate) { free(bytes); free(candidate); nvs->present = true; s->notice = "Family memory unavailable"; return false; }
    bool bad = false, unknown = false;
    for (unsigned slot = 0; slot < 2; slot++) {
        size_t size = 0; esp_err_t err = nvs_get_blob(nvs->handle, keys[slot], NULL, &size);
        if (err == ESP_ERR_NVS_NOT_FOUND) continue;
        nvs->present = true;
        if (err != ESP_OK || size > PET_NVS_RECORD_MAX || size < 8) { unknown = true; continue; }
        err = nvs_get_blob(nvs->handle, keys[slot], bytes, &size);
        if (err != ESP_OK) { unknown = true; continue; }
        uint32_t sequence = 0; pet_family_result_t result = unpack(candidate, &sequence, bytes, size);
        if (result == PET_FAMILY_VERSION) { unknown = true; continue; }
        if (result != PET_FAMILY_OK) { bad = true; continue; }
        if (!s->loaded || sequence > s->sequence) {
            *family = *candidate; s->sequence = sequence; s->next_slot = slot ^ 1u; s->loaded = true;
        }
    }
    free(bytes); free(candidate);
    s->writable = !unknown && (!bad || s->loaded) && s->sequence < UINT32_MAX && space_available();
    s->notice = unknown ? "Newer family: update firmware" : bad && !s->loaded ? "Family unreadable: writes stopped" :
        !s->writable ? "Shared storage full: save stopped" : bad ? "Recovered previous family" : NULL;
    ESP_LOGI("pet_save", "Family NVS ready: loaded=%d writable=%d sequence=%lu", s->loaded, s->writable, (unsigned long)s->sequence);
    return s->writable;
}
void pet_family_nvs_close(pet_family_nvs_t *nvs)
{ if (nvs->opened) nvs_close(nvs->handle); memset(nvs, 0, sizeof(*nvs)); }
