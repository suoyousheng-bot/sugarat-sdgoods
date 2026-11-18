/* SPDX-License-Identifier: MIT */
/** RGB565 software rendering owned by the fluid worker. */
#pragma once
#include "liquid.h"
#include <stdbool.h>
#include <stdint.h>
#define FLUID_THEMES 4

/** Base colour of each pure ingredient, indexed like bottle_liquid_t parts. */
extern const uint8_t fluid_theme_rgb[FLUID_THEMES][3];

/** Pour guidance drawn at the bottle mouth while two bottles are linked. */
typedef enum {
    FLUID_CUE_NONE,
    FLUID_CUE_CATCH, /**< Upright and ready: arrows point into the mouth. */
    FLUID_CUE_WAIT,  /**< Mouth down but the friend is not catching: arrows point out. */
} fluid_cue_t;

/** One frame's inputs plus the stream impact reported back to the solver owner. */
typedef struct {
    const bottle_liquid_t *liquid; /**< This bottle; drives the fill gauge. */
    const bottle_liquid_t *source; /**< Liquid shown in the mouth stream. */
    int flow;                      /**< -1 pouring out, +1 pouring in, 0 closed. */
    unsigned rate;                 /**< Pour rate 1..6 while flow is nonzero. */
    float down_x, down_y;          /**< Gravity in screen axes, 1.0 = 1 g in the screen plane. */
    fluid_cue_t cue;
    bool linked;                   /**< Keep the fill percentage visible. */
    uint32_t time_ms;              /**< Monotonic animation clock. */
    /** Output: the incoming stream touched liquid or glass during this frame. */
    bool impact;
    /** Output: last impact point in screen pixels and velocity in screen px/s. */
    float impact_x, impact_y, impact_vx, impact_vy;
} fluid_scene_t;

/** Initialize lookup tables before rendering. */
void fluid_render_init(void);

/** Mixed colour of a liquid; an empty bottle reports the first ingredient.
 * @param liquid Ingredient quantities.
 * @param rgb Receives the quantity-weighted colour.
 */
void fluid_liquid_rgb(const bottle_liquid_t *liquid, uint8_t rgb[3]);

/** Paint particles, pour effects, glass, mouth cue and fill gauge into a 360x360 byte-swapped
 * RGB565 frame. Effects advance by the time since the previous call. A buffer seen in one of the
 * last few calls is cleared only where that call drew, so nothing else may write to it in between.
 * @param pixels Caller-owned frame, written synchronously.
 * @param scene Frame inputs; impact fields are overwritten.
 */
void fluid_render_scene(uint16_t *pixels, fluid_scene_t *scene);
