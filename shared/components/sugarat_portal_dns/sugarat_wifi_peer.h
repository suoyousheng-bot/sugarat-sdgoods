/* SPDX-License-Identifier: MIT
 * IPv4 subnet checks also accept IPv4-mapped peers from the HTTP server's dual-stack listener. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline bool sugarat_wifi_peer_on_subnet(const uint8_t *address, size_t bytes, uint32_t network, uint32_t netmask) {
    static const uint8_t mapped_prefix[12] = {0,0,0,0,0,0,0,0,0,0,0xff,0xff};
    if (bytes == 16) {
        if (memcmp(address, mapped_prefix, sizeof(mapped_prefix))) return false;
        address += sizeof(mapped_prefix);
    } else if (bytes != 4) return false;
    uint32_t ipv4; memcpy(&ipv4, address, sizeof(ipv4));
    return (ipv4 & netmask) == (network & netmask);
}
