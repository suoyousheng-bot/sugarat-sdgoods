/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 */
#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"

typedef enum {
    WIFI_STATE_SETUP, WIFI_STATE_CONNECTING, WIFI_STATE_CONNECTED, WIFI_STATE_FAILED
} wifi_prov_state_t;

/** Create the single Sugarat Wi-Fi owner. NVS and LVGL must be initialized. */
esp_err_t wifi_prov_start(void);
/** Read synchronized network state; connected requires saved credentials and closed setup services. */
wifi_prov_state_t wifi_prov_get_state(void);
/** Return whether the radio needs Wi-Fi setup or reconnect. */
bool wifi_prov_is_setup_mode(void);
/** Show the component's setup screen on the LVGL thread; return_screen remains caller-owned.
 * After confirmed connection, return to that screen or start Radio when NULL.
 */
void wifi_prov_show(lv_obj_t *return_screen);
