/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 */
#include "radio_region.h"
#include "sdkconfig.h"
#include <string.h>

#define EAST "EST5EDT,M3.2.0/2,M11.1.0/2"
#define CENTRAL "CST6CDT,M3.2.0/2,M11.1.0/2"
#define MOUNTAIN "MST7MDT,M3.2.0/2,M11.1.0/2"
#define PACIFIC "PST8PDT,M3.2.0/2,M11.1.0/2"
static const radio_region_t markets[] = {
    {"", "United States", "", "", CONFIG_SUGARAT_RADIO_DEFAULT_TIMEZONE},
    {"nyc", "New York, NY", "New York", "New York", EAST},
    {"la", "Los Angeles, CA", "Los Angeles", "California", PACIFIC},
    {"chicago", "Chicago, IL", "Chicago", "Illinois", CENTRAL},
    {"houston", "Houston, TX", "Houston", "Texas", CENTRAL},
    {"phoenix", "Phoenix, AZ", "Phoenix", "Arizona", "MST7"},
    {"philadelphia", "Philadelphia, PA", "Philadelphia", "Pennsylvania", EAST},
    {"san-francisco", "San Francisco, CA", "San Francisco", "California", PACIFIC},
    {"seattle", "Seattle, WA", "Seattle", "Washington", PACIFIC},
    {"boston", "Boston, MA", "Boston", "Massachusetts", EAST},
    {"dc", "Washington, DC", "Washington", "District of Columbia", EAST},
    {"miami", "Miami, FL", "Miami", "Florida", EAST},
    {"atlanta", "Atlanta, GA", "Atlanta", "Georgia", EAST},
    {"denver", "Denver, CO", "Denver", "Colorado", MOUNTAIN},
    {"nashville", "Nashville, TN", "Nashville", "Tennessee", CENTRAL},
    {"honolulu", "Honolulu, HI", "Honolulu", "Hawaii", "HST10"},
    {"anchorage", "Anchorage, AK", "Anchorage", "Alaska", "AKST9AKDT,M3.2.0/2,M11.1.0/2"},
};
size_t radio_region_count(void) { return sizeof(markets) / sizeof(markets[0]); }
const radio_region_t *radio_region_at(size_t index) {
    return index < radio_region_count() ? &markets[index] : NULL;
}
const radio_region_t *radio_region_find(const char *id) {
    for (size_t i = 0; id && i < radio_region_count(); ++i)
        if (!strcmp(id, markets[i].id)) return &markets[i];
    return &markets[0];
}
