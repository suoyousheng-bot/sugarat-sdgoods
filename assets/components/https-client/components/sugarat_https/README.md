# HTTPS client

English | [中文](README.zh.md)

Install `https-client` with `target=esp-idf` under the ESP-IDF project root and add `sugarat_https` to `main/REQUIRES`. The component performs one bounded HTTPS request at a time over `esp_http_client`, verifies servers with the ESP-IDF certificate bundle (`CONFIG_MBEDTLS_CERTIFICATE_BUNDLE`, enabled by default) and returns the body in PSRAM. Start requests only after the network is connected, for example after `SUGARAT_WIFI_EVENT_CONNECTED`, and from a worker task: a request blocks for up to `timeout_ms` and must not run on the LVGL task.

```c
static sugarat_https_backoff_t backoff; /* sugarat_https_backoff_init(&backoff, 2000, 60000) once */

static void poll(void)
{
    int64_t now = esp_timer_get_time() / 1000;
    if (!sugarat_https_backoff_ready(&backoff, now)) return;
    char auth[512];
    snprintf(auth, sizeof(auth), "Bearer %s", token);
    const sugarat_https_header_t headers[] = {{"Authorization", auth}};
    sugarat_https_request_t request = sugarat_https_request_default("https://api.example.com/v1/state");
    request.headers = headers; request.header_count = 1;
    sugarat_https_response_t response;
    esp_err_t err = sugarat_https_perform(&request, &response);
    memset(auth, 0, sizeof(auth));
    sugarat_https_backoff_record(&backoff, now, response.status != 0, response.status, response.retry_after_s);
    if (err == ESP_OK && response.status == 200) parse((const char *)response.body, response.length);
    sugarat_https_response_free(&response);
}
```

## Requests and responses

`sugarat_https_request_default()` selects GET, a 64 KiB response limit, a 12000 ms timeout and a PSRAM body. Set `method`, `headers`, `body`, `body_length` and `content_type` for other requests; a body requires a content type. Only `https://` URLs are accepted. Redirects are returned as their 3xx status and are not followed, so credentials never travel to another host. `sugarat_https_perform()` returns `ESP_OK` whenever an HTTP status arrived, including 4xx and 5xx; check `response.status`. A body longer than `max_response_bytes` fails with `ESP_ERR_INVALID_SIZE` while `status` is still set. Without a status the result is the `esp_http_client` or TLS error. The body is NUL-terminated and `NULL` when empty. Requests from all tasks run one after another, so one TLS session at a time uses internal RAM.

## Backoff

`sugarat_https_backoff.h` paces retries for one service. After a transport failure, a 5xx or a 429 without `Retry-After`, the wait starts at `base_ms` and doubles up to `max_ms`. A 429 with `Retry-After` waits that many seconds; a non-numeric `Retry-After` counts as 60 s. Any other status clears the failure count. The header has no ESP-IDF dependency; `test/host_test.c` checks it with `cc -std=c11 -fsanitize=address,undefined -Iinclude test/host_test.c`.

## Secrets

Logs carry only the method, host, status and body length, never the path, query, headers or body. `sugarat_https_response_free()` zeroes the body before freeing it, and growing the body zeroes the old buffer. `esp_http_client` keeps its own copy of request headers and frees it without zeroing; clear the caller's header buffers after the request, as in the example. Do not compile tokens or passwords into firmware.

## Known limitations

The body is buffered in memory up to `max_response_bytes`; there is no streaming callback for large downloads. OAuth token refresh and request cancellation are not provided. Responses are not cached. The source is extracted from the Spotify player, whose request path was verified on an SDGOODS badge with ESP-IDF 5.5.4; the component itself is verified by compilation and the backoff host test. Verify each new service's requests on the device.
