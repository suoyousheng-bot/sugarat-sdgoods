/* SPDX-License-Identifier: Apache-2.0 */
/** One firmware radio callback slot borrows its owner's queue while holding the bridge lock. */
#include "radio.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "sdkconfig.h"
#include <string.h>
typedef struct { uint8_t mac[6], bytes[BOTTLE_PACKET_SIZE]; } datagram_t;
static QueueHandle_t receive_queue;
static portMUX_TYPE receive_lock = portMUX_INITIALIZER_UNLOCKED;
static void receive(const esp_now_recv_info_t *info,const uint8_t *bytes,int size)
{
    if(!info || size!=BOTTLE_PACKET_SIZE) return;
    datagram_t frame; memcpy(frame.mac,info->src_addr,6); memcpy(frame.bytes,bytes,sizeof(frame.bytes));
    portENTER_CRITICAL(&receive_lock);
    if(receive_queue) xQueueSend(receive_queue,&frame,0);
    portEXIT_CRITICAL(&receive_lock);
}
void bottle_radio_close(bottle_radio_t *r)
{
    portENTER_CRITICAL(&receive_lock); receive_queue=NULL; portEXIT_CRITICAL(&receive_lock);
    if(r->now_owned) { esp_now_unregister_recv_cb(); esp_now_deinit(); r->now_owned=false; }
    if(r->wifi_owned) { esp_wifi_stop(); esp_wifi_deinit(); r->wifi_owned=false; }
    if(r->queue) { vQueueDelete(r->queue); r->queue=NULL; }
    if(r->event_owned) { esp_event_loop_delete_default(); r->event_owned=false; }
}
esp_err_t bottle_radio_open(bottle_radio_t *r)
{
    memset(r,0,sizeof(*r)); wifi_mode_t mode;
    if(esp_wifi_get_mode(&mode)!=ESP_ERR_WIFI_NOT_INIT) return r->error=ESP_ERR_INVALID_STATE;
    esp_err_t err=esp_netif_init();
    if(err!=ESP_OK) return r->error=err;
    err=esp_event_loop_create_default();
    if(err!=ESP_OK && err!=ESP_ERR_INVALID_STATE) return r->error=err;
    r->event_owned=err==ESP_OK;
    r->queue=xQueueCreate(12,sizeof(datagram_t));
    if(!r->queue) { err=ESP_ERR_NO_MEM; goto failed; }
    wifi_init_config_t config=WIFI_INIT_CONFIG_DEFAULT();
    config.static_rx_buf_num=4; config.dynamic_rx_buf_num=8;
    config.tx_buf_type=1; config.dynamic_tx_buf_num=8; config.cache_tx_buf_num=4;
    err=esp_wifi_init(&config); if(err!=ESP_OK) goto failed;
    r->wifi_owned=true;
    if((err=esp_wifi_set_storage(WIFI_STORAGE_RAM))!=ESP_OK ||
       (err=esp_wifi_set_mode(WIFI_MODE_STA))!=ESP_OK ||
       (err=esp_wifi_start())!=ESP_OK ||
       (err=esp_wifi_set_channel(CONFIG_BOTTLE_RADIO_CHANNEL,WIFI_SECOND_CHAN_NONE))!=ESP_OK ||
       (err=esp_wifi_set_ps(WIFI_PS_NONE))!=ESP_OK ||
       (err=esp_now_init())!=ESP_OK) goto failed;
    r->now_owned=true;
    portENTER_CRITICAL(&receive_lock); receive_queue=r->queue; portEXIT_CRITICAL(&receive_lock);
    err=esp_now_register_recv_cb(receive); if(err!=ESP_OK) goto failed;
    return ESP_OK;
failed:
    bottle_radio_close(r); r->error=err;
    ESP_LOGE("bottle_radio","Nearby unavailable: %s; return to Solo and retry",esp_err_to_name(err));
    return err;
}
void bottle_radio_send(void *context,const uint8_t mac[6],const uint8_t *bytes,size_t size)
{
    bottle_radio_t *r=context;
    if(!r->now_owned) return;
    if(!esp_now_is_peer_exist(mac)) {
        esp_now_peer_info_t peer={.channel=CONFIG_BOTTLE_RADIO_CHANNEL,.ifidx=WIFI_IF_STA};
        memcpy(peer.peer_addr,mac,6);
        if(esp_now_add_peer(&peer)!=ESP_OK) return;
    }
    /* Send errors are packet loss. Session acknowledgements stop quantities from advancing. */
    esp_now_send(mac,bytes,size);
}
void bottle_radio_poll(bottle_radio_t *r,bottle_link_t *link,uint32_t now)
{
    if(!r->queue) return;
    datagram_t frame;
    for(unsigned i=0;i<12 && xQueueReceive(r->queue,&frame,0)==pdTRUE;i++)
        bottle_link_receive(link,frame.mac,frame.bytes,sizeof(frame.bytes),now);
}
