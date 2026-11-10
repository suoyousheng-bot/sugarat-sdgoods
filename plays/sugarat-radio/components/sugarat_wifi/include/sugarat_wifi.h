/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif
#define SUGARAT_WIFI_NETWORKS 16

typedef struct sugarat_wifi sugarat_wifi_t;
typedef enum {
    SW_IDLE, SW_SCANNING, SW_CONNECTING, SW_CONNECTED, SW_FAILED, SW_STOPPED
} sugarat_wifi_phase_t;
typedef enum {
    SUGARAT_WIFI_EVENT_CONNECTING = 1,
    SUGARAT_WIFI_EVENT_CONNECTED,
    SUGARAT_WIFI_EVENT_DISCONNECTED,
    SUGARAT_WIFI_EVENT_FAILED,
    SUGARAT_WIFI_EVENT_STORAGE_FAILED,
    SUGARAT_WIFI_EVENT_PORTAL_OPENED,
    SUGARAT_WIFI_EVENT_PORTAL_CLOSED,
    SUGARAT_WIFI_EVENT_PORTAL_FAILED
} sugarat_wifi_event_id_t;
/** Immutable notification; contains no password. CONNECTED requires DHCP, saved credentials and a closed portal. */
typedef struct {
    sugarat_wifi_event_id_t id;
    uint32_t sequence, attempt;
    esp_err_t error;
    int disconnect_reason;
    bool saved, portal;
    char ssid[33], ip[16];
} sugarat_wifi_event_t;
/** Runs on the network worker, outside its mutex. Copy event data before returning.
 * Do not call LVGL, block on this worker, or destroy Wi-Fi here. Snapshot and nonblocking queue APIs are allowed. */
typedef void (*sugarat_wifi_event_cb_t)(sugarat_wifi_t *wifi, const sugarat_wifi_event_t *event, void *user_data);
typedef struct {
    char ssid[33];
    int8_t rssi;
    uint8_t channel;
    bool secured;
    bool supported;
} sugarat_wifi_network_t;
typedef struct {
    sugarat_wifi_phase_t phase;
    uint32_t revision, generation;
    uint32_t completed_attempt; /* Last DHCP-completed join receipt; zero before a requested join completes. */
    int error, disconnect_reason;
    bool saved, portal;
    char ssid[33], ip[16], ap_ssid[33];
    unsigned count;
    sugarat_wifi_network_t networks[SUGARAT_WIFI_NETWORKS];
    sugarat_wifi_event_t last_event;
    sugarat_wifi_event_t last_connected; /* Retained success notification for consumers created after connection. */
} sugarat_wifi_snapshot_t;
typedef struct {
    const char *ap_prefix;
    uint32_t portal_seconds, connect_seconds;
    unsigned reconnect_attempts;
    sugarat_wifi_event_cb_t on_event;
    void *user_data; /* Must outlive Wi-Fi; no callbacks remain after destroy returns. */
} sugarat_wifi_config_t;

/** Explicit defaults: Sugarat hotspot, 15-minute setup window, 20-second join, three reconnects. */
sugarat_wifi_config_t sugarat_wifi_default_config(void);
/** Own the default Wi-Fi driver/netifs until destroy. NVS must already be initialized.
 * Refuses an already-owned Wi-Fi driver. Does not erase NVS. Loads only this component's namespace.
 * On failure, *out remains NULL and acquired resources are released. */
esp_err_t sugarat_wifi_create(const sugarat_wifi_config_t *config, sugarat_wifi_t **out);
/** Stop HTTP, radio and worker, then release callbacks/netifs. Call after deleting dependent UI.
 * Must not be called from the component's HTTP/event/worker callbacks. */
void sugarat_wifi_destroy(sugarat_wifi_t *wifi);
/** Copy synchronized connection state and the open provisioning hotspot name. */
void sugarat_wifi_snapshot(sugarat_wifi_t *wifi, sugarat_wifi_snapshot_t *out);
/** Queue a scan; no radio or blocking work executes on the caller's UI/HTTP thread. */
esp_err_t sugarat_wifi_scan(sugarat_wifi_t *wifi);
/** Join only an id from the specified scan generation. Credentials are saved only after an IP is obtained.
 * Password is copied before returning, never logged, and limited to valid personal-Wi-Fi lengths. */
esp_err_t sugarat_wifi_connect(sugarat_wifi_t *wifi, uint32_t generation, unsigned id, const char *password);
/** Open/close an open SoftAP portal; joining the setup hotspot requires no password. Opening queues an automatic scan.
 * DHCP and credential-save success close the portal immediately before CONNECTED is delivered; idle portals expire. */
esp_err_t sugarat_wifi_portal(sugarat_wifi_t *wifi, bool open);
/** Forget only this component's saved credentials and disconnect. Never erase the NVS partition. */
esp_err_t sugarat_wifi_forget(sugarat_wifi_t *wifi);
/** Stable public phase name for logs and UI; never contains credentials. */
const char *sugarat_wifi_phase_name(sugarat_wifi_phase_t phase);
#ifdef __cplusplus
}
#endif
