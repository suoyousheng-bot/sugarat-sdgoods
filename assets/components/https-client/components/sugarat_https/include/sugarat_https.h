/* Bounded HTTPS requests over esp_http_client with the ESP-IDF certificate bundle. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_http_client.h"
#include "sugarat_https_backoff.h"

#ifdef __cplusplus
extern "C" {
#endif

/** One request header. Values are sent as given and never logged. */
typedef struct { const char *name,*value; } sugarat_https_header_t;

typedef struct {
    const char *url;                         /**< Must start with "https://". */
    esp_http_client_method_t method;
    const sugarat_https_header_t *headers;   /**< header_count entries, valid during the call. */
    size_t header_count;
    const char *content_type;                /**< Required with a body. */
    const void *body;size_t body_length;
    size_t max_response_bytes;               /**< Larger bodies fail with ESP_ERR_INVALID_SIZE. */
    int timeout_ms;
    bool prefer_psram;                       /**< Allocate the response body in PSRAM when present. */
} sugarat_https_request_t;

/** GET url, 64 KiB response limit, 12000 ms timeout, response body in PSRAM. */
sugarat_https_request_t sugarat_https_request_default(const char *url);

typedef struct {
    int status;              /**< HTTP status; 0 when none arrived. */
    uint8_t *body;           /**< NUL-terminated, length bytes before the NUL; NULL when empty. */
    size_t length;
    int retry_after_s;       /**< Retry-After seconds; 60 for a non-numeric value; 0 when absent. */
} sugarat_https_response_t;

/** Performs one request; requests from all tasks run one at a time. Redirects are returned as their
 *  3xx status, not followed. ESP_OK means an HTTP status arrived, whatever its value. Errors:
 *  ESP_ERR_INVALID_ARG for a missing or non-HTTPS URL, a zero limit or a body without content type;
 *  ESP_ERR_INVALID_SIZE when the body exceeds max_response_bytes (status is still set);
 *  ESP_ERR_NO_MEM; or the esp_http_client/TLS error when no status arrived. On error body is NULL.
 *  Logs carry the method, host, status and length only. Must not run on the LVGL task. */
esp_err_t sugarat_https_perform(const sugarat_https_request_t *request,sugarat_https_response_t *response);

/** Overwrites the body with zeros, frees it and resets the response; safe to call twice. */
void sugarat_https_response_free(sugarat_https_response_t *response);

#ifdef __cplusplus
}
#endif
