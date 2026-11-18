/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Shared immutable palette frames; only current output pixels need writable memory. */
#pragma once
#include <stddef.h>
#include <stdint.h>
typedef struct { const uint8_t *data; size_t size; } pet_art_frame_t;
/** Palette entries use canonical RGB565; the view converts to panel byte order. */
typedef struct { const uint16_t *palette; pet_art_frame_t frames[16]; } pet_art_character_t;
extern const pet_art_character_t pet_art_characters[6];
