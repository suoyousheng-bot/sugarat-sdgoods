/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "pet_radio.h"
#include "esp_event.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "sdkconfig.h"
#include <string.h>

typedef struct { uint8_t mac[6], bytes[PET_PACKET_MAX]; uint16_t size; } datagram_t;
/* ESP-IDF exposes one receive slot. This bridge borrows the open owner's queue
 * under a lock; close revokes it before the queue can be released. */
static QueueHandle_t receive_queue;
static portMUX_TYPE receive_lock = portMUX_INITIALIZER_UNLOCKED;
static void receive(const esp_now_recv_info_t *info, const uint8_t *data, int size)
{
    if (!info || size < 0 || size > PET_PACKET_MAX) return;
    datagram_t frame; memcpy(frame.mac, info->src_addr, 6); memcpy(frame.bytes, data, size); frame.size = (uint16_t)size;
    portENTER_CRITICAL(&receive_lock);
    if (receive_queue) xQueueSend(receive_queue, &frame, 0);
    portEXIT_CRITICAL(&receive_lock);
}
bool pet_radio_open(pet_radio_t *r)
{
    memset(r, 0, sizeof(*r)); wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) != ESP_ERR_WIFI_NOT_INIT) { r->notice = "Wi-Fi is busy in another service"; return false; }
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) { r->notice = "Radio event loop unavailable"; return false; }
    r->event_owned = err == ESP_OK;
    r->queue = xQueueCreate(12, sizeof(datagram_t));
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    if (!r->queue || esp_wifi_init(&config) != ESP_OK) goto failed;
    r->wifi_owned = true;
    if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK || esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK ||
        esp_wifi_start() != ESP_OK || esp_wifi_set_channel(CONFIG_PET_SOCIAL_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK ||
        esp_wifi_set_ps(WIFI_PS_NONE) != ESP_OK || esp_now_init() != ESP_OK) goto failed;
    r->now_owned = true;
    portENTER_CRITICAL(&receive_lock); receive_queue = r->queue; portEXIT_CRITICAL(&receive_lock);
    if (esp_now_register_recv_cb(receive) != ESP_OK) goto failed;
    return true;
failed:
    pet_radio_close(r); r->notice = "Nearby radio could not start"; return false;
}
void pet_radio_close(pet_radio_t *r)
{
    if (r->now_owned) {
        portENTER_CRITICAL(&receive_lock); receive_queue = NULL; portEXIT_CRITICAL(&receive_lock);
        esp_now_unregister_recv_cb(); esp_now_deinit(); r->now_owned = false;
    }
    if (r->wifi_owned) { esp_wifi_stop(); esp_wifi_deinit(); r->wifi_owned = false; }
    if (r->queue) { vQueueDelete(r->queue); r->queue = NULL; }
    if (r->event_owned) { esp_event_loop_delete_default(); r->event_owned = false; }
}
bool pet_radio_send(pet_radio_t *r, const uint8_t mac[6], const uint8_t *data, size_t size)
{
    if (!r->now_owned || size > PET_PACKET_MAX) return false;
    if (!esp_now_is_peer_exist(mac)) {
        esp_now_peer_info_t peer = {.channel = CONFIG_PET_SOCIAL_CHANNEL, .ifidx = WIFI_IF_STA}; memcpy(peer.peer_addr, mac, 6);
        if (esp_now_add_peer(&peer) != ESP_OK) return false;
    }
    return esp_now_send(mac, data, size) == ESP_OK;
}
void pet_radio_poll(pet_radio_t *r, pet_social_t *social)
{
    if (!r->queue) return;
    datagram_t frame;
    for (unsigned i = 0; i < 12 && xQueueReceive(r->queue, &frame, 0) == pdTRUE; i++)
        pet_social_receive(social, frame.mac, frame.bytes, frame.size);
}
