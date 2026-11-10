/* SPDX-License-Identifier: Apache-2.0 */
/** Serialized playback requests with confirmed stop/completion and explicit timeouts. */
#ifndef SUGARAT_MEDIA_TRANSPORT_H
#define SUGARAT_MEDIA_TRANSPORT_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { MEDIA_PAUSED, MEDIA_STARTING, MEDIA_PLAYING, MEDIA_STOPPING, MEDIA_DONE, MEDIA_FAILED } media_phase_t;
typedef enum { MEDIA_ERROR_NONE, MEDIA_ERROR_DEVICE, MEDIA_ERROR_STOP_TIMEOUT, MEDIA_ERROR_START_TIMEOUT } media_error_t;
typedef enum { MEDIA_FINISHED, MEDIA_STOPPED, MEDIA_BROKEN } media_terminal_t;
typedef struct { uint32_t id, track, position_ms; bool play; } media_request_t;
typedef struct {
    uint32_t id, track, position_ms;
    uint8_t volume;
    media_phase_t phase;
    media_error_t error;
} media_snapshot_t;
typedef struct {
    bool (*start)(void *context, media_request_t request);
    void (*stop)(void *context);
    void *context;
} media_driver_t;
typedef struct {
    media_driver_t driver;
    const uint32_t *durations_ms;
    uint32_t track_count, stop_timeout_ms, start_timeout_ms;
    media_snapshot_t state;
    media_request_t pending;
    uint32_t active_id, active_track, base_ms;
    uint64_t deadline;
    bool has_pending, stopping, fault_latched;
} media_transport_t;
/** Initialize one owner with static track durations and positive timeout budgets. */
void media_transport_init(media_transport_t *owner, media_driver_t driver, const uint32_t *durations_ms,
                          uint32_t track_count, uint32_t stop_timeout_ms, uint32_t start_timeout_ms);
/** Replace the pending request; the active buffer remains owned until a matching terminal event. */
void media_transport_submit(media_transport_t *owner, media_request_t request);
/** Advance one owner; drivers must report events later instead of reentering from start/stop. */
void media_transport_tick(media_transport_t *owner, uint64_t now_ms);
/** Publish elapsed audio time only for the current active request. */
void media_transport_progress(media_transport_t *owner, uint32_t id, uint32_t elapsed_ms);
/** Acknowledge actual completion or stop; old ids cannot complete newer requests. */
void media_transport_terminal(media_transport_t *owner, uint32_t id, media_terminal_t terminal);
#endif
