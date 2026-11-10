/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 */
#pragma once
#include <stddef.h>

/** A selectable US market. Empty id selects the nationwide directory. */
typedef struct {
    const char *id, *label, *city, *state, *timezone;
} radio_region_t;

/** Return the number of markets, including nationwide. */
size_t radio_region_count(void);
/** Return a market by index, or NULL for an invalid index. */
const radio_region_t *radio_region_at(size_t index);
/** Resolve a saved market id; missing or retired ids select nationwide. */
const radio_region_t *radio_region_find(const char *id);
