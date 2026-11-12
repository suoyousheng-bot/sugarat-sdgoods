/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 *
 * Sugarat play community client: public play details, covers, download tickets and the firmware stream.
 * All three endpoints are anonymous. Calls block; run them on the install worker.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    COMMUNITY_OK = 0,
    COMMUNITY_NOT_FOUND,   /**< No public play with this ID. */
    COMMUNITY_OFFLINE,     /**< Connection, DNS or timeout failure. */
    COMMUNITY_SERVER,      /**< Unexpected HTTP status. */
    COMMUNITY_FORMAT,      /**< Response missing fields, or the firmware is not an application .bin. */
    COMMUNITY_CANCELLED,
    COMMUNITY_SINK,        /**< The data sink refused a chunk; the sink holds the reason. */
} community_status_t;

/** Published version of a play, as needed to decide and perform an install. */
typedef struct {
    uint32_t play_id, version_id;
    char title[64];
    char author[48];
    char category[24];
    char version[33];
    uint32_t product_id, hardware_revision_id;
    uint32_t size;
    uint8_t sha256[32];
    char cover_url[200];   /**< Absolute cover URL, or empty. */
} community_play_t;

/** Receives consecutive firmware bytes; return false to stop the download. */
typedef bool (*community_sink_t)(const uint8_t *data, size_t len, void *context);

/** Fetch the public details of a play (`GET /api/v1/community/plays/{id}`). */
community_status_t community_fetch_play(uint32_t play_id, community_play_t *out);

/** Request a download ticket and stream exactly `play->size` bytes into `sink`.
 * Interrupted transfers resume with HTTP Range up to three times, each with a fresh ticket.
 * @param received - Updated as bytes are delivered; readable from other tasks for progress.
 * @param cancel - Polled between chunks. */
community_status_t community_download(const community_play_t *play, community_sink_t sink, void *context,
                                      volatile const bool *cancel, volatile uint32_t *received);

/** Download the play cover into a PSRAM buffer that the caller frees.
 * @returns COMMUNITY_FORMAT when the play has no cover, the file exceeds 256 KiB, or it is not a JPEG. */
community_status_t community_fetch_cover(const community_play_t *play, uint8_t **jpeg, size_t *len);
