/* SPDX-License-Identifier: Apache-2.0 */
/** Triple-buffer ownership. The caller serializes publish/present; rendering needs no shared lock. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "sugarat_frame_pacing.h"

typedef struct {
    uint16_t *rendering;
    uint16_t *ready;
    uint16_t *visible;
    uint32_t produced;
    uint32_t presented;
} fluid_frames_t;

/** Publish the completed rendering buffer and return the previous ready buffer to the producer.
 * @param frames Three distinct caller-owned buffers, with rendering exclusively owned by the producer.
 */
static inline void fluid_frames_publish(fluid_frames_t *frames)
{
    uint16_t *previous = frames->ready;
    frames->ready = frames->rendering;
    frames->rendering = previous;
    frames->produced++;
}

/** Take the newest complete frame when due; an empty queue consumes no presentation slot.
 * @param frames Serialized frame ownership. The consumer retains visible until its next successful call.
 * @param pacer Presentation cadence, owned by the consumer.
 * @param now_us Monotonic time.
 * @returns Whether a new frame became visible.
 */
static inline bool fluid_frames_present(fluid_frames_t *frames, sugarat_frame_pacer_t *pacer, int64_t now_us)
{
    if (frames->produced == frames->presented || !sugarat_frame_pacer_due(pacer, now_us)) return false;
    uint16_t *previous = frames->visible;
    frames->visible = frames->ready;
    frames->ready = previous;
    frames->presented = frames->produced;
    return true;
}
