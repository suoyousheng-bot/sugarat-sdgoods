/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Packed original atlas playback without allocations or timers. */
#pragma once
#include "pet_family.h"
/** Resolve a real atlas frame from effective foreground animation time. */
unsigned pet_animation_frame(const pet_model_t *model, const pet_profile_t *profile);
/** Decode a validated generated RLE frame to caller-owned 96x96 indexed pixels. */
bool pet_animation_decode(pet_kind_t kind, unsigned frame, uint8_t pixels[96 * 96]);
