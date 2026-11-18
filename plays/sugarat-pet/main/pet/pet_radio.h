/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Owned ESP-NOW station. No credentials or platform partition settings are changed. */
#pragma once
#include "pet_social.h"
typedef struct { void *queue; bool wifi_owned, event_owned, now_owned; const char *notice; } pet_radio_t;
/** Start a private RAM-configured STA only when no other Wi-Fi owner exists. */
bool pet_radio_open(pet_radio_t *radio);
/** Stop receive delivery before releasing its queue and owned Wi-Fi resources. */
void pet_radio_close(pet_radio_t *radio);
/** Add a channel-local peer and transmit one bounded application datagram. */
bool pet_radio_send(pet_radio_t *radio, const uint8_t mac[6], const uint8_t *data, size_t size);
/** Drain queued frames on the social owner's thread. */
void pet_radio_poll(pet_radio_t *radio, pet_social_t *social);
