/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 * Sugarat Radio owns Wi-Fi for the entire firmware lifetime; UI runs on LVGL.
 */
#include "wifi_prov.h"
#include "sugarat_wifi.h"
#include "sugarat_wifi_ui.h"
#include "ui_radio.h"
#include "esp_log.h"

static struct {
    sugarat_wifi_t *network;
    lv_obj_t *setup, *return_screen;
} application;

esp_err_t wifi_prov_start(void) {
    if (application.network) return ESP_OK;
    sugarat_wifi_config_t config = sugarat_wifi_default_config();
    config.ap_prefix = "Sugarat-Radio";
    return sugarat_wifi_create(&config, &application.network);
}

wifi_prov_state_t wifi_prov_get_state(void) {
    if (!application.network) return WIFI_STATE_SETUP;
    sugarat_wifi_snapshot_t state;
    sugarat_wifi_snapshot(application.network, &state);
    if (state.phase == SW_CONNECTED && state.saved && !state.portal &&
        state.last_connected.id == SUGARAT_WIFI_EVENT_CONNECTED) return WIFI_STATE_CONNECTED;
    if (state.phase == SW_CONNECTING) return WIFI_STATE_CONNECTING;
    return state.phase == SW_FAILED ? WIFI_STATE_FAILED : WIFI_STATE_SETUP;
}
bool wifi_prov_is_setup_mode(void) { return wifi_prov_get_state() != WIFI_STATE_CONNECTED; }

static void connected(lv_obj_t *screen, const sugarat_wifi_event_t *event, void *user_data) {
    (void)event; (void)user_data;
    lv_obj_t *destination = application.return_screen;
    application.return_screen = NULL;
    application.setup = NULL;
    if (destination) lv_scr_load(destination);
    else ui_radio_start();
    lv_obj_del(screen);
}

void wifi_prov_show(lv_obj_t *return_screen) {
    if (application.setup) { lv_scr_load(application.setup); return; }
    sugarat_wifi_ui_config_t config = sugarat_wifi_ui_default_config();
    config.on_connected = connected;
    config.notify_current_connection = return_screen == NULL;
    lv_obj_t *screen = sugarat_wifi_ui_create_with_config(application.network, &config);
    if (!screen) {
        ESP_LOGE("Sugarat Radio", "Cannot open Wi-Fi setup: restart the device to retry");
        return;
    }
    application.return_screen = return_screen;
    application.setup = screen;
    lv_scr_load(screen);
}
