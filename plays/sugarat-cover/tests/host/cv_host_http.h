/* SPDX-License-Identifier: Apache-2.0 */
/** Host-only ESP HTTP transport adapter; product handlers and store remain unchanged. */
#pragma once
#include "cv_backend.h"
#include "cv_store.h"
#include "cJSON.h"
#include <stdint.h>
#include <stddef.h>
typedef int esp_err_t;
#define ESP_OK 0
#define portMAX_DELAY 0
#define HTTPD_404_NOT_FOUND 404
typedef enum { HTTP_GET,HTTP_POST,HTTP_PUT,HTTP_DELETE } httpd_method_t;
typedef void *httpd_handle_t;
typedef struct {cv_store_t store;cv_status_t status;void *mutex,*library_mutex;httpd_handle_t http;char directory[192];bool faulted;} cv_device_t;
void cv_device_library_changed(cv_device_t *device);
typedef struct {
    void *user_ctx;const char *uri,*token;httpd_method_t method;size_t content_len,offset;
    const uint8_t *body;uint8_t *response;size_t response_size;int status;char type[64],location[128];
} httpd_req_t;
typedef struct {const char *uri;httpd_method_t method;esp_err_t (*handler)(httpd_req_t *);void *user_ctx;} httpd_uri_t;
typedef struct {bool (*uri_match_fn)(const char *,const char *,size_t);unsigned stack_size,max_open_sockets,recv_wait_timeout,send_wait_timeout;bool lru_purge_enable;} httpd_config_t;
#define HTTPD_DEFAULT_CONFIG() ((httpd_config_t){0})
static inline void xSemaphoreTake(void *mutex,int timeout){(void)mutex;(void)timeout;}
static inline void xSemaphoreGive(void *mutex){(void)mutex;}
static inline void sdgoods_touch_flash_window(unsigned ms){(void)ms;}
static inline void sdgoods_touch_flash_window_end(void){}
int64_t esp_timer_get_time(void);
size_t httpd_req_get_hdr_value_len(httpd_req_t *req,const char *key);
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *req,const char *key,char *out,size_t size);
esp_err_t httpd_resp_set_type(httpd_req_t *req,const char *type);
esp_err_t httpd_resp_set_hdr(httpd_req_t *req,const char *key,const char *value);
esp_err_t httpd_resp_set_status(httpd_req_t *req,const char *status);
esp_err_t httpd_resp_send(httpd_req_t *req,const char *body,ptrdiff_t size);
esp_err_t httpd_resp_sendstr(httpd_req_t *req,const char *body);
esp_err_t httpd_resp_send_err(httpd_req_t *req,int status,const char *body);
int httpd_req_recv(httpd_req_t *req,char *out,size_t count);
bool httpd_uri_match_wildcard(const char *a,const char *b,size_t n);
esp_err_t httpd_start(httpd_handle_t *handle,httpd_config_t *config);
esp_err_t httpd_register_uri_handler(httpd_handle_t handle,const httpd_uri_t *uri);
esp_err_t httpd_stop(httpd_handle_t handle);
bool cv_http_stop(cv_device_t *device);
typedef struct {uint8_t *bytes;size_t size;} host_asset_t;
extern host_asset_t cv_host_assets[4];
#define html_start (cv_host_assets[0].bytes)
#define html_end (cv_host_assets[0].bytes+cv_host_assets[0].size)
#define js_start (cv_host_assets[1].bytes)
#define js_end (cv_host_assets[1].bytes+cv_host_assets[1].size)
#define css_start (cv_host_assets[2].bytes)
#define css_end (cv_host_assets[2].bytes+cv_host_assets[2].size)
#define font_start (cv_host_assets[3].bytes)
#define font_end (cv_host_assets[3].bytes+cv_host_assets[3].size)
