/* SPDX-License-Identifier: Apache-2.0 */
/** English time-tool text selected for English requests and references. */
#include "time_copy.h"
#include <inttypes.h>
const time_copy_t time_copy_en = {
    .titles = {"Focus", "Countdown", "Stopwatch", "Clock"},
    .ready = "Ready", .running = "Running", .paused = "Paused", .done = "Finished · tap to continue",
    .focus = "Focus", .rest = "Break",
    .focus_detail = "%s · %" PRIu32 " sessions done", .countdown_detail = "Keeps running across pages",
    .lap_detail = "Lap %" PRIu32 " · %02" PRIu64 ":%02" PRIu64 ".%02u", .lap_hint = "Record a lap while running",
    .seconds = "%02u sec", .clock_unset = "Set the current time first", .clock_detail = "Manual · resets on restart",
    .clock_columns = "Hours          Minutes", .duration_columns = "Minutes         Seconds",
    .set_time = "Set time", .set_duration = "Set duration",
    .cancel = "Cancel", .save = "Save", .reset = "Reset", .lap = "Lap", .start = "Start", .pause = "Pause",
    .focus_presets = {"25 min", "45 min", "Break"}, .countdown_presets = {"10 sec", "5 min", "Custom"},
    .rest_complete = "Break complete", .focus_complete = "Focus complete", .countdown_complete = "Countdown finished",
    .invalid_duration = "Set at least 1 second",
};
