/* SPDX-License-Identifier: Apache-2.0 */
/** Quantity accounting independent of radio, display and particle physics. */
#include "liquid.h"
#include <string.h>
unsigned bottle_liquid_total(const bottle_liquid_t *liquid)
{
    unsigned total = 0;
    for (unsigned i = 0; i < BOTTLE_INGREDIENTS; i++) total += liquid->part[i];
    return total;
}
bool bottle_liquid_valid(const bottle_liquid_t *liquid)
{
    return bottle_liquid_total(liquid) <= BOTTLE_CAPACITY;
}
void bottle_liquid_fill(bottle_liquid_t *liquid, unsigned units, unsigned ingredient)
{
    memset(liquid, 0, sizeof(*liquid));
    liquid->part[ingredient % BOTTLE_INGREDIENTS] = units > BOTTLE_CAPACITY ? BOTTLE_CAPACITY : units;
}
unsigned bottle_liquid_transfer(bottle_liquid_t *source, bottle_liquid_t *destination, unsigned units)
{
    unsigned total = bottle_liquid_total(source), space = BOTTLE_CAPACITY - bottle_liquid_total(destination);
    if (units > total) units = total;
    if (units > space) units = space;
    if (!units) return 0;
    unsigned cumulative = 0, moved = 0;
    for (unsigned i = 0; i < BOTTLE_INGREDIENTS; i++) {
        cumulative += source->part[i];
        unsigned next = cumulative * units / total;
        unsigned amount = next - moved;
        source->part[i] -= amount;
        destination->part[i] += amount;
        moved = next;
    }
    return moved;
}
