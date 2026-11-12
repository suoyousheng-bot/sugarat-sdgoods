/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 *
 * Launcher Wi-Fi, owned for the whole firmware lifetime through the kit's sugarat_wifi component.
 * Credentials live in NVS namespace "sugarat_wifi", which every app built with the same component
 * reads, so a network joined here is reused by those apps (for example Sugarat Radio).
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "lvgl.h"

/** Create the Wi-Fi owner and reconnect to saved credentials. NVS must be initialized. */
esp_err_t launcher_wifi_start(void);

/** @returns True when the station has an IP address. */
bool launcher_wifi_connected(void);

/** Copy the connected network name, or an empty string. */
void launcher_wifi_ssid(char *out, size_t cap);

/** Create the provisioning screen (LVGL thread). `on_done` runs on the LVGL thread after a
 * connection is saved or when the user returns; it must load another screen.
 * @returns The setup screen, or NULL when it could not be created. */
lv_obj_t *launcher_wifi_setup_create(void (*on_done)(void));
