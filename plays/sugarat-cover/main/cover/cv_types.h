/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Shared image limits and stable failures for device, radio and web consumers. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define CV_SIDE 360
#define CV_PIXELS (CV_SIDE * CV_SIDE)
#define CV_MAX_JPEG (160U * 1024U)
#define CV_MAX_PHOTOS 32
#define CV_ID_LENGTH 32
#define CV_PEERS 6
typedef enum { CV_EN, CV_ZH } cv_locale_t;
typedef enum {
    CV_OK, CV_INVALID, CV_JPEG, CV_FULL, CV_IO, CV_SETTINGS, CV_BUSY,
    CV_TIMEOUT, CV_CANCELLED, CV_AUTH, CV_RADIO, CV_MEMORY, CV_CORRUPT, CV_UNKNOWN,
    CV_ERROR_COUNT
} cv_error_t;
typedef struct { char id[CV_ID_LENGTH + 1]; uint32_t size; } cv_photo_t;
typedef struct {
    cv_photo_t photos[CV_MAX_PHOTOS]; unsigned count, selected;
    cv_locale_t locale; uint32_t revision; bool writable; cv_error_t error;
} cv_library_t;
/** @returns A stable English or Chinese error and safe recovery action. */
const char *cv_error_text(cv_locale_t locale, cv_error_t error);
