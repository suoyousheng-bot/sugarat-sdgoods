/* SPDX-License-Identifier: Apache-2.0 */
/** Private helpers shared by the Sugarat Watch sources. */
#ifndef SW_INTERNAL_H
#define SW_INTERNAL_H

#include "sugarat_watch.h"

extern const char *const sw_icon_glyphs[SW_ICON_COUNT];

/** @returns A plain object without theme styles, scrolling or click handling. */
lv_obj_t *sw_plain(lv_obj_t *parent);

/** @returns The icon font for a size. */
const lv_font_t *sw_icon_font(sw_size_t size);

/** Make obj a flex container with the given flow, centered on both axes, and the given gap. */
void sw_flex(lv_obj_t *obj, lv_flex_flow_t flow, lv_coord_t gap);

/** Make obj tappable: clickable, bubbles events, shrinks while pressed. */
void sw_tappable(lv_obj_t *obj);

/** @returns A readable text tone on top of a tone fill. */
sw_tone_t sw_on_tone(sw_tone_t tone);

/** Allocate state freed when obj is deleted; stored as obj's user data. */
void *sw_state(lv_obj_t *obj, size_t size);

#endif
