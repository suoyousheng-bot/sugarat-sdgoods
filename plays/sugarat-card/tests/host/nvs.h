/* SPDX-License-Identifier: Apache-2.0 */
/** In-memory NVS adapter for the real language preference implementation. */
#pragma once
#include <stddef.h>
typedef int esp_err_t;
typedef unsigned nvs_handle_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NVS_NOT_FOUND 1
#define NVS_READONLY 0
#define NVS_READWRITE 1
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *handle);
esp_err_t nvs_get_str(nvs_handle_t handle,const char *key,char *value,size_t *size);
esp_err_t nvs_set_str(nvs_handle_t handle,const char *key,const char *value);
esp_err_t nvs_commit(nvs_handle_t handle);
void nvs_close(nvs_handle_t handle);
