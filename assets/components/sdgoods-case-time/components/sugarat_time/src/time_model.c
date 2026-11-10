/* SPDX-License-Identifier: Apache-2.0 */
/** Elapsed-time accounting independent of rendering frequency and wall-clock edits. */
#include "time_model.h"
#include <string.h>

void time_countdown_reset(time_countdown_t *timer, uint64_t duration_ms)
{
    *timer = (time_countdown_t){.total_ms = duration_ms, .remaining_ms = duration_ms};
}

void time_model_init(time_model_t *model, uint64_t now_ms)
{
    memset(model, 0, sizeof(*model));
    model->last_ms = now_ms;
    model->focus_duration_ms = 25 * 60000;
    time_countdown_reset(&model->focus, 25 * 60000);
    time_countdown_reset(&model->countdown, 5 * 60000);
}

static bool advance(time_countdown_t *timer, uint64_t delta)
{
    if (!timer->running) return false;
    if (delta < timer->remaining_ms) {
        timer->remaining_ms -= delta;
        return false;
    }
    timer->remaining_ms = 0;
    timer->running = false;
    timer->done = true;
    return true;
}

unsigned time_model_update(time_model_t *model, uint64_t now_ms)
{
    uint64_t delta = now_ms - model->last_ms;
    model->last_ms = now_ms;
    unsigned completed = 0;
    if (advance(&model->focus, delta)) {
        completed |= 1;
        if (!model->resting) ++model->completed_focus;
    }
    if (advance(&model->countdown, delta)) completed |= 2;
    if (model->stopwatch_running) model->stopwatch_ms += delta;
    if (model->clock_set) model->clock_ms = (model->clock_ms + delta) % 86400000;
    return completed;
}

void time_countdown_toggle(time_countdown_t *timer)
{
    if (timer->done) time_countdown_reset(timer, timer->total_ms);
    timer->running = !timer->running;
}
