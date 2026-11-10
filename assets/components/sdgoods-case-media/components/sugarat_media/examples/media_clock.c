/* SPDX-License-Identifier: Apache-2.0 */
/** Host simulation emits explicit completion and stop acknowledgements without audio. */
#include "media_clock.h"
static bool start(void *context, media_request_t request)
{
    media_clock_t *clock = context;
    clock->driver_id = request.id; clock->started_ms = clock->now_ms();
    clock->duration_ms = clock->transport.durations_ms[request.track] - request.position_ms;
    return true;
}
static void stop(void *context)
{
    media_clock_t *clock = context;
    clock->stopped_id = clock->driver_id; clock->driver_id = 0;
}
static bool request(void *context, media_request_t command)
{
    media_clock_t *clock = context;
    if (clock->transport.fault_latched) return false;
    media_transport_submit(&clock->transport, command); return true;
}
static bool volume(void *context, uint8_t percent)
{
    media_clock_t *clock = context; clock->volume = percent; return true;
}
static media_snapshot_t read(void *context)
{
    media_clock_t *clock = context;
    uint64_t now = clock->now_ms();
    if (clock->stopped_id) {
        media_transport_terminal(&clock->transport, clock->stopped_id, MEDIA_STOPPED); clock->stopped_id = 0;
    }
    if (clock->driver_id) {
        uint64_t elapsed = now - clock->started_ms;
        if (elapsed >= clock->duration_ms) {
            media_transport_terminal(&clock->transport, clock->driver_id, MEDIA_FINISHED); clock->driver_id = 0;
        } else media_transport_progress(&clock->transport, clock->driver_id, (uint32_t)elapsed);
    }
    media_transport_tick(&clock->transport, now);
    media_snapshot_t state = clock->transport.state; state.volume = clock->volume; return state;
}
void media_clock_init(media_clock_t *clock, uint64_t (*now_ms)(void), const uint32_t *durations_ms,
                      uint32_t count, uint8_t initial_volume)
{
    *clock = (media_clock_t){.now_ms = now_ms, .volume = initial_volume};
    media_transport_init(&clock->transport, (media_driver_t){start, stop, clock}, durations_ms, count, 2000, 2000);
}
media_backend_t media_clock_backend(media_clock_t *clock)
{
    return (media_backend_t){request, volume, read, clock};
}
