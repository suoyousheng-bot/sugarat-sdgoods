/* SPDX-License-Identifier: Apache-2.0 */
/** Silent host backend using the same serialized transport as the device. */
#ifndef SUGARAT_MEDIA_CLOCK_H
#define SUGARAT_MEDIA_CLOCK_H
#include "sugarat_media.h"
typedef struct {
    media_transport_t transport;
    uint64_t (*now_ms)(void);
    uint64_t started_ms;
    uint32_t driver_id, stopped_id, duration_ms;
    uint8_t volume;
} media_clock_t;
/** Initialize an explicit silent timeline with caller-owned state and durations. */
void media_clock_init(media_clock_t *clock, uint64_t (*now_ms)(void), const uint32_t *durations_ms,
                      uint32_t count, uint8_t volume);
/** Return callbacks whose context remains valid through screen disposal. */
media_backend_t media_clock_backend(media_clock_t *clock);
#endif
