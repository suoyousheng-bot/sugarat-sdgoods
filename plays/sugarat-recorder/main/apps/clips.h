/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include "esp_err.h"

#include <stddef.h>
#include <stdint.h>

#define CLIP_LIMIT 128
#define CLIP_MAX_MS 90000u

typedef struct {
    uint32_t id;
    uint32_t samples;
} voice_clip_t;

/* The file worker owns the catalog until it returns its result to the UI. */
typedef struct {
    char path[64];
    voice_clip_t clips[CLIP_LIMIT];
    size_t count;
    uint32_t next_id;
} clip_store_t;

esp_err_t clips_open(clip_store_t *store);
esp_err_t clips_save(clip_store_t *store, const int16_t *pcm, size_t samples);
esp_err_t clips_read(const clip_store_t *store, voice_clip_t clip, uint8_t **wav, size_t *bytes);
esp_err_t clips_delete(clip_store_t *store, uint32_t id);
