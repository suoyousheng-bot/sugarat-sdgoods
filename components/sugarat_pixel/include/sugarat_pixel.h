/* SPDX-License-Identifier: Apache-2.0 */
/**
 * Sugarat Pixel: a library of 32x32 pixel characters for LVGL 8.3.
 *
 * Each character stores one 4-bit indexed frame per mood plus a blink frame.
 * sp_avatar() draws a character at an integer scale with crisp pixel edges and
 * animates it (bob, blink, talk, hop). Call every function on the LVGL thread.
 */
#ifndef SUGARAT_PIXEL_H
#define SUGARAT_PIXEL_H

#include "lvgl.h"
#include "sp_chars.h"

typedef enum { SP_FAMILY_PEOPLE, SP_FAMILY_ANIMAL, SP_FAMILY_FANTASY, SP_FAMILY_COUNT } sp_family_t;

/** One character: palette[0] is transparent; frames are row-major, two pixels per byte, high nibble first. */
typedef struct {
    const char *id;
    const char *name;
    sp_family_t family;
    const uint32_t *palette;
    const uint8_t (*frames)[SP_FRAME_BYTES];
} sp_char_t;

extern const sp_char_t sp_characters[SP_CHAR_COUNT];

/** Return a built-in character with static lifetime. */
const sp_char_t *sp_char_get(sp_char_id_t id);

/** Create an animated character of (32 * scale) pixels square; scale is 1..10. */
lv_obj_t *sp_avatar(lv_obj_t *parent, sp_char_id_t id, uint8_t scale, sp_mood_t mood);

/** Show another character, keeping scale, mood and animation. */
void sp_avatar_set_char(lv_obj_t *avatar, sp_char_id_t id);

/** Change the expression and its animation. */
void sp_avatar_set_mood(lv_obj_t *avatar, sp_mood_t mood);

/** Start or stop the built-in animation; a stopped avatar shows its mood frame at rest. */
void sp_avatar_set_animated(lv_obj_t *avatar, bool animated);

#endif
