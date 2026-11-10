/* SPDX-License-Identifier: Apache-2.0 */
/** Private helpers shared by the Sugarat Game sources. */
#ifndef SG_INTERNAL_H
#define SG_INTERNAL_H

#include "sugarat_game.h"

#define SG_INK 0x0B0C16 /* Outline and hard shadow color. */
#define SG_DEEP 0x22305E /* FRAME window fill. */

extern const char *const sg_icon_glyphs[SG_ICON_COUNT];

/** @returns A plain object without theme styles, scrolling or click handling. */
lv_obj_t *sg_plain(lv_obj_t *parent);
/** Make obj a flex container centered on both axes with the given gap. */
void sg_flex(lv_obj_t *obj, lv_flex_flow_t flow, lv_coord_t gap);
/** Make obj tappable: clickable, bubbling events, dimmed while disabled. */
void sg_tappable(lv_obj_t *obj);
/** @returns A readable text tone on top of a tone fill. */
sg_tone_t sg_on_tone(sg_tone_t tone);
/** Allocate zeroed state freed when obj is deleted; stored as obj's user data. */
void *sg_state(lv_obj_t *obj, size_t size);
/** @returns The pixel icon font for a size. */
const lv_font_t *sg_icon_font(sg_size_t size);

/** @returns c mixed toward white (amount > 0) or the ink color (amount < 0), amount in -255..255. */
lv_color_t sg_shade(lv_color_t c, int amount);
/** Fill a rectangle given by inclusive corners. */
void sg_rect(lv_draw_ctx_t *ctx, lv_coord_t x1, lv_coord_t y1, lv_coord_t x2, lv_coord_t y2, lv_color_t color);
/** Draw a pixel box into area. */
void sg_draw_box(lv_draw_ctx_t *ctx, const lv_area_t *area, sg_box_style_t style, sg_tone_t tone);

/** Blocks laid out on a circle; angles in degrees clockwise from 3 o'clock. */
typedef struct {
    int16_t start, sweep;
    uint8_t count, filled, block;
    int8_t head; /* Spinner head index, or -1. */
    sg_tone_t tone;
} sg_blocks_t;

/** @returns A size x size object drawing a block ring; its sg_blocks_t is the object's user data. */
lv_obj_t *sg_block_ring(lv_obj_t *parent, lv_coord_t size, const sg_blocks_t *blocks);

#endif
