# HTTPS 客户端

[English](README.md) | 中文

在 ESP-IDF 工程根目录以 `target=esp-idf` 安装 `https-client`，并把 `sugarat_https` 加入 `main/REQUIRES`。组件通过 `esp_http_client` 一次执行一个有上限的 HTTPS 请求，用 ESP-IDF 证书包（`CONFIG_MBEDTLS_CERTIFICATE_BUNDLE`，默认开启）校验服务器，并把响应体放在 PSRAM 中。只在网络连接后发起请求，例如收到 `SUGARAT_WIFI_EVENT_CONNECTED` 之后，并在工作任务中调用：一次请求最长阻塞 `timeout_ms`，不能在 LVGL 任务中运行。

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

## 请求与响应

`sugarat_https_request_default()` 选择 GET、64 KiB 响应上限、12000 ms 超时和 PSRAM 响应体。其他请求设置 `method`、`headers`、`body`、`body_length` 和 `content_type`；带请求体时必须提供内容类型。只接受 `https://` 地址。重定向以 3xx 状态码返回，不会自动跟随，因此凭据不会被发往其他主机。只要收到 HTTP 状态码（包括 4xx 和 5xx），`sugarat_https_perform()` 就返回 `ESP_OK`；请检查 `response.status`。响应体超过 `max_response_bytes` 时返回 `ESP_ERR_INVALID_SIZE`，`status` 仍会设置。没有状态码时返回 `esp_http_client` 或 TLS 的错误。响应体以 NUL 结尾，为空时是 `NULL`。所有任务的请求依次执行，因此同一时间只有一个 TLS 会话占用内部 RAM。

## 退避

`sugarat_https_backoff.h` 为单个服务控制重试节奏。传输失败、5xx 或不带 `Retry-After` 的 429 之后，等待时间从 `base_ms` 开始逐次翻倍，最多 `max_ms`。带 `Retry-After` 的 429 等待其给出的秒数；非数字的 `Retry-After` 按 60 秒计。其他状态码清零失败次数。该头文件不依赖 ESP-IDF；`test/host_test.c` 用 `cc -std=c11 -fsanitize=address,undefined -Iinclude test/host_test.c` 检查它。

## 密钥

日志只包含方法、主机、状态码和响应体长度，不含路径、查询参数、请求头或响应体。`sugarat_https_response_free()` 在释放前清零响应体，响应体扩容时也会清零旧缓冲区。`esp_http_client` 会保留一份请求头副本，释放时不清零；请求结束后请清除调用方自己的请求头缓冲区，如示例所示。不要把令牌或密码编译进固件。

## 已知限制

响应体在内存中缓冲，最多 `max_response_bytes`；没有用于大文件下载的流式回调。不提供 OAuth 令牌刷新和请求取消。不缓存响应。源码提取自 Spotify 播放器，该播放器的请求路径已在谷仓徽章上用 ESP-IDF 5.5.4 验证；组件本身经过编译和退避主机测试验证。每个新服务的请求都要在设备上验证。
