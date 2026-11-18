/* SPDX-License-Identifier: MIT */
#pragma once
#include "esp_netif.h"

typedef struct sugarat_wifi_dns sugarat_wifi_dns_t;
/** Start hotspot-only DNS and advertise its address through DHCP; NULL output on failure. */
esp_err_t sugarat_wifi_dns_start(esp_netif_t *ap, sugarat_wifi_dns_t **out);
/** Remove the UDP receiver on the TCP/IP thread before freeing its context; accepts NULL. */
void sugarat_wifi_dns_stop(sugarat_wifi_dns_t *dns);
