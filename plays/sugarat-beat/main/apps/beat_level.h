/** Chart and input rules shared by the device and host checks. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    BEAT_MS = 500,
    COUNT_IN_MS = 1000,
    FINISH_TILE = 57,
    TRACK_MS = 30000,
    TAP_EARLY_MS = 130,
    TAP_LATE_MS = 100,
    BEAT_NO_CUE = -1,
};

typedef enum { BEAT_ACTIVE, BEAT_MISSED, BEAT_CLEARED } beat_result_t;
typedef enum { BEAT_TAP_IGNORED, BEAT_TAP_QUEUED, BEAT_TAP_EXTENDED } beat_tap_t;

typedef struct {
    uint32_t launch_ms;
    unsigned from;
    unsigned to;
    bool tap_queued;
    beat_result_t result;
} beat_level_t;

/** Reset to tile zero. The first launch follows the two count-in beats. */
void beat_level_reset(beat_level_t *level);

/** Resolve every landing through the given music position, including across dropped frames. */
void beat_level_advance(beat_level_t *level, uint32_t ms);

/** Return the track time at which the current hop lands and the next can launch. */
uint32_t beat_level_next_launch(const beat_level_t *level);

/** Queue the next long jump before the beat, or extend the current hop just after it. */
beat_tap_t beat_level_tap(beat_level_t *level, uint32_t ms);

/** Return the lit launch pad before or after a beat, or BEAT_NO_CUE. Call after advancing to ms. */
int beat_level_cue_pad(const beat_level_t *level, uint32_t ms);

/** Sample the current jump for rendering. */
void beat_level_pose(const beat_level_t *level, uint32_t ms, float *tile, float *lift);

/** True when a tile must be skipped. */
bool beat_level_hazard(unsigned tile);
