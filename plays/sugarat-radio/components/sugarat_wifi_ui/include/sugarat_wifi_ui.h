/* SPDX-License-Identifier: MIT */
#pragma once
#include "lvgl.h"
#include "sugarat_wifi.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Runs on the LVGL thread automatically once per successful connection, including reconnects.
 * A user-requested return to the application invokes it again with the retained successful event.
 * The hotspot is closed and credentials are saved. The callback may load another screen and delete setup_screen.
 * Event data is valid only during this call; copy it if retained. */
typedef void (*sugarat_wifi_ui_connected_cb_t)(lv_obj_t *setup_screen, const sugarat_wifi_event_t *event, void *user_data);
typedef struct {
    sugarat_wifi_ui_connected_cb_t on_connected;
    void *user_data;
    /** Replay startup success once. False keeps settings open; the home back button still returns to the app. */
    bool notify_current_connection;
} sugarat_wifi_ui_config_t;
/** Explicit defaults: no application callback; replay a current successful connection on the first UI tick. */
sugarat_wifi_ui_config_t sugarat_wifi_ui_default_config(void);
/** Create with application callbacks. Config is copied; user_data must outlive the screen.
 * Automatic notifications begin on a later LVGL tick; explicit return runs in the button event.
 * Creation never calls the handler. NULL config/wifi returns NULL. */
lv_obj_t *sugarat_wifi_ui_create_with_config(sugarat_wifi_t *wifi, const sugarat_wifi_ui_config_t *config);
/** Create the English-default round-screen provisioning UI on the LVGL thread.
 * Wi-Fi must outlive the returned screen. Deleting the screen cancels its UI timer and frees UI state.
 * Radio work is queued to the network component. English/Chinese may be switched before password entry.
 * Touch targets are at least 48x48px within the 240x240 safe area; passwords use a grouped keyboard. */
lv_obj_t *sugarat_wifi_ui_create(sugarat_wifi_t *wifi);
#ifdef __cplusplus
}
#endif
