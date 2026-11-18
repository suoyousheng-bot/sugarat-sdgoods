/* SPDX-License-Identifier: Apache-2.0 */
/** ESP-NOW transport owned by the bottle worker; callbacks enqueue only. */
#pragma once
#include "link.h"
#include "esp_err.h"
typedef struct { void *queue; bool wifi_owned, event_owned, now_owned; esp_err_t error; } bottle_radio_t;
/** Open the configured channel. Failure closes acquired resources and leaves solo usable. */
esp_err_t bottle_radio_open(bottle_radio_t *radio);
/** Revoke the callback queue before releasing it, then stop owned Wi-Fi resources. */
void bottle_radio_close(bottle_radio_t *radio);
/** bottle_send_fn adapter; delivery is confirmed by the session, not the MAC callback. */
void bottle_radio_send(void *radio, const uint8_t mac[6], const uint8_t *bytes, size_t size);
/** Drain a bounded receive batch into the worker-owned session. */
void bottle_radio_poll(bottle_radio_t *radio, bottle_link_t *link, uint32_t now_ms);
