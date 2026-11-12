/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 *
 * App icons made from community play covers. The operations console publishes every cover as a
 * 360 x 360 baseline JPEG; the ESP32-S3 ROM JPEG decoder reads it at 1/4 scale into a 90 x 90 icon,
 * and a 45 x 45 copy is averaged from that for grid tiles. Icons are kept in a PSRAM cache keyed by
 * play ID and, for installed plays, in /appdata/launcher/icons/<play>.icon.
 *
 * The install worker decodes, stores and removes icons; the LVGL thread only calls app_icon_find().
 * Cache entries are never freed, so a returned image stays valid for the firmware lifetime.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lvgl.h"

#define APP_ICON_LARGE 90
#define APP_ICON_SMALL 45

typedef struct {
    lv_img_dsc_t large;   /**< 90 x 90, for play and detail pages. */
    lv_img_dsc_t small;   /**< 45 x 45, for grid tiles. */
} app_icon_t;

/** Create the cache and the icon directory. Call once on the worker after appdata is mounted. */
void app_icon_init(void);

/** Decode a cover into the cache for `play_id` (memory only).
 * @returns False when the data is not a baseline JPEG the ROM decoder reads, or it is larger than 720 px. */
bool app_icon_decode(uint32_t play_id, const uint8_t *jpeg, size_t len);

/** Write the cached icon of `play_id` to its file so it survives restarts.
 * @returns False when no icon is cached or the file could not be written. */
bool app_icon_persist(uint32_t play_id);

/** Load the stored icon of `play_id` into the cache. @returns False when there is no valid file. */
bool app_icon_load(uint32_t play_id);

/** Delete the stored icon of `play_id`; a cached copy stays until the slot is reused. */
void app_icon_remove(uint32_t play_id);

/** @returns The cached icon of `play_id`, or NULL. Safe on any task. */
const app_icon_t *app_icon_find(uint32_t play_id);
