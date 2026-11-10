/*
 * Deotaland Agent Link protocol constants and build configuration checks.
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "sdkconfig.h"

#if CONFIG_SUGARAT_AGENT_LINK_PRODUCT_ID <= 0
#error "Set CONFIG_SUGARAT_AGENT_LINK_PRODUCT_ID to the Deotaland product id registered for this device (menuconfig or sdkconfig.defaults)"
#endif

/* Realtime voice WebSocket subprotocol. */
#define AGENT_VOICE_SUBPROTOCOL "tidb-link.realtime.v1"

/* Requested audio: Opus 16 kHz up, Opus 24 kHz down (decoded to 16 kHz), 20 ms packets. */
#define AGENT_VOICE_AUDIO_CONFIG \
    "{\"input\":{\"encoding\":\"opus\",\"sampleRate\":16000,\"channels\":1,\"packetDurationMs\":20}," \
    "\"output\":{\"encoding\":\"opus\",\"sampleRate\":24000,\"channels\":1,\"packetDurationMs\":20,\"bitrate\":24000}}"
