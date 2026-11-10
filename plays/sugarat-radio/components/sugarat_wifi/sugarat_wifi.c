/* Modified for Sugarat Radio by Deotaland, 2026: US channel policy. */
/* SPDX-License-Identifier: MIT
 * One worker owns scans, joins, retry budgets and portal lifetime. Event handlers only publish link facts. */
#include "sugarat_wifi.h"
#include "sugarat_wifi_peer.h"
#include "sugarat_wifi_dns.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs.h"
#include "cJSON.h"
#include "lwip/inet.h"

#define LINK_UP BIT0
#define LINK_DOWN BIT1
#define WORKER_DONE BIT2
static const char *TAG = "sugarat_wifi";
typedef enum { CMD_SCAN, CMD_JOIN, CMD_PORTAL, CMD_FORGET, CMD_STOP } command_kind_t;
typedef struct { command_kind_t kind; bool open; uint32_t attempt; wifi_config_t station; } command_t;
typedef struct { uint32_t version; wifi_config_t station; } saved_config_t;
struct sugarat_wifi {
    sugarat_wifi_config_t config;
    char prefix[20], csrf[33];
    SemaphoreHandle_t mutex;
    QueueHandle_t commands;
    EventGroupHandle_t events;
    TaskHandle_t task;
    esp_netif_t *sta, *ap;
    esp_event_handler_instance_t wifi_handler, ip_handler;
    bool event_loop_owned, driver_owned, started, pending, joining, stopping, ap_enabled;
    unsigned reconnects;
    int64_t join_deadline, retry_at, portal_deadline;
    wifi_config_t candidate;
    wifi_ap_record_t records[SUGARAT_WIFI_NETWORKS];
    sugarat_wifi_snapshot_t state;
    httpd_handle_t server;
    sugarat_wifi_dns_t *dns;
    uint32_t next_attempt, active_attempt;
    uint32_t event_sequence;
};
extern const uint8_t portal_html_start[] asm("_binary_portal_html_start");
extern const uint8_t portal_html_end[] asm("_binary_portal_html_end");
static void lock(sugarat_wifi_t *w) { xSemaphoreTake(w->mutex, portMAX_DELAY); }
static void unlock(sugarat_wifi_t *w) { xSemaphoreGive(w->mutex); }
static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }
static void emit(sugarat_wifi_t *w, sugarat_wifi_event_id_t id, esp_err_t error) {
    sugarat_wifi_event_t event = { .id = id, .error = error, .attempt = w->active_attempt };
    lock(w);
    if (id == SUGARAT_WIFI_EVENT_CONNECTED && (!w->state.ip[0] || !w->state.saved || w->state.portal)) { unlock(w); return; }
    event.sequence = ++w->event_sequence; event.saved = w->state.saved; event.portal = w->state.portal;
    event.disconnect_reason = w->state.disconnect_reason;
    memcpy(event.ssid, w->state.ssid, sizeof(event.ssid)); memcpy(event.ip, w->state.ip, sizeof(event.ip));
    w->state.last_event = event; w->state.revision++;
    if (id == SUGARAT_WIFI_EVENT_CONNECTED) w->state.last_connected = event;
    unlock(w);
    ESP_LOGI(TAG, "event=%u sequence=%lu attempt=%lu error=%d portal=%u", (unsigned)id, (unsigned long)event.sequence, (unsigned long)event.attempt, error, (unsigned)event.portal);
    if (w->config.on_event) w->config.on_event(w, &event, w->config.user_data);
}
static void wipe(void *data, size_t bytes) { volatile uint8_t *p = data; while (bytes--) *p++ = 0; }
static void phase(sugarat_wifi_t *w, sugarat_wifi_phase_t value, esp_err_t error) {
    lock(w); w->state.phase = value; w->state.error = error; w->state.revision++; int reason = w->state.disconnect_reason; unlock(w);
    ESP_LOGI(TAG, "state=%s error=%d disconnect_reason=%d", sugarat_wifi_phase_name(value), error, reason);
}
const char *sugarat_wifi_phase_name(sugarat_wifi_phase_t value) {
    static const char *names[] = {"idle", "scanning", "connecting", "connected", "failed", "stopped"};
    return value <= SW_STOPPED ? names[value] : "failed";
}
sugarat_wifi_config_t sugarat_wifi_default_config(void) {
    return (sugarat_wifi_config_t){ .ap_prefix = "Sugarat", .portal_seconds = 900, .connect_seconds = 20, .reconnect_attempts = 3 };
}
void sugarat_wifi_snapshot(sugarat_wifi_t *w, sugarat_wifi_snapshot_t *out) { lock(w); *out = w->state; unlock(w); }
static void link_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    sugarat_wifi_t *w = arg;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = data;
        lock(w); snprintf(w->state.ip, sizeof(w->state.ip), IPSTR, IP2STR(&event->ip_info.ip)); unlock(w);
        xEventGroupSetBits(w->events, LINK_UP);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = data;
        lock(w); w->state.disconnect_reason = event->reason; w->state.ip[0] = 0; memset(&w->state.last_connected, 0, sizeof(w->state.last_connected)); unlock(w);
        ESP_LOGW(TAG, "station_disconnected reason=%u", (unsigned)event->reason);
        xEventGroupSetBits(w->events, LINK_DOWN);
    }
}
static esp_err_t enqueue(sugarat_wifi_t *w, command_t *command) {
    lock(w);
    if (w->stopping || w->pending || (w->state.phase == SW_CONNECTING && command->kind != CMD_STOP)) { unlock(w); return ESP_ERR_INVALID_STATE; }
    w->pending = true;
    unlock(w);
    if (xQueueSend(w->commands, command, 0) == pdTRUE) return ESP_OK;
    lock(w); w->pending = false; unlock(w);
    return ESP_ERR_NO_MEM;
}
esp_err_t sugarat_wifi_scan(sugarat_wifi_t *w) { command_t c = { .kind = CMD_SCAN }; return enqueue(w, &c); }
esp_err_t sugarat_wifi_portal(sugarat_wifi_t *w, bool open) { command_t c = { .kind = CMD_PORTAL, .open = open }; return enqueue(w, &c); }
esp_err_t sugarat_wifi_forget(sugarat_wifi_t *w) { command_t c = { .kind = CMD_FORGET }; return enqueue(w, &c); }
static bool supported(wifi_auth_mode_t auth) { return auth == WIFI_AUTH_OPEN || auth == WIFI_AUTH_WPA2_PSK || auth == WIFI_AUTH_WPA_WPA2_PSK || auth == WIFI_AUTH_WPA3_PSK || auth == WIFI_AUTH_WPA2_WPA3_PSK || auth == WIFI_AUTH_WPA_PSK; }
static void resolve_station(wifi_config_t *station) {
    /* Selection identifies an SSID; all radios serving that network remain eligible. */
    station->sta.bssid_set = false; memset(station->sta.bssid, 0, sizeof(station->sta.bssid));
    station->sta.channel = 0; station->sta.scan_method = WIFI_ALL_CHANNEL_SCAN; station->sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    station->sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    if (station->sta.password[0] && station->sta.threshold.authmode == WIFI_AUTH_OPEN) station->sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
}
static esp_err_t connect_request(sugarat_wifi_t *w, uint32_t generation, unsigned id, const char *password, uint32_t *attempt) {
    command_t c = { .kind = CMD_JOIN };
    size_t bytes = strlen(password);
    lock(w);
    if (generation != w->state.generation || id >= w->state.count || !w->state.networks[id].supported) { unlock(w); return ESP_ERR_INVALID_ARG; }
    bool secured = w->state.networks[id].secured;
    if (w->next_attempt == UINT32_MAX) { unlock(w); return ESP_ERR_INVALID_STATE; }
    if ((secured && (bytes < 8 || bytes > 63)) || (!secured && bytes != 0)) { unlock(w); return ESP_ERR_INVALID_ARG; }
    memcpy(c.station.sta.ssid, w->records[id].ssid, sizeof(c.station.sta.ssid));
    c.station.sta.threshold.authmode = !secured ? WIFI_AUTH_OPEN : w->records[id].authmode == WIFI_AUTH_WPA_PSK ? WIFI_AUTH_WPA_PSK : WIFI_AUTH_WPA2_PSK;
    c.station.sta.pmf_cfg.capable = true;
    memcpy(c.station.sta.password, password, bytes);
    resolve_station(&c.station);
    c.attempt = ++w->next_attempt;
    ESP_LOGI(TAG, "network_selected authmode=%u channel=%u rssi=%d", (unsigned)w->records[id].authmode, (unsigned)w->records[id].primary, (int)w->records[id].rssi);
    unlock(w);
    esp_err_t err = enqueue(w, &c);
    if (err == ESP_OK && attempt) *attempt = c.attempt;
    wipe(&c, sizeof(c));
    return err;
}
esp_err_t sugarat_wifi_connect(sugarat_wifi_t *w, uint32_t generation, unsigned id, const char *password) {
    return connect_request(w, generation, id, password, NULL);
}
static esp_err_t storage_write(sugarat_wifi_t *w) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("sugarat_wifi", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    saved_config_t value = { .version = 1, .station = w->candidate };
    err = nvs_set_blob(handle, "station", &value, sizeof(value));
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle); wipe(&value, sizeof(value));
    return err;
}
static void scan(sugarat_wifi_t *w) {
    sugarat_wifi_snapshot_t before; sugarat_wifi_snapshot(w, &before);
    phase(w, SW_SCANNING, ESP_OK);
    wifi_scan_config_t config = { .show_hidden = false, .scan_type = WIFI_SCAN_TYPE_ACTIVE, .scan_time.active = { .min = 80, .max = 120 } };
    esp_err_t err = esp_wifi_scan_start(&config, true);
    wifi_ap_record_t *found = NULL;
    uint16_t count = 0;
    if (err == ESP_OK) err = esp_wifi_scan_get_ap_num(&count);
    if (err == ESP_OK && count > 0) {
        if (count > 64) count = 64;
        found = calloc(count, sizeof(*found));
        if (!found) err = ESP_ERR_NO_MEM;
        else err = esp_wifi_scan_get_ap_records(&count, found);
    }
    /* Release driver-owned scan records even after allocation/scan-result failures. */
    esp_wifi_clear_ap_list();
    if (err == ESP_OK) {
        lock(w); w->state.count = 0; w->state.generation++;
        for (unsigned i = 0; i < count && w->state.count < SUGARAT_WIFI_NETWORKS; ++i) {
            if (!found[i].ssid[0]) continue;
            bool duplicate = false;
            for (unsigned j = 0; j < w->state.count; ++j) if (!memcmp(w->records[j].ssid, found[i].ssid, 32)) duplicate = true;
            if (duplicate) continue;
            unsigned j = w->state.count++;
            w->records[j] = found[i];
            sugarat_wifi_network_t *network = &w->state.networks[j];
            memcpy(network->ssid, found[i].ssid, 32); network->ssid[32] = 0;
            network->rssi = found[i].rssi; network->channel = found[i].primary;
            network->secured = found[i].authmode != WIFI_AUTH_OPEN; network->supported = supported(found[i].authmode);
        }
        ESP_LOGI(TAG, "scan_complete count=%u generation=%lu", w->state.count, (unsigned long)w->state.generation);
        unlock(w);
    }
    free(found);
    phase(w, err == ESP_OK ? (before.phase == SW_CONNECTED ? SW_CONNECTED : SW_IDLE) : SW_FAILED, err);
}
static bool local_client(httpd_req_t *req, sugarat_wifi_t *w) {
    if (!w->ap) return false;
    struct sockaddr_storage peer; socklen_t bytes = sizeof(peer);
    esp_netif_ip_info_t info;
    if (getpeername(httpd_req_to_sockfd(req), (struct sockaddr *)&peer, &bytes) != 0 || esp_netif_get_ip_info(w->ap, &info) != ESP_OK) return false;
    if (peer.ss_family == AF_INET && bytes >= sizeof(struct sockaddr_in)) {
        const struct sockaddr_in *ipv4 = (const struct sockaddr_in *)&peer;
        return sugarat_wifi_peer_on_subnet((const uint8_t *)&ipv4->sin_addr.s_addr, 4, info.ip.addr, info.netmask.addr);
    }
#if LWIP_IPV6
    if (peer.ss_family == AF_INET6 && bytes >= sizeof(struct sockaddr_in6)) {
        const struct sockaddr_in6 *ipv6 = (const struct sockaddr_in6 *)&peer;
        return sugarat_wifi_peer_on_subnet(ipv6->sin6_addr.s6_addr, 16, info.ip.addr, info.netmask.addr);
    }
#endif
    return false;
}
static esp_err_t respond(httpd_req_t *req, const char *status, const char *json) {
    httpd_resp_set_status(req, status); httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store"); httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    return httpd_resp_sendstr(req, json);
}
static esp_err_t captive_redirect(httpd_req_t *req, httpd_err_code_t error);
static esp_err_t portal_get(httpd_req_t *req) {
    sugarat_wifi_t *w = req->user_ctx;
    if (!local_client(req, w)) return respond(req, "403 Forbidden", "{\"error\":\"local_only\"}");
    if (!strcmp(req->uri, "/")) {
        char host[80];
        if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) != ESP_OK
            || (strcmp(host, "192.168.4.1") && strcmp(host, "192.168.4.1:80"))) return captive_redirect(req, HTTPD_404_NOT_FOUND);
        ESP_LOGI(TAG, "portal_page_served");
        httpd_resp_set_type(req, "text/html; charset=utf-8");
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        httpd_resp_set_hdr(req, "Content-Security-Policy", "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; frame-ancestors 'none'; base-uri 'none'");
        return httpd_resp_send(req, (const char *)portal_html_start, portal_html_end - portal_html_start - 1);
    }
    sugarat_wifi_snapshot_t s; sugarat_wifi_snapshot(w, &s);
    cJSON *root = cJSON_CreateObject(), *networks = cJSON_AddArrayToObject(root, "networks");
    if (!root || !networks) { cJSON_Delete(root); return respond(req, "503 Service Unavailable", "{\"error\":\"memory\"}"); }
    cJSON_AddStringToObject(root, "state", sugarat_wifi_phase_name(s.phase));
    cJSON_AddNumberToObject(root, "error", s.error); cJSON_AddNumberToObject(root, "reason", s.disconnect_reason);
    cJSON_AddNumberToObject(root, "generation", s.generation); cJSON_AddBoolToObject(root, "saved", s.saved);
    cJSON_AddNumberToObject(root, "completedAttempt", s.completed_attempt);
    cJSON_AddStringToObject(root, "ip", s.ip); cJSON_AddStringToObject(root, "ssid", s.ssid);
    cJSON_AddStringToObject(root, "token", w->csrf);
    for (unsigned i = 0; i < s.count; ++i) {
        cJSON *entry = cJSON_CreateObject(); if (!entry) continue;
        cJSON_AddNumberToObject(entry, "id", i); cJSON_AddStringToObject(entry, "ssid", s.networks[i].ssid);
        cJSON_AddNumberToObject(entry, "rssi", s.networks[i].rssi); cJSON_AddBoolToObject(entry, "secured", s.networks[i].secured);
        cJSON_AddBoolToObject(entry, "supported", s.networks[i].supported); cJSON_AddItemToArray(networks, entry);
    }
    char *json = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    if (!json) return respond(req, "503 Service Unavailable", "{\"error\":\"memory\"}");
    esp_err_t err = respond(req, "200 OK", json); free(json); return err;
}
static esp_err_t portal_post(httpd_req_t *req) {
    sugarat_wifi_t *w = req->user_ctx;
    if (!local_client(req, w)) return respond(req, "403 Forbidden", "{\"error\":\"local_only\"}");
    char token[40], type[40], origin[80];
    if (httpd_req_get_hdr_value_str(req, "X-Sugarat-Token", token, sizeof(token)) != ESP_OK || strcmp(token, w->csrf)
        || httpd_req_get_hdr_value_str(req, "Content-Type", type, sizeof(type)) != ESP_OK || strcmp(type, "application/json"))
        return respond(req, "403 Forbidden", "{\"error\":\"request_denied\"}");
    if (httpd_req_get_hdr_value_len(req, "Origin") > 0 && (httpd_req_get_hdr_value_str(req, "Origin", origin, sizeof(origin)) != ESP_OK || strcmp(origin, "http://192.168.4.1")))
        return respond(req, "403 Forbidden", "{\"error\":\"origin_denied\"}");
    if (req->content_len <= 0 || req->content_len > 256) return respond(req, "400 Bad Request", "{\"error\":\"invalid_request\"}");
    char body[257] = {0}; int offset = 0;
    while (offset < req->content_len) {
        int read = httpd_req_recv(req, body + offset, req->content_len - offset);
        if (read <= 0) { wipe(body, sizeof(body)); return ESP_FAIL; }
        offset += read;
    }
    const char *end = NULL;
    cJSON *json = cJSON_ParseWithLengthOpts(body, offset + 1, &end, true); wipe(body, sizeof(body));
    if (!cJSON_IsObject(json)) { cJSON_Delete(json); return respond(req, "400 Bad Request", "{\"error\":\"invalid_request\"}"); }
    esp_err_t err = ESP_ERR_INVALID_ARG; uint32_t attempt = 0;
    if (!strcmp(req->uri, "/api/scan")) err = sugarat_wifi_scan(w);
    else if (json && !strcmp(req->uri, "/api/connect")) {
        cJSON *id = cJSON_GetObjectItemCaseSensitive(json, "id"), *generation = cJSON_GetObjectItemCaseSensitive(json, "generation"), *password = cJSON_GetObjectItemCaseSensitive(json, "password");
        if (cJSON_IsNumber(id) && id->valuedouble >= 0 && id->valuedouble < SUGARAT_WIFI_NETWORKS && id->valuedouble == id->valueint
            && cJSON_IsNumber(generation) && generation->valuedouble >= 0 && generation->valuedouble <= UINT32_MAX && generation->valuedouble == (uint32_t)generation->valuedouble
            && cJSON_IsString(password)) err = connect_request(w, generation->valuedouble, id->valueint, password->valuestring, &attempt);
        if (cJSON_IsString(password)) wipe(password->valuestring, strlen(password->valuestring));
    }
    cJSON_Delete(json);
    if (err == ESP_OK) {
        char receipt[64]; snprintf(receipt, sizeof(receipt), "{\"accepted\":true,\"attempt\":%lu}", (unsigned long)attempt);
        return respond(req, "202 Accepted", receipt);
    }
    return respond(req, err == ESP_ERR_INVALID_STATE ? "409 Conflict" : "400 Bad Request", err == ESP_ERR_INVALID_STATE ? "{\"error\":\"busy\"}" : "{\"error\":\"invalid_selection_or_password\"}");
}
static void retain_context(void *context) { (void)context; }
static esp_err_t portal_session(httpd_handle_t server, int socket) {
    sugarat_wifi_t *w = httpd_get_global_user_ctx(server);
    struct ifreq interface = {0};
    if (esp_netif_get_netif_impl_name(w->ap, interface.ifr_name) != ESP_OK) return ESP_FAIL;
    /* Bind every accepted stream to SoftAP even when the home LAN uses the same IP subnet. */
    return setsockopt(socket, SOL_SOCKET, SO_BINDTODEVICE, &interface, sizeof(interface)) == 0 ? ESP_OK : ESP_FAIL;
}
static esp_err_t close_portal(sugarat_wifi_t *w) {
    bool opened = w->server || w->dns || w->ap_enabled;
    if (opened && w->started) {
        esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
        if (err != ESP_OK) { emit(w, SUGARAT_WIFI_EVENT_PORTAL_FAILED, err); return err; }
        w->ap_enabled = false;
    }
    lock(w); w->state.portal = false; w->state.revision++; unlock(w);
    sugarat_wifi_dns_stop(w->dns); w->dns = NULL;
    if (w->server) { esp_err_t err = httpd_stop(w->server); if (err != ESP_OK) { emit(w, SUGARAT_WIFI_EVENT_PORTAL_FAILED, err); return err; } w->server = NULL; }
    if (opened) { ESP_LOGI(TAG, "portal_closed"); emit(w, SUGARAT_WIFI_EVENT_PORTAL_CLOSED, ESP_OK); }
    return ESP_OK;
}
static esp_err_t captive_redirect(httpd_req_t *req, httpd_err_code_t error) {
    (void)error;
    sugarat_wifi_t *w = httpd_get_global_user_ctx(req->handle);
    if (!local_client(req, w)) return respond(req, "403 Forbidden", "{\"error\":\"local_only\"}");
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    ESP_LOGI(TAG, "captive_probe_redirected");
    return httpd_resp_sendstr(req, "<a href=\"http://192.168.4.1/\">Open Sugarat Wi-Fi setup</a>");
}
static esp_err_t open_portal(sugarat_wifi_t *w) {
    if (w->server) { w->portal_deadline = now_ms() + w->config.portal_seconds * 1000; return ESP_OK; }
    uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    wifi_config_t ap = {0};
    snprintf((char *)ap.ap.ssid, sizeof(ap.ap.ssid), "%s-%02X%02X", w->prefix, mac[4], mac[5]);
    ap.ap.ssid_len = strlen((char *)ap.ap.ssid); ap.ap.authmode = WIFI_AUTH_OPEN; ap.ap.max_connection = 2; ap.ap.channel = 1;
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err == ESP_OK) w->ap_enabled = true;
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_AP, &ap);
    if (err != ESP_OK) { esp_err_t closed = close_portal(w); wipe(&ap, sizeof(ap)); return closed == ESP_OK ? err : closed; }
    uint8_t nonce[16]; esp_fill_random(nonce, sizeof(nonce));
    for (unsigned i = 0; i < 16; ++i) snprintf(w->csrf + i * 2, 3, "%02x", nonce[i]);
    httpd_config_t config = HTTPD_DEFAULT_CONFIG(); config.stack_size = 8192; config.max_uri_handlers = 5; config.recv_wait_timeout = 5;
    config.global_user_ctx = w; config.global_user_ctx_free_fn = retain_context; config.open_fn = portal_session;
    err = httpd_start(&w->server, &config);
    if (err == ESP_OK) {
        httpd_uri_t routes[] = { {.uri="/",.method=HTTP_GET,.handler=portal_get,.user_ctx=w}, {.uri="/api/status",.method=HTTP_GET,.handler=portal_get,.user_ctx=w},
            {.uri="/api/scan",.method=HTTP_POST,.handler=portal_post,.user_ctx=w}, {.uri="/api/connect",.method=HTTP_POST,.handler=portal_post,.user_ctx=w} };
        for (unsigned i = 0; i < sizeof(routes)/sizeof(routes[0]) && err == ESP_OK; ++i) err = httpd_register_uri_handler(w->server, &routes[i]);
    }
    if (err == ESP_OK) err = httpd_register_err_handler(w->server, HTTPD_404_NOT_FOUND, captive_redirect);
    if (err == ESP_OK) err = sugarat_wifi_dns_start(w->ap, &w->dns);
    if (err == ESP_OK) {
        w->portal_deadline = now_ms() + w->config.portal_seconds * 1000;
        lock(w); w->state.portal = true; strcpy(w->state.ap_ssid, (char *)ap.ap.ssid); w->state.revision++; unlock(w);
        ESP_LOGI(TAG, "portal_ready url=http://192.168.4.1");
        emit(w, SUGARAT_WIFI_EVENT_PORTAL_OPENED, ESP_OK);
    } else { esp_err_t closed = close_portal(w); if (closed != ESP_OK) err = closed; }
    wipe(&ap, sizeof(ap)); return err;
}
static void begin_join(sugarat_wifi_t *w) {
    w->joining = false;
    lock(w); memset(&w->state.last_connected, 0, sizeof(w->state.last_connected)); unlock(w);
    wifi_ap_record_t current;
    if (esp_wifi_sta_get_ap_info(&current) == ESP_OK) {
        esp_wifi_disconnect();
        xEventGroupWaitBits(w->events, LINK_DOWN, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));
    }
    xEventGroupClearBits(w->events, LINK_UP | LINK_DOWN);
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &w->candidate);
    if (err == ESP_OK) err = esp_wifi_connect();
    if (err == ESP_OK) {
        w->joining = true; w->join_deadline = now_ms() + w->config.connect_seconds * 1000;
        lock(w); snprintf(w->state.ssid, sizeof(w->state.ssid), "%.32s", w->candidate.sta.ssid); w->state.ip[0] = 0; w->state.saved = false; w->state.disconnect_reason = 0; unlock(w);
        ESP_LOGI(TAG, "join_started bssid_locked=%u scan_method=%u auth_floor=%u sae=%u", (unsigned)w->candidate.sta.bssid_set, (unsigned)w->candidate.sta.scan_method, (unsigned)w->candidate.sta.threshold.authmode, (unsigned)w->candidate.sta.sae_pwe_h2e);
        phase(w, SW_CONNECTING, ESP_OK);
        emit(w, SUGARAT_WIFI_EVENT_CONNECTING, ESP_OK);
    } else { phase(w, SW_FAILED, err); emit(w, SUGARAT_WIFI_EVENT_FAILED, err); }
}
static void worker(void *arg) {
    sugarat_wifi_t *w = arg;
    saved_config_t saved = {0}; nvs_handle_t handle;
    if (nvs_open("sugarat_wifi", NVS_READONLY, &handle) == ESP_OK) {
        size_t bytes = sizeof(saved); esp_err_t err = nvs_get_blob(handle, "station", &saved, &bytes); nvs_close(handle);
        if (err == ESP_OK && bytes == sizeof(saved) && saved.version == 1 && saved.station.sta.ssid[0]) { w->candidate = saved.station; resolve_station(&w->candidate); begin_join(w); }
    }
    wipe(&saved, sizeof(saved));
    if (!w->joining) scan(w);
    for (;;) {
        command_t c = {0};
        if (xQueueReceive(w->commands, &c, pdMS_TO_TICKS(200)) == pdTRUE) {
            if (c.kind == CMD_STOP) { wipe(&c,sizeof(c)); break; }
            if (c.kind == CMD_SCAN) scan(w);
            else if (c.kind == CMD_JOIN) { w->candidate = c.station; w->active_attempt = c.attempt; w->reconnects = 0; w->retry_at = 0; begin_join(w); }
            else if (c.kind == CMD_PORTAL) {
                esp_err_t err = c.open ? open_portal(w) : close_portal(w);
                if (err != ESP_OK) phase(w, SW_FAILED, err);
                else if (c.open) scan(w);
                else {
                    sugarat_wifi_snapshot_t s; sugarat_wifi_snapshot(w, &s);
                    if (s.ip[0] && s.saved && s.last_connected.id != SUGARAT_WIFI_EVENT_CONNECTED) {
                        phase(w, SW_CONNECTED, ESP_OK); emit(w, SUGARAT_WIFI_EVENT_CONNECTED, ESP_OK);
                    }
                }
            }
            else if (c.kind == CMD_FORGET) {
                esp_err_t err = nvs_open("sugarat_wifi", NVS_READWRITE, &handle);
                if (err == ESP_OK) { err = nvs_erase_key(handle, "station"); if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK; if (err == ESP_OK) err = nvs_commit(handle); nvs_close(handle); }
                if (err == ESP_OK) { w->joining = false; w->retry_at = 0; wipe(&w->candidate, sizeof(w->candidate)); esp_wifi_disconnect(); lock(w); w->state.saved = false; w->state.ssid[0] = 0; w->state.ip[0] = 0; unlock(w); }
                phase(w, err == ESP_OK ? SW_IDLE : SW_FAILED, err);
            }
            wipe(&c, sizeof(c)); lock(w); w->pending = false; unlock(w);
        }
        EventBits_t bits = xEventGroupClearBits(w->events, LINK_UP | LINK_DOWN);
        if ((bits & (LINK_UP | LINK_DOWN)) == (LINK_UP | LINK_DOWN)) {
            lock(w); bool connected = w->state.ip[0] != 0; unlock(w);
            bits = connected ? LINK_UP : LINK_DOWN;
        }
        if (bits & LINK_UP) {
            w->joining = false; w->retry_at = 0; w->reconnects = 0;
            esp_err_t err = storage_write(w);
            lock(w); w->state.saved = err == ESP_OK; w->state.completed_attempt = w->active_attempt; unlock(w);
            phase(w, SW_CONNECTED, err);
            if (err != ESP_OK) emit(w, SUGARAT_WIFI_EVENT_STORAGE_FAILED, err);
            else {
                err = close_portal(w);
                if (err == ESP_OK) emit(w, SUGARAT_WIFI_EVENT_CONNECTED, ESP_OK);
                else phase(w, SW_FAILED, err);
            }
        } else if (bits & LINK_DOWN) {
            sugarat_wifi_snapshot_t s; sugarat_wifi_snapshot(w, &s);
            emit(w, SUGARAT_WIFI_EVENT_DISCONNECTED, ESP_ERR_WIFI_NOT_CONNECT);
            if (w->joining || s.phase == SW_CONNECTED) {
                w->joining = false;
                if (w->reconnects++ < w->config.reconnect_attempts) { w->retry_at = now_ms() + 1500; phase(w, SW_CONNECTING, ESP_OK); }
                else { w->retry_at = 0; phase(w, SW_FAILED, ESP_ERR_WIFI_NOT_CONNECT); emit(w, SUGARAT_WIFI_EVENT_FAILED, ESP_ERR_WIFI_NOT_CONNECT); }
            }
        }
        if (w->joining && now_ms() > w->join_deadline) { w->joining = false; esp_wifi_disconnect(); phase(w, SW_FAILED, ESP_ERR_TIMEOUT); emit(w, SUGARAT_WIFI_EVENT_FAILED, ESP_ERR_TIMEOUT); }
        if (w->retry_at && now_ms() >= w->retry_at) { w->retry_at = 0; begin_join(w); }
        if (w->server && now_ms() > w->portal_deadline) { esp_err_t err = close_portal(w); if (err != ESP_OK) phase(w, SW_FAILED, err); }
    }
    ESP_ERROR_CHECK(close_portal(w));
    phase(w, SW_STOPPED, ESP_OK);
    xEventGroupSetBits(w->events, WORKER_DONE);
    vTaskDelete(NULL);
}
void sugarat_wifi_destroy(sugarat_wifi_t *w) {
    if (!w) return;
    if (w->task) {
        lock(w); w->stopping = true; unlock(w);
        command_t c = { .kind = CMD_STOP }; xQueueSend(w->commands, &c, portMAX_DELAY);
        xEventGroupWaitBits(w->events, WORKER_DONE, pdFALSE, pdTRUE, portMAX_DELAY);
    }
    if (w->started) esp_wifi_stop();
    if (w->wifi_handler) esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, w->wifi_handler);
    if (w->ip_handler) esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, w->ip_handler);
    if (w->driver_owned) esp_wifi_deinit();
    if (w->sta) esp_netif_destroy_default_wifi(w->sta);
    if (w->ap) esp_netif_destroy_default_wifi(w->ap);
    if (w->event_loop_owned) esp_event_loop_delete_default();
    if (w->commands) { command_t c; while (xQueueReceive(w->commands, &c, 0) == pdTRUE) wipe(&c, sizeof(c)); vQueueDelete(w->commands); }
    if (w->mutex) vSemaphoreDelete(w->mutex);
    if (w->events) vEventGroupDelete(w->events);
    wipe(w, sizeof(*w)); free(w);
}
esp_err_t sugarat_wifi_create(const sugarat_wifi_config_t *config, sugarat_wifi_t **out) {
    *out = NULL;
    if (!config || !config->ap_prefix || strlen(config->ap_prefix) < 1 || strlen(config->ap_prefix) > 19 || config->portal_seconds < 60 || config->portal_seconds > 3600 || config->connect_seconds < 5 || config->connect_seconds > 120 || config->reconnect_attempts > 10) return ESP_ERR_INVALID_ARG;
    if (esp_netif_get_handle_from_ifkey("WIFI_STA_DEF") || esp_netif_get_handle_from_ifkey("WIFI_AP_DEF")) return ESP_ERR_INVALID_STATE;
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) != ESP_ERR_WIFI_NOT_INIT) return ESP_ERR_INVALID_STATE;
    sugarat_wifi_t *w = calloc(1, sizeof(*w)); if (!w) return ESP_ERR_NO_MEM;
    w->config = *config; strcpy(w->prefix, config->ap_prefix); w->config.ap_prefix = w->prefix;
    w->mutex = xSemaphoreCreateMutex(); w->events = xEventGroupCreate(); w->commands = xQueueCreate(2, sizeof(command_t));
    esp_err_t err = ESP_ERR_NO_MEM;
    if (!w->mutex || !w->events || !w->commands) goto fail;
    err = esp_netif_init(); if (err != ESP_OK) goto fail;
    err = esp_event_loop_create_default(); if (err == ESP_OK) w->event_loop_owned = true; else if (err != ESP_ERR_INVALID_STATE) goto fail;
    w->sta = esp_netif_create_default_wifi_sta(); w->ap = esp_netif_create_default_wifi_ap();
    if (!w->sta || !w->ap) { err = ESP_ERR_NO_MEM; goto fail; }
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init); if (err != ESP_OK) goto fail; w->driver_owned = true;
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM); if (err != ESP_OK) goto fail;
    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, link_event, w, &w->wifi_handler); if (err != ESP_OK) goto fail;
    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, link_event, w, &w->ip_handler); if (err != ESP_OK) goto fail;
    err = esp_wifi_set_mode(WIFI_MODE_STA); if (err != ESP_OK) goto fail;
#if CONFIG_SUGARAT_WIFI_US_CHANNELS
    wifi_country_t country = { .cc = "US", .schan = 1, .nchan = 11,
        .max_tx_power = CONFIG_SUGARAT_WIFI_MAX_TX_POWER_DBM, .policy = WIFI_COUNTRY_POLICY_MANUAL };
    err = esp_wifi_set_country(&country); if (err != ESP_OK) goto fail;
#endif
    err = esp_wifi_start(); if (err != ESP_OK) goto fail; w->started = true;
    err = esp_wifi_set_max_tx_power(CONFIG_SUGARAT_WIFI_MAX_TX_POWER_DBM * 4); if (err != ESP_OK) goto fail;
    int8_t power;
    err = esp_wifi_get_max_tx_power(&power); if (err != ESP_OK) goto fail;
    ESP_LOGI(TAG, "radio_ready max_tx_power_quarter_dbm=%d", (int)power);
    if (xTaskCreate(worker, "sugarat_wifi", 6144, w, 4, &w->task) != pdPASS) { err = ESP_ERR_NO_MEM; goto fail; }
    *out = w; return ESP_OK;
fail:
    sugarat_wifi_destroy(w); return err;
}
