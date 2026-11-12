/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 */
#include "community.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "sdkconfig.h"

static const char *TAG = "community";
#define JSON_LIMIT   (64 * 1024)
#define CHUNK_BYTES  (8 * 1024)
#define ATTEMPTS     4
#define TIMEOUT_MS   15000
#define COVER_LIMIT  (256 * 1024)

static void *psram_malloc(size_t size) { return heap_caps_malloc_prefer(size, 2, MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT); }

static void json_hooks(void) {
    static bool done;
    if (done) return;
    cJSON_Hooks hooks = { .malloc_fn = psram_malloc, .free_fn = free };
    cJSON_InitHooks(&hooks);
    done = true;
}

static esp_http_client_handle_t client_for(const char *url, esp_http_client_method_t method) {
    esp_http_client_config_t config = {
        .url = url,
        .method = method,
        .timeout_ms = TIMEOUT_MS,
        .buffer_size = 2048,
        .buffer_size_tx = 1024,
        .keep_alive_enable = false,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client) esp_http_client_set_header(client, "Accept", "application/json");
    return client;
}

/* Perform a request without a body and return the JSON response body (caller frees). */
static community_status_t request_json(const char *url, esp_http_client_method_t method, const char *idempotency,
                                       int *status, char **body) {
    *body = NULL;
    esp_http_client_handle_t client = client_for(url, method);
    if (!client) return COMMUNITY_OFFLINE;
    if (idempotency) esp_http_client_set_header(client, "Idempotency-Key", idempotency);
    community_status_t result = COMMUNITY_OFFLINE;
    if (esp_http_client_open(client, 0) == ESP_OK && esp_http_client_fetch_headers(client) >= 0) {
        *status = esp_http_client_get_status_code(client);
        char *buf = psram_malloc(JSON_LIMIT + 1);
        int len = 0, n = 0;
        while (buf && len < JSON_LIMIT && (n = esp_http_client_read(client, buf + len, JSON_LIMIT - len)) > 0) len += n;
        if (buf && n >= 0) {
            buf[len] = 0;
            *body = buf;
            result = COMMUNITY_OK;
        } else {
            free(buf);
        }
    }
    esp_http_client_cleanup(client);
    return result;
}

static const char *json_text(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static bool json_u32(const cJSON *object, const char *name, uint32_t *out) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    if (cJSON_IsNumber(item) && item->valuedouble >= 0) { *out = (uint32_t)item->valuedouble; return true; }
    if (cJSON_IsString(item) && item->valuestring[0]) { *out = strtoul(item->valuestring, NULL, 10); return true; }
    return false;
}

static void copy_utf8(char *dst, size_t cap, const char *src) {
    if (!src) src = "";
    size_t n = strnlen(src, cap - 1);
    while (n > 0 && ((unsigned char)src[n] & 0xC0) == 0x80) n--;
    memcpy(dst, src, n);
    dst[n] = 0;
}

static bool parse_sha256(const char *hex, uint8_t out[32]) {
    if (!hex || strlen(hex) != 64) return false;
    for (int i = 0; i < 32; i++) {
        unsigned value;
        if (sscanf(hex + i * 2, "%2x", &value) != 1) return false;
        out[i] = (uint8_t)value;
    }
    return true;
}

community_status_t community_fetch_play(uint32_t play_id, community_play_t *out) {
    json_hooks();
    char url[160];
    snprintf(url, sizeof url, "%s/api/v1/community/plays/%lu", CONFIG_LAUNCHER_COMMUNITY_URL, (unsigned long)play_id);
    int status = 0;
    char *body;
    community_status_t result = request_json(url, HTTP_METHOD_GET, NULL, &status, &body);
    if (result != COMMUNITY_OK) return result;
    if (status == 404 || status == 400) { free(body); return COMMUNITY_NOT_FOUND; }
    if (status != 200) { ESP_LOGW(TAG, "play %lu: HTTP %d", (unsigned long)play_id, status); free(body); return COMMUNITY_SERVER; }
    cJSON *root = cJSON_Parse(body);
    free(body);
    const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(data, "publishedVersion");
    const cJSON *firmware = cJSON_GetObjectItemCaseSensitive(version, "firmware");
    const char *filename = json_text(firmware, "filename");
    memset(out, 0, sizeof *out);
    out->play_id = play_id;
    bool ok = data && version && firmware &&
              json_u32(version, "id", &out->version_id) &&
              json_u32(firmware, "sizeBytes", &out->size) &&
              json_u32(cJSON_GetObjectItemCaseSensitive(version, "product"), "id", &out->product_id) &&
              json_u32(cJSON_GetObjectItemCaseSensitive(version, "hardwareRevision"), "id", &out->hardware_revision_id) &&
              parse_sha256(json_text(firmware, "sha256"), out->sha256) &&
              filename && strlen(filename) > 4 && !strcasecmp(filename + strlen(filename) - 4, ".bin") && out->size > 0;
    if (ok) {
        copy_utf8(out->title, sizeof out->title, json_text(data, "title"));
        copy_utf8(out->author, sizeof out->author, json_text(cJSON_GetObjectItemCaseSensitive(data, "author"), "nickname"));
        copy_utf8(out->category, sizeof out->category, json_text(cJSON_GetObjectItemCaseSensitive(data, "category"), "id"));
        copy_utf8(out->version, sizeof out->version, json_text(version, "version"));
        const char *cover = json_text(data, "coverUrl");
        if (cover && cover[0] == '/') snprintf(out->cover_url, sizeof out->cover_url, "%s%s", CONFIG_LAUNCHER_COMMUNITY_URL, cover);
        else if (cover && strlen(cover) < sizeof out->cover_url) strcpy(out->cover_url, cover);
    }
    cJSON_Delete(root);
    if (!ok) ESP_LOGW(TAG, "play %lu: response is missing firmware facts or the file is not a .bin", (unsigned long)play_id);
    return ok ? COMMUNITY_OK : COMMUNITY_FORMAT;
}

static community_status_t request_ticket(const community_play_t *play, char *url, size_t cap) {
    char endpoint[200], key[64];
    snprintf(endpoint, sizeof endpoint, "%s/api/v1/community/plays/%lu/versions/%lu/downloads", CONFIG_LAUNCHER_COMMUNITY_URL,
             (unsigned long)play->play_id, (unsigned long)play->version_id);
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(key, sizeof key, "launcher-%02x%02x%02x-%lu-%08lx", mac[3], mac[4], mac[5], (unsigned long)play->play_id,
             (unsigned long)esp_random());
    int status = 0;
    char *body;
    community_status_t result = request_json(endpoint, HTTP_METHOD_POST, key, &status, &body);
    if (result != COMMUNITY_OK) return result;
    if (status == 404) { free(body); return COMMUNITY_NOT_FOUND; }
    if (status != 200 && status != 201) { ESP_LOGW(TAG, "ticket: HTTP %d", status); free(body); return COMMUNITY_SERVER; }
    cJSON *root = cJSON_Parse(body);
    free(body);
    const char *download = json_text(cJSON_GetObjectItemCaseSensitive(root, "data"), "downloadUrl");
    bool ok = download && strlen(download) < cap;
    if (ok) strcpy(url, download);
    cJSON_Delete(root);
    return ok ? COMMUNITY_OK : COMMUNITY_FORMAT;
}

/* One transfer attempt from *received to the end. */
static community_status_t stream(const char *url, const community_play_t *play, community_sink_t sink, void *context,
                                 volatile const bool *cancel, volatile uint32_t *received, uint8_t *chunk) {
    esp_http_client_handle_t client = client_for(url, HTTP_METHOD_GET);
    if (!client) return COMMUNITY_OFFLINE;
    esp_http_client_set_header(client, "Accept", "application/octet-stream");
    char range[40];
    uint32_t skip = 0;
    if (*received) {
        snprintf(range, sizeof range, "bytes=%lu-", (unsigned long)*received);
        esp_http_client_set_header(client, "Range", range);
    }
    community_status_t result = COMMUNITY_OFFLINE;
    if (esp_http_client_open(client, 0) == ESP_OK && esp_http_client_fetch_headers(client) >= 0) {
        int status = esp_http_client_get_status_code(client);
        if (status == 200) skip = *received;          /* server ignored Range: discard what was already written */
        if (status != 200 && status != 206) { ESP_LOGW(TAG, "download: HTTP %d", status); result = COMMUNITY_SERVER; }
        else {
            result = COMMUNITY_OK;
            while (*received < play->size) {
                if (*cancel) { result = COMMUNITY_CANCELLED; break; }
                int n = esp_http_client_read(client, (char *)chunk, CHUNK_BYTES);
                if (n <= 0) { result = COMMUNITY_OFFLINE; break; }
                uint32_t len = (uint32_t)n, start = 0;
                if (skip) { start = len < skip ? len : skip; skip -= start; }
                if (len - start > play->size - *received) { result = COMMUNITY_FORMAT; break; }
                if (len > start && !sink(chunk + start, len - start, context)) { result = COMMUNITY_SINK; break; }
                *received += len - start;
            }
        }
    }
    esp_http_client_cleanup(client);
    return result;
}

community_status_t community_download(const community_play_t *play, community_sink_t sink, void *context,
                                      volatile const bool *cancel, volatile uint32_t *received) {
    uint8_t *chunk = psram_malloc(CHUNK_BYTES);
    char *url = psram_malloc(512);
    if (!chunk || !url) { free(chunk); free(url); return COMMUNITY_OFFLINE; }
    *received = 0;
    community_status_t result = COMMUNITY_OFFLINE;
    for (int attempt = 0; attempt < ATTEMPTS; attempt++) {
        if (*cancel) { result = COMMUNITY_CANCELLED; break; }
        result = request_ticket(play, url, 512);
        if (result == COMMUNITY_OK) result = stream(url, play, sink, context, cancel, received, chunk);
        if (result == COMMUNITY_OK || result == COMMUNITY_CANCELLED || result == COMMUNITY_SINK ||
            result == COMMUNITY_NOT_FOUND || result == COMMUNITY_FORMAT) break;
        ESP_LOGW(TAG, "download interrupted at %lu/%lu, retrying", (unsigned long)*received, (unsigned long)play->size);
    }
    free(chunk);
    free(url);
    return result;
}

community_status_t community_fetch_cover(const community_play_t *play, uint8_t **jpeg, size_t *len) {
    *jpeg = NULL;
    *len = 0;
    if (!play->cover_url[0]) return COMMUNITY_FORMAT;
    esp_http_client_handle_t client = client_for(play->cover_url, HTTP_METHOD_GET);
    if (!client) return COMMUNITY_OFFLINE;
    esp_http_client_set_header(client, "Accept", "image/jpeg");
    community_status_t result = COMMUNITY_OFFLINE;
    if (esp_http_client_open(client, 0) == ESP_OK && esp_http_client_fetch_headers(client) >= 0) {
        int status = esp_http_client_get_status_code(client);
        uint8_t *buf = status == 200 ? psram_malloc(COVER_LIMIT + 1) : NULL;
        int total = 0, n = 0;
        while (buf && total <= COVER_LIMIT && (n = esp_http_client_read(client, (char *)buf + total, COVER_LIMIT + 1 - total)) > 0) total += n;
        if (status != 200) {
            ESP_LOGW(TAG, "cover: HTTP %d", status);
            result = COMMUNITY_SERVER;
        } else if (buf && n >= 0 && total > 2 && total <= COVER_LIMIT && buf[0] == 0xFF && buf[1] == 0xD8) {
            *jpeg = buf;
            *len = (size_t)total;
            buf = NULL;
            result = COMMUNITY_OK;
        } else if (buf && n >= 0) {
            ESP_LOGW(TAG, "play %lu: cover is not a JPEG of at most 256 KiB", (unsigned long)play->play_id);
            result = COMMUNITY_FORMAT;
        }
        free(buf);
    }
    esp_http_client_cleanup(client);
    return result;
}
