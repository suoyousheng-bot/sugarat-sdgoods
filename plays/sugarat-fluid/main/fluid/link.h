/* SPDX-License-Identifier: Apache-2.0 */
/** Ephemeral two-bottle session. The host owns quantities; guests acknowledge complete snapshots.
 * One worker owns this object. Radio callbacks only enqueue bytes. Lost sessions retain their
 * last snapshot and accept resynchronization only from the same peer and room. Reboots and
 * explicit Solo start a new practice bottle; liquid is not durable. See the play README.
 */
#pragma once
#include "liquid.h"
#include <stddef.h>
#define BOTTLE_PACKET_SIZE 44
#define BOTTLE_LINK_INTERVAL_MS 100
#define BOTTLE_LINK_TIMEOUT_MS 1500
#define BOTTLE_INVITE_TIMEOUT_MS 10000
typedef enum {
    BOTTLE_SOLO, BOTTLE_HOSTING, BOTTLE_SCANNING, BOTTLE_FOUND,
    BOTTLE_JOINING, BOTTLE_INVITED, BOTTLE_LINKED, BOTTLE_LOST
} bottle_phase_t;
typedef enum { BOTTLE_CLOSED, BOTTLE_RECEIVE, BOTTLE_POUR } bottle_pose_t;
typedef struct { bottle_pose_t pose; uint8_t rate; } bottle_input_t;
typedef enum { BOTTLE_HELLO = 1, BOTTLE_JOIN, BOTTLE_INPUT, BOTTLE_STATE, BOTTLE_LEAVE } bottle_message_t;
typedef struct {
    bottle_message_t type;
    bottle_input_t input;
    uint32_t room, sender, target, sequence, acknowledgement;
    bottle_liquid_t liquid[2];
} bottle_packet_t;
typedef void (*bottle_send_fn)(void *context, const uint8_t mac[6], const uint8_t *bytes, size_t size);
typedef struct {
    bottle_phase_t phase;
    bool host, rearm;
    uint32_t self, peer, room, ignored_room, revision, input_sequence, peer_sequence, acknowledged;
    uint32_t last_received_ms, last_sent_ms, last_step_ms, began_ms, last_transfer_ms;
    uint8_t peer_mac[6];
    bottle_liquid_t liquid[2];
    bottle_input_t local, remote;
    int8_t flow; /* -1: guest to host; +1: host to guest; 0: stopped. */
} bottle_link_t;
/** Encode a fixed-size versioned datagram without C structure padding. */
void bottle_packet_encode(const bottle_packet_t *packet, uint8_t bytes[BOTTLE_PACKET_SIZE]);
/** Reject unknown versions, invalid inputs, zero identifiers and over-capacity quantities. */
bool bottle_packet_decode(bottle_packet_t *packet, const uint8_t *bytes, size_t size);
/** Start a new solo practice bottle with the selected ingredient and quantity. */
void bottle_link_solo(bottle_link_t *link, unsigned ingredient, unsigned units);
/** Advertise a room using a fresh, nonzero random identity supplied by the platform. */
void bottle_link_host(bottle_link_t *link, uint32_t identity, uint32_t now_ms);
/** Discover rooms; ignore the last displayed room when Next is used. */
void bottle_link_scan(bottle_link_t *link, uint32_t identity, uint32_t now_ms);
/** Ask to join the displayed room; only its host may approve. */
void bottle_link_join(bottle_link_t *link, uint32_t now_ms);
/** Approve the pending guest and establish the authoritative initial quantities. */
void bottle_link_accept(bottle_link_t *link, uint32_t now_ms);
/** Classify normalized gravity in screen coordinates (right, down, into screen). */
bottle_input_t bottle_input_from_gravity(float x, float y, float z, bool available);
/** Set posture. Recovery requires an upright posture before another pour. */
void bottle_link_input(bottle_link_t *link, bottle_input_t input);
/** Consume a datagram from the worker; unrelated peers and stale packets have no effect. */
void bottle_link_receive(bottle_link_t *link, const uint8_t mac[6], const uint8_t *bytes, size_t size, uint32_t now_ms);
/** Send bounded retries/heartbeats and advance at most one acknowledged transfer per interval.
 * Missing acknowledgements prevent new transfers. After silence both sides stop and show Lost;
 * the host snapshot resolves any unacknowledged transfer when that same session reconnects.
 */
void bottle_link_tick(bottle_link_t *link, uint32_t now_ms, bottle_send_fn send, void *context);
/** Send a best-effort goodbye before radio teardown; delivery is not guaranteed. */
void bottle_link_leave(bottle_link_t *link, bottle_send_fn send, void *context);
/** Index of this device's bottle in the shared snapshot. */
unsigned bottle_link_local_index(const bottle_link_t *link);
