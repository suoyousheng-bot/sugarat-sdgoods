/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Mutually exclusive ESP-NOW and temporary editing access point with optional WPA2. */
#pragma once
#include "card_language.h"
#include "card_exchange.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "sugarat_wifi_dns.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
typedef enum {CARD_NET_OFF,CARD_NET_RADIO,CARD_NET_PORTAL} card_network_mode_t;
typedef struct {
    card_network_mode_t mode;
    bool wifi_owned,event_owned,now_owned;
    QueueHandle_t datagrams;
    esp_netif_t *netif;
    sugarat_wifi_dns_t *dns;
    httpd_handle_t server;
    SemaphoreHandle_t mutex;
    card_profile_t published,pending;
    unsigned request;
    bool queued,finished,failed;
    char ssid[33],password[17],token[33],url[128];
    uint64_t deadline;
    card_message_t notice;
} card_network_t;
/** Start only if the process has no other Wi-Fi owner; all Wi-Fi settings remain in RAM. */
bool card_network_radio(card_network_t *network);
/** Send a bounded datagram on the configured application channel. */
bool card_network_send(void *context,const uint8_t mac[6],const uint8_t *bytes,size_t size);
/** Drain callbacks on the LVGL/application thread. */
void card_network_poll(card_network_t *network,card_exchange_t *exchange,uint64_t now);
/** Start an expiring AP in the selected language; false opens it, true adds a fresh eight-digit WPA2 password. */
bool card_network_portal(card_network_t *network,const card_profile_t *profile,uint64_t now,bool password_required,card_language_t language);
/** Take at most one queued profile mutation; HTTP never waits for the LVGL thread. */
bool card_network_pending(card_network_t *network,card_profile_t *out);
/** Publish saved state only after a verified durable write, or a visible failure. */
void card_network_complete(card_network_t *network,bool success,const card_profile_t *profile);
/** Join handlers before release. False retains unreleased resources and forbids a new network mode. */
bool card_network_close(card_network_t *network);
/** Start editor routes; the network owner closes a partially started server on failure. */
bool card_http_start(card_network_t *network);
/** Read station presence; false leaves the output untouched and requires stopping the portal. */
bool card_network_connected(card_network_t *network,bool *connected);
