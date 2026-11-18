/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Completed, serialized flash writes keep only their electrical settling interval.
 * Slow-read backoff is independent and must not be shortened by this window. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
typedef struct {atomic_int_least32_t until;} sdg_touch_window_t;
static inline void sdg_touch_window_begin(sdg_touch_window_t *window,int32_t now,int32_t budget)
{atomic_store_explicit(&window->until,(int32_t)((uint32_t)now+(uint32_t)budget),memory_order_release);}
static inline void sdg_touch_window_end(sdg_touch_window_t *window,int32_t now,int32_t settling)
{atomic_store_explicit(&window->until,(int32_t)((uint32_t)now+(uint32_t)settling),memory_order_release);}
static inline bool sdg_touch_window_busy(const sdg_touch_window_t *window,int32_t now)
{return (int32_t)((uint32_t)atomic_load_explicit(&window->until,memory_order_acquire)-(uint32_t)now)>0;}
