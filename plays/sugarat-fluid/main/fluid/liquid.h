/* SPDX-License-Identifier: Apache-2.0 */
/** Integer liquid quantities; each unit is one visible particle, with four conserved ingredients. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#define BOTTLE_CAPACITY 600
#define BOTTLE_INGREDIENTS 4
typedef struct { uint16_t part[BOTTLE_INGREDIENTS]; } bottle_liquid_t;
/** Return total units; parsed liquids must also pass bottle_liquid_valid(). */
unsigned bottle_liquid_total(const bottle_liquid_t *liquid);
/** Validate capacity at the radio parser. Empty bottles are valid. */
bool bottle_liquid_valid(const bottle_liquid_t *liquid);
/** Replace a practice bottle with a pure ingredient, clamped to capacity. */
void bottle_liquid_fill(bottle_liquid_t *liquid, unsigned units, unsigned ingredient);
/** Move at most units, bounded by source and destination; conserve each ingredient exactly. */
unsigned bottle_liquid_transfer(bottle_liquid_t *source, bottle_liquid_t *destination, unsigned units);
