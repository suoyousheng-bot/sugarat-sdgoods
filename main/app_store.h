/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 *
 * Installed applications in the launcher slot pool.
 * The partition table and the images on flash are the source of truth; /appdata/launcher/apps.tsv
 * only adds the community identity (play ID, title, category) that an image does not carry.
 * Mutating calls run on the install worker only. app_store_snapshot() is safe from any task.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define APP_STORE_MAX 16

typedef enum {
    APP_OK = 0,
    APP_ERR_LAYOUT,    /**< Flash is not the launcher layout, or the table could not be read. */
    APP_ERR_SPACE,     /**< No pool gap is large enough. */
    APP_ERR_SLOTS,     /**< All 16 slots are used. */
    APP_ERR_FLASH,     /**< A flash erase, write or table commit failed. */
    APP_ERR_IMAGE,     /**< The data is not a valid ESP32-S3 application image. */
    APP_ERR_HASH,      /**< SHA-256 of the received bytes differs from the published value. */
    APP_ERR_CONFLICT,  /**< Another installed app already uses this application identity. */
    APP_ERR_SIZE,      /**< More or fewer bytes than announced. */
    APP_ERR_NO_MEM,
} app_problem_t;

typedef struct {
    int slot;                /**< ota_N index. */
    uint32_t offset, size;   /**< Slot geometry. */
    bool damaged;            /**< Slot holds no verifiable image; only removal is offered. */
    char project[33];        /**< esp_app_desc_t.project_name; also the /appdata directory. */
    char version[33];        /**< esp_app_desc_t.version. */
    uint32_t play_id;        /**< Community play ID; 0 when installed without the launcher. */
    char title[64];          /**< Community title, or the project name. */
    char category[24];       /**< Community category ID, may be empty. */
} app_info_t;

typedef struct {
    uint32_t pool, free, largest;
} app_space_t;

/** Description of an image about to be streamed in. */
typedef struct {
    uint32_t play_id;
    char title[64];
    char category[24];
    uint32_t image_bytes;
    uint8_t sha256[32];
    int replaces_slot;       /**< Installed slot of the same play (update), or -1. */
    bool release_first;      /**< Release `replaces_slot` (keeping its data) before allocating, when space is short. */
} app_install_t;

/** Mount appdata, read the table and every slot, verify slots without launcher records.
 * @returns APP_ERR_LAYOUT when the device is not in the launcher layout; installs are then refused. */
app_problem_t app_store_init(void);

/** Copy installed apps (slot order) and pool usage.
 * @returns Number of apps written to `out`. */
int app_store_snapshot(app_info_t *out, int cap, app_space_t *space);

typedef enum {
    APP_STORE_LOADING,      /**< app_store_init() has not finished. */
    APP_STORE_READY,
    APP_STORE_UNAVAILABLE,  /**< Not the launcher layout; listing and installs are refused. */
} app_store_state_t;

/** @returns Current availability; safe from any task. */
app_store_state_t app_store_state(void);

/** Bytes a slot for `image_bytes` needs. */
uint32_t app_store_slot_bytes(uint32_t image_bytes);

/** Allocate a slot and commit it to the table. Fails before writing any image byte. */
app_problem_t app_store_install_begin(const app_install_t *request);
/** Append the next chunk; erases ahead in 64 KiB blocks. */
app_problem_t app_store_install_write(const uint8_t *data, size_t len);
/** Check size, SHA-256, image validity and identity, record the app and release the replaced slot.
 * On failure the new slot is released. */
app_problem_t app_store_install_end(app_info_t *installed);
/** Release the slot of an unfinished install. */
void app_store_install_abort(void);

/** Remove an app: invalidate its image, release its slot and, unless `keep_data`, delete /appdata/<project>. */
app_problem_t app_store_uninstall(int slot, bool keep_data);

/** Select the slot for the next boot. The caller restarts the device after the screen shows feedback. */
app_problem_t app_store_select_boot(int slot);
