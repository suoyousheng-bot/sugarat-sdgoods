/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include "esp_err.h"
#define HTTP_METHOD_GET 0
struct catalog_http;
typedef struct catalog_http *esp_http_client_handle_t;
typedef struct {
    const char *url, *user_agent;
    int method, timeout_ms, buffer_size;
    void *crt_bundle_attach;
    bool keep_alive_enable;
} esp_http_client_config_t;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config);
int esp_http_client_open(esp_http_client_handle_t client, int size);
int esp_http_client_fetch_headers(esp_http_client_handle_t client);
int esp_http_client_get_status_code(esp_http_client_handle_t client);
int esp_http_client_read(esp_http_client_handle_t client, char *out, int capacity);
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t client);
void esp_http_client_close(esp_http_client_handle_t client);
void esp_http_client_cleanup(esp_http_client_handle_t client);
