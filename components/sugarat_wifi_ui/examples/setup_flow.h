/* SPDX-License-Identifier: MIT */
#pragma once
#include "sugarat_wifi_ui.h"

/** Create a setup/home/settings flow with the component-owned return arrow on the LVGL thread.
 * Load the returned screen; NULL means allocation failed. Wi-Fi must outlive all screens in this flow. */
lv_obj_t *sugarat_wifi_example_create(sugarat_wifi_t *wifi);
