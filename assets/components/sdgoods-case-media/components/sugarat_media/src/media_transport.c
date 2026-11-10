/* SPDX-License-Identifier: Apache-2.0 */
/** A new track starts only after the previous audio operation acknowledges completion. */
#include "media_transport.h"

void media_transport_init(media_transport_t *o, media_driver_t driver, const uint32_t *durations_ms,
                          uint32_t track_count, uint32_t stop_timeout_ms, uint32_t start_timeout_ms)
{
    *o = (media_transport_t){.driver = driver, .durations_ms = durations_ms, .track_count = track_count,
        .stop_timeout_ms = stop_timeout_ms, .start_timeout_ms = start_timeout_ms};
}

void media_transport_submit(media_transport_t *o, media_request_t request)
{
    if (o->fault_latched) return;
    o->pending = request;
    o->has_pending = true;
    o->state = (media_snapshot_t){.id = request.id, .track = request.track, .position_ms = request.position_ms,
        .phase = o->active_id ? MEDIA_STOPPING : request.play ? MEDIA_STARTING : MEDIA_PAUSED};
}

void media_transport_tick(media_transport_t *o, uint64_t now_ms)
{
    if (o->fault_latched) return;
    if (o->active_id && o->deadline && now_ms >= o->deadline) {
        o->state.phase = MEDIA_FAILED;
        o->state.error = o->stopping ? MEDIA_ERROR_STOP_TIMEOUT : MEDIA_ERROR_START_TIMEOUT;
        o->fault_latched = true;
        o->has_pending = false;
        o->driver.stop(o->driver.context);
        return;
    }
    if (!o->has_pending) return;
    if (o->active_id) {
        if (!o->stopping) {
            o->stopping = true;
            o->deadline = now_ms + o->stop_timeout_ms;
            o->driver.stop(o->driver.context);
        }
        return;
    }
    media_request_t request = o->pending;
    o->has_pending = false;
    if (!request.play || request.position_ms >= o->durations_ms[request.track]) {
        o->state.phase = request.position_ms >= o->durations_ms[request.track] ? MEDIA_DONE : MEDIA_PAUSED;
        return;
    }
    o->active_id = request.id;
    o->active_track = request.track;
    o->base_ms = request.position_ms;
    o->deadline = now_ms + o->start_timeout_ms;
    o->state.phase = MEDIA_STARTING;
    if (!o->driver.start(o->driver.context, request)) {
        o->active_id = 0;
        o->deadline = 0;
        o->state.phase = MEDIA_FAILED;
        o->state.error = MEDIA_ERROR_DEVICE;
    }
}

void media_transport_progress(media_transport_t *o, uint32_t id, uint32_t elapsed_ms)
{
    if (o->fault_latched || id != o->active_id || o->stopping || o->has_pending) return;
    o->deadline = 0;
    o->state.phase = MEDIA_PLAYING;
    uint32_t duration = o->durations_ms[o->active_track];
    o->state.position_ms = elapsed_ms >= duration - o->base_ms ? duration : o->base_ms + elapsed_ms;
}

void media_transport_terminal(media_transport_t *o, uint32_t id, media_terminal_t terminal)
{
    if (!id || id != o->active_id) return;
    o->active_id = 0;
    o->stopping = false;
    o->deadline = 0;
    if (o->fault_latched || o->has_pending) return;
    if (terminal == MEDIA_FINISHED) {
        o->state.position_ms = o->durations_ms[o->active_track];
        o->state.phase = MEDIA_DONE;
    } else {
        o->state.phase = terminal == MEDIA_BROKEN ? MEDIA_FAILED : MEDIA_PAUSED;
        o->state.error = terminal == MEDIA_BROKEN ? MEDIA_ERROR_DEVICE : MEDIA_ERROR_NONE;
    }
}
