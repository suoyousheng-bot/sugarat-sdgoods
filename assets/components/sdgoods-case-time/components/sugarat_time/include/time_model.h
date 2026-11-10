/* SPDX-License-Identifier: Apache-2.0 */
/** Monotonic, RAM-only timers shared by the badge and native preview. */
#ifndef SUGARAT_TIME_MODEL_H
#define SUGARAT_TIME_MODEL_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint64_t total_ms, remaining_ms;
    bool running, done;
} time_countdown_t;

typedef struct {
    uint64_t last_ms, stopwatch_ms, lap_ms, clock_ms;
    time_countdown_t focus, countdown;
    uint32_t completed_focus, laps, focus_duration_ms;
    bool resting, stopwatch_running, clock_set;
} time_model_t;

/** Initialize at the caller's monotonic timestamp, in milliseconds. */
void time_model_init(time_model_t *model, uint64_t now_ms);
/** Advance all timers; return bit 1 for focus completion and bit 2 for countdown completion. */
unsigned time_model_update(time_model_t *model, uint64_t now_ms);
/** Replace a countdown with a stopped, unfinished duration in milliseconds. */
void time_countdown_reset(time_countdown_t *timer, uint64_t duration_ms);
/** Pause, resume or restart a completed countdown. */
void time_countdown_toggle(time_countdown_t *timer);
#endif
