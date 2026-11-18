/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Single-question captive DNS: A returns the hotspot; other IN types return NODATA.
 * Additional records are omitted. Compressed questions and malformed labels are rejected. */
static inline size_t sugarat_wifi_dns_reply(uint8_t *packet, size_t bytes, size_t capacity, uint32_t ipv4) {
    if (bytes < 12 || bytes > capacity || (packet[2] & 0xf8) || packet[4] || packet[5] != 1) return 0;
    size_t end = 12;
    for (;;) {
        if (end >= bytes) return 0;
        unsigned length = packet[end++];
        if (!length) break;
        if (length > 63 || length > bytes - end || end + length - 12 > 254) return 0;
        end += length;
    }
    if (bytes - end < 4 || packet[end + 2] || packet[end + 3] != 1) return 0;
    bool address = packet[end] == 0 && packet[end + 1] == 1;
    end += 4;
    if (address && capacity - end < 16) return 0;
    packet[2] = 0x84 | (packet[2] & 1); packet[3] &= 0x10;
    memset(packet + 6, 0, 6); packet[7] = address ? 1 : 0;
    if (address) {
        const uint8_t answer[12] = {0xc0,12,0,1,0,1,0,0,0,0,0,4};
        memcpy(packet + end, answer, sizeof(answer));
        memcpy(packet + end + sizeof(answer), &ipv4, 4);
        end += 16;
    }
    return end;
}
