/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 */
#include "launcher_wifi.h"
#include <string.h>
#include "esp_log.h"
#include "sugarat_wifi.h"
#include "sugarat_wifi_ui.h"

static sugarat_wifi_t *s_wifi;
static void (*s_on_done)(void);

esp_err_t launcher_wifi_start(void) {
    if (s_wifi) return ESP_OK;
    sugarat_wifi_config_t config = sugarat_wifi_default_config();
    config.ap_prefix = "Sugarat-Launcher";
    return sugarat_wifi_create(&config, &s_wifi);
}

bool launcher_wifi_connected(void) {
    if (!s_wifi) return false;
    sugarat_wifi_snapshot_t state;
    sugarat_wifi_snapshot(s_wifi, &state);
    return state.phase == SW_CONNECTED && state.ip[0];
}

void launcher_wifi_ssid(char *out, size_t cap) {
    out[0] = 0;
    if (!s_wifi || !cap) return;
    sugarat_wifi_snapshot_t state;
    sugarat_wifi_snapshot(s_wifi, &state);
    if (state.phase == SW_CONNECTED && state.ip[0]) strlcpy(out, state.ssid, cap);
}

static void connected(lv_obj_t *screen, const sugarat_wifi_event_t *event, void *user_data) {
    (void)screen; (void)event; (void)user_data;
    if (s_on_done) s_on_done();
}

lv_obj_t *launcher_wifi_setup_create(void (*on_done)(void)) {
    if (!s_wifi) return NULL;
    s_on_done = on_done;
    sugarat_wifi_ui_config_t config = sugarat_wifi_ui_default_config();
    config.on_connected = connected;
    config.notify_current_connection = false;
    lv_obj_t *screen = sugarat_wifi_ui_create_with_config(s_wifi, &config);
    if (!screen) ESP_LOGE("launcher_wifi", "Wi-Fi setup screen could not be created");
    return screen;
}
