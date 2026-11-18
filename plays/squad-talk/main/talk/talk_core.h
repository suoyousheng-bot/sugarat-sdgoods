/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Two-peer, half-duplex session protocol. One worker owns each instance; time is monotonic ms. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TALK_CHANNELS 8
#define TALK_SAMPLES 256
#define TALK_HEADER 24
#define TALK_AUDIO_BYTES (4 + TALK_SAMPLES / 2)
#define TALK_PACKET_MAX (TALK_HEADER + TALK_AUDIO_BYTES)

typedef enum { TALK_HELLO=1, TALK_REQUEST, TALK_GRANT, TALK_AUDIO, TALK_END } talk_kind_t;
typedef enum { TALK_WAITING, TALK_READY, TALK_REQUESTING, TALK_TRANSMITTING, TALK_RECEIVING, TALK_PAUSED } talk_phase_t;
typedef struct {
    uint8_t channel;
    talk_kind_t kind;
    uint32_t boot, session, sequence;
    const uint8_t *payload;
    size_t size;
} talk_packet_t;
typedef bool (*talk_send_fn)(const uint8_t *destination, const uint8_t *data, size_t size, void *user);
typedef void (*talk_play_fn)(const int16_t *samples, size_t count, void *user);
typedef struct {
    uint8_t mac[6], peer[6], channel;
    uint32_t boot, peer_boot, session, remote_session, next_sequence, receive_sequence;
    uint64_t now, peer_seen, remote_audio, started, hello_at, request_at;
    bool paired, peer_available, held, paused, have_sequence, transport_failed;
    talk_phase_t phase;
    uint32_t received, lost, rejected;
    talk_send_fn send;
    talk_play_fn play;
    void *user;
} talk_core_t;

/** Validate a wire packet, including exact lengths, version, group and codec state. */
bool talk_packet_read(talk_packet_t *out, const uint8_t *data, size_t size);
/** Encode an independently decodable 16 ms IMA ADPCM block; output has TALK_AUDIO_BYTES bytes. */
void talk_audio_encode(uint8_t *out, const int16_t *pcm);
/** Decode one validated block to TALK_SAMPLES signed 16-bit samples. */
void talk_audio_decode(int16_t *out, const uint8_t *data);
/** Initialize callbacks and identity. The transport copies callback buffers before returning. */
void talk_core_init(talk_core_t *core, const uint8_t mac[6], uint32_t boot,
                    talk_send_fn send, talk_play_fn play, void *user);
/** Advance heartbeats, bounded request retries, talk timeout and peer expiry. */
void talk_core_tick(talk_core_t *core, uint64_t now);
/** Accept a received wire packet with its actual radio source address; wrong groups are ignored. */
void talk_core_receive(talk_core_t *core, const uint8_t source[6], const uint8_t *data, size_t size);
/** Change the hold state. A new press is required after failure, timeout or losing a simultaneous request. */
void talk_core_hold(talk_core_t *core, bool held);
/** End any conversation and forget peers when selecting a different logical channel (1..8). */
void talk_core_channel(talk_core_t *core, uint8_t channel);
/** Suspend participation during overlays, sleep or application exit; resuming requires a fresh press. */
void talk_core_pause(talk_core_t *core, bool paused);
/** Send a microphone block only while an acknowledged transmit session owns the channel. */
void talk_core_audio(talk_core_t *core, const int16_t *samples);
