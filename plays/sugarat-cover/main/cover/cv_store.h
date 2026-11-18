/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Serial, caller-locked photo store. JPEG publication precedes catalogue visibility. */
#pragma once
#include "cv_types.h"
typedef bool (*cv_settings_write_t)(void *user, const char *current, cv_locale_t locale);
typedef struct {
    char directory[192], current[CV_ID_LENGTH+1]; cv_library_t library;
    cv_settings_write_t save_settings; void *user;
} cv_store_t;
/** Read the private directory without formatting or deleting user photos. Empty current uses its first image. */
cv_error_t cv_store_open(cv_store_t *store, const char *directory, const char *current,
                        cv_locale_t locale, cv_settings_write_t save_settings, void *user);
/** Validate, fsync and atomically publish a new photo; errors preserve every existing photo.
 * @param id Receives the content-derived photo identifier on success. */
cv_error_t cv_store_add(cv_store_t *store, const uint8_t *bytes, size_t size, char id[CV_ID_LENGTH+1]);
/** Read verified JPEG bytes into owned memory; caller frees *bytes after success. */
cv_error_t cv_store_read(cv_store_t *store, const char *id, uint8_t **bytes, size_t *size);
/** Read content-hash-verified bytes without the trial JPEG decode, for a caller that decodes them next and rejects invalid JPEG itself. */
cv_error_t cv_store_load(cv_store_t *store, const char *id, uint8_t **bytes, size_t *size);
/** Persist the new cover before changing the readable selection. */
cv_error_t cv_store_select(cv_store_t *store, const char *id);
/** Delete a non-current image. Current images must first be replaced explicitly. */
cv_error_t cv_store_delete(cv_store_t *store, const char *id);
/** Persist a language before publishing it. */
cv_error_t cv_store_locale(cv_store_t *store, cv_locale_t locale);
/** @returns Whether text is exactly one lowercase content identifier. */
bool cv_id_valid(const char *id);
