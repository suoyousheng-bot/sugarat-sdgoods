/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Firmware-lifetime services; wireless callbacks never access LVGL or released screens. */
#pragma once
#include "cv_backend.h"
#include "cv_store.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "nvs.h"
#include "sugarat_wifi_dns.h"
typedef struct {
    cv_store_t store; SemaphoreHandle_t mutex,library_mutex; QueueHandle_t commands,packets;
    TaskHandle_t worker,preferences_worker;QueueHandle_t preferences;cv_status_t status; cv_link_t link; nvs_handle_t nvs;
    httpd_handle_t http; esp_netif_t *ap;
    sugarat_wifi_dns_t *dns;
    uint8_t *outgoing; bool wifi,now,event,netif,settings_ready,seeded,faulted;
    uint32_t generation;
    bool password_required;
} cv_device_t;
/** Initialize private settings/storage and one persistent service worker. Failed storage stays visibly read-only. */
bool cv_device_start(cv_device_t *device,cv_backend_t *backend);
/** Request cancellation without freeing resources still used by the worker. */
void cv_device_suspend(cv_device_t *device);
/** Publish an immutable catalogue copy while the caller holds library_mutex. UI reads never wait for flash I/O. */
void cv_device_library_changed(cv_device_t *device);
/** @returns Whether a session token exactly matches the current management session. */
bool cv_http_authorized(cv_device_t *device,const char *token);
/** Start the management site and captive redirects on the already-running SoftAP. */
bool cv_http_start(cv_device_t *device);
/** Stop accepting requests and wait for the HTTP task before revoking the management session. */
bool cv_http_stop(cv_device_t *device);
