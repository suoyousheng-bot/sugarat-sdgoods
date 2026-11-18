/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Decode bounded, baseline 360px JPEGs with the foundation's maintained TJpgDec. */
#pragma once
#include "cv_types.h"
/** Decode to LVGL's configured RGB565 byte order; pixels NULL performs full validation only.
 * @returns CV_JPEG for malformed, unsupported or differently sized images. */
cv_error_t cv_jpeg(const uint8_t *bytes, size_t size, uint16_t *pixels);
