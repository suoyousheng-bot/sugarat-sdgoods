/** The fixed chart uses quarter-note indices, with tile zero on the first playable downbeat. */
#include "beat_level.h"

static const bool hazards[FINISH_TILE + 1] = {
    [5] = true, [9] = true, [13] = true, [16] = true, [20] = true, [22] = true,
    [26] = true, [30] = true, [33] = true, [35] = true, [38] = true, [41] = true,
    [44] = true, [46] = true, [49] = true, [51] = true, [54] = true, [56] = true,
};

bool beat_level_hazard(unsigned tile)
{
    return tile <= FINISH_TILE && hazards[tile];
}

void beat_level_reset(beat_level_t *level)
{
    *level = (beat_level_t){.launch_ms = COUNT_IN_MS};
}

uint32_t beat_level_next_launch(const beat_level_t *level)
{
    return COUNT_IN_MS + level->to * BEAT_MS;
}

void beat_level_advance(beat_level_t *level, uint32_t ms)
{
    while (level->result == BEAT_ACTIVE && ms >= beat_level_next_launch(level)) {
        level->launch_ms = beat_level_next_launch(level);
        level->from = level->to;
        if (beat_level_hazard(level->from)) {
            level->result = BEAT_MISSED;
            return;
        }
        if (level->from == FINISH_TILE) {
            level->result = BEAT_CLEARED;
            return;
        }
        const bool skip = level->tap_queued && level->from + 1 < FINISH_TILE;
        level->to = level->from + (skip ? 2 : 1);
        level->tap_queued = false;
    }
}

static bool can_extend(const beat_level_t *level, uint32_t ms)
{
    return level->to == level->from + 1 && level->to < FINISH_TILE
        && ms >= level->launch_ms && ms - level->launch_ms <= TAP_LATE_MS;
}

static bool can_queue(const beat_level_t *level, uint32_t ms)
{
    const uint32_t landing_ms = beat_level_next_launch(level);
    return !level->tap_queued && level->to + 1 < FINISH_TILE
        && !beat_level_hazard(level->to)
        && ms < landing_ms && landing_ms - ms <= TAP_EARLY_MS;
}

beat_tap_t beat_level_tap(beat_level_t *level, uint32_t ms)
{
    beat_level_advance(level, ms);
    if (level->result != BEAT_ACTIVE) {
        return BEAT_TAP_IGNORED;
    }
    if (can_extend(level, ms)) {
        ++level->to;
        return BEAT_TAP_EXTENDED;
    }
    if (can_queue(level, ms)) {
        level->tap_queued = true;
        return BEAT_TAP_QUEUED;
    }
    return BEAT_TAP_IGNORED;
}

int beat_level_cue_pad(const beat_level_t *level, uint32_t ms)
{
    if (level->result != BEAT_ACTIVE || ms < COUNT_IN_MS) {
        return BEAT_NO_CUE;
    }
    if (beat_level_hazard(level->to) && can_extend(level, ms)) {
        return (int)level->from;
    }
    if (beat_level_hazard(level->to + 1) && can_queue(level, ms)) {
        return (int)level->to;
    }
    return BEAT_NO_CUE;
}

void beat_level_pose(const beat_level_t *level, uint32_t ms, float *tile, float *lift)
{
    *tile = (float)level->from;
    *lift = 0.0f;
    if (level->result != BEAT_ACTIVE || level->from == level->to || ms < level->launch_ms) {
        return;
    }
    const uint32_t elapsed = ms - level->launch_ms;
    const float short_phase = (float)elapsed / BEAT_MS;
    *tile += short_phase;
    const float short_lift = 156.0f * short_phase * (1.0f - short_phase);
    if (level->to - level->from == 1 || elapsed <= TAP_LATE_MS) {
        *lift = short_lift;
        return;
    }

    const float phase = short_phase / 2.0f;
    const float long_lift = 280.0f * phase * (1.0f - phase);
    /* The curves share their first 100 ms so a late tap cannot move the character abruptly. */
    const float blend = (float)(elapsed - TAP_LATE_MS) / 48.0f;
    if (blend >= 1.0f) {
        *lift = long_lift;
    } else {
        const float smooth = blend * blend * (3.0f - 2.0f * blend);
        *lift = short_lift + (long_lift - short_lift) * smooth;
    }
}
