#pragma once
#include <stdio.h>
#define ESP_LOGE(tag, fmt, ...) fprintf(stderr, "%s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) fprintf(stderr, "%s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) ((void)0)
static inline const char *esp_err_to_name(int error) { (void)error; return "injected error"; }
