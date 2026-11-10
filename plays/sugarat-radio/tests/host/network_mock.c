/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 * Deterministic radio fixture: no physical Wi-Fi or credentials are accessed.
 */
#include "sugarat_wifi.h"
#include <assert.h>
#include <string.h>
struct sugarat_wifi { sugarat_wifi_snapshot_t state; };
static struct sugarat_wifi fixture;
sugarat_wifi_config_t sugarat_wifi_default_config(void) {
    return (sugarat_wifi_config_t){ .ap_prefix="Sugarat", .portal_seconds=900, .connect_seconds=20, .reconnect_attempts=3 };
}
esp_err_t sugarat_wifi_create(const sugarat_wifi_config_t *config, sugarat_wifi_t **out) {
    assert(!strcmp(config->ap_prefix,"Sugarat-Radio"));
    memset(&fixture,0,sizeof(fixture));
    fixture.state.phase=SW_SCANNING; fixture.state.revision=1;
    *out=&fixture; return ESP_OK;
}
void sugarat_wifi_snapshot(sugarat_wifi_t *wifi, sugarat_wifi_snapshot_t *out) { *out=wifi->state; }
esp_err_t sugarat_wifi_scan(sugarat_wifi_t *wifi) { (void)wifi; return ESP_OK; }
esp_err_t sugarat_wifi_connect(sugarat_wifi_t *wifi,uint32_t generation,unsigned id,const char *password) {
    (void)wifi;(void)generation;(void)id;(void)password;return ESP_OK;
}
esp_err_t sugarat_wifi_portal(sugarat_wifi_t *wifi, bool open) { wifi->state.portal=open; return ESP_OK; }
esp_err_t sugarat_wifi_forget(sugarat_wifi_t *wifi) { (void)wifi; return ESP_OK; }
void mock_wifi_outcome(bool saved,bool portal) {
    sugarat_wifi_snapshot_t *state=&fixture.state;
    state->revision++;state->phase=SW_CONNECTED;state->saved=saved;state->portal=portal;
    state->last_connected=(sugarat_wifi_event_t){ .id=SUGARAT_WIFI_EVENT_CONNECTED,
        .saved=saved,.portal=portal,.sequence=state->revision };
}
void mock_wifi_fail(void) { fixture.state.phase=SW_FAILED;fixture.state.revision++; }
