/* SPDX-License-Identifier: Apache-2.0 */
/** Host transport adapter for the firmware HTTP handlers; no ESP32 driver emulation. */
#pragma once
#include "card_exchange.h"
#include <stddef.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define portMAX_DELAY 0
typedef void *httpd_handle_t;
typedef struct {
    httpd_handle_t server;
    void *mutex;
    card_profile_t published,pending;
    unsigned request;
    bool queued,finished,failed;
    char token[33],url[128];
} card_network_t;
typedef enum {HTTP_GET,HTTP_POST} httpd_method_t;
typedef struct {
    void *user_ctx;
    const char *uri,*host,*authorization,*body;
    size_t content_len,offset;
    int code;
    char response[18000],location[128],type[64],cache[32],referrer[32];
} httpd_req_t;
typedef struct {const char *uri;httpd_method_t method;esp_err_t (*handler)(httpd_req_t *);void *user_ctx;} httpd_uri_t;
typedef struct {unsigned stack_size,recv_wait_timeout,send_wait_timeout,max_uri_handlers;bool lru_purge_enable;bool (*uri_match_fn)(const char *,const char *,size_t);} httpd_config_t;
#define HTTPD_DEFAULT_CONFIG() ((httpd_config_t){0})
static inline void xSemaphoreTake(void *mutex,int timeout){(void)mutex;(void)timeout;}
static inline void xSemaphoreGive(void *mutex){(void)mutex;}
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *r,const char *key,char *out,size_t size);
esp_err_t httpd_resp_set_status(httpd_req_t *r,const char *value);
esp_err_t httpd_resp_set_type(httpd_req_t *r,const char *value);
esp_err_t httpd_resp_set_hdr(httpd_req_t *r,const char *key,const char *value);
esp_err_t httpd_resp_send(httpd_req_t *r,const char *body,ptrdiff_t size);
esp_err_t httpd_resp_sendstr(httpd_req_t *r,const char *body);
int httpd_req_recv(httpd_req_t *r,char *out,size_t size);
bool httpd_uri_match_wildcard(const char *a,const char *b,size_t size);
esp_err_t httpd_start(httpd_handle_t *handle,httpd_config_t *config);
esp_err_t httpd_register_uri_handler(httpd_handle_t handle,const httpd_uri_t *uri);
bool card_http_start(card_network_t *network);
