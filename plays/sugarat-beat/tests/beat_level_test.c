/** Exercise the chart and tap windows with the badge's 8 ms audio clock. */
#include "beat_level.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void clear_with_timing(bool late)
{
    beat_level_t level;
    beat_level_reset(&level);
    unsigned jumps = 0;
    for (uint32_t ms = 0; ms < TRACK_MS && level.result == BEAT_ACTIVE; ms += 8) {
        beat_level_advance(&level, ms);
        if (beat_level_cue_pad(&level, ms) == BEAT_NO_CUE) {
            continue;
        }
        if (late) {
            if (!beat_level_hazard(level.to) || ms - level.launch_ms < 64) {
                continue;
            }
            assert(beat_level_tap(&level, ms) == BEAT_TAP_EXTENDED);
        } else {
            if (!beat_level_hazard(level.to + 1) || beat_level_next_launch(&level) - ms > 112) {
                continue;
            }
            assert(beat_level_tap(&level, ms) == BEAT_TAP_QUEUED);
        }
        ++jumps;
    }
    assert(level.result == BEAT_CLEARED && level.from == FINISH_TILE && jumps == 18);
    assert(level.launch_ms == 29500);
}

int main(void)
{
    beat_level_t level;
    beat_level_reset(&level);
    assert(beat_level_tap(&level, 3000 - TAP_EARLY_MS - 1) == BEAT_TAP_IGNORED);
    assert(beat_level_cue_pad(&level, 3000 - TAP_EARLY_MS - 1) == BEAT_NO_CUE);
    assert(beat_level_tap(&level, 3000 - TAP_EARLY_MS) == BEAT_TAP_QUEUED);
    assert(beat_level_tap(&level, 2999) == BEAT_TAP_IGNORED);
    beat_level_advance(&level, 3000);
    assert(level.from == 4 && level.to == 6 && !level.tap_queued);
    assert(beat_level_tap(&level, 3000) == BEAT_TAP_IGNORED);

    beat_level_reset(&level);
    beat_level_advance(&level, 3000);
    assert(beat_level_cue_pad(&level, 3000) == 4);
    assert(beat_level_tap(&level, 3000) == BEAT_TAP_EXTENDED);

    beat_level_reset(&level);
    const uint32_t late_edge = 3000 + TAP_LATE_MS;
    beat_level_advance(&level, late_edge);
    assert(beat_level_cue_pad(&level, late_edge) == 4);
    float before_tile, before_lift, after_tile, after_lift;
    beat_level_pose(&level, late_edge, &before_tile, &before_lift);
    assert(beat_level_tap(&level, late_edge) == BEAT_TAP_EXTENDED);
    beat_level_pose(&level, late_edge, &after_tile, &after_lift);
    assert(fabsf(before_tile - after_tile) < .001f && fabsf(before_lift - after_lift) < .001f);
    assert(beat_level_cue_pad(&level, late_edge) == BEAT_NO_CUE);
    beat_level_pose(&level, 3500, &after_tile, &after_lift);
    assert(fabsf(after_tile - 5.0f) < .001f && after_lift > 60.0f);

    beat_level_reset(&level);
    assert(beat_level_tap(&level, late_edge + 1) == BEAT_TAP_IGNORED);
    assert(beat_level_cue_pad(&level, late_edge + 1) == BEAT_NO_CUE);
    assert(beat_level_tap(&level, 3480) == BEAT_TAP_IGNORED);
    beat_level_advance(&level, 7000);
    assert(level.result == BEAT_MISSED && level.from == 5 && level.launch_ms == 3500);
    assert(beat_level_tap(&level, 7000) == BEAT_TAP_IGNORED);

    clear_with_timing(false);
    clear_with_timing(true);
    puts("All 18 hazards cleared with early and late taps. On-beat, edge bounds and pose continuity verified.");
}
