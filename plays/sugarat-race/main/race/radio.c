/* SPDX-License-Identifier: Apache-2.0 */
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
typedef struct{uint8_t mac[6],bytes[64];}datagram;
static QueueHandle_t callback_queue;
static portMUX_TYPE bridge_lock=portMUX_INITIALIZER_UNLOCKED;
static void received(const esp_now_recv_info_t *info,const uint8_t *data,int size){
    if(!info||size!=64)return;
    datagram d;memcpy(d.mac,info->src_addr,6);memcpy(d.bytes,data,64);
    portENTER_CRITICAL(&bridge_lock);if(callback_queue)xQueueSend(callback_queue,&d,0);portEXIT_CRITICAL(&bridge_lock);
}
void race_radio_close(race_radio *r){
    portENTER_CRITICAL(&bridge_lock);callback_queue=NULL;portEXIT_CRITICAL(&bridge_lock);
    if(r->now_owned){esp_now_unregister_recv_cb();esp_now_deinit();r->now_owned=false;}
    if(r->wifi_owned){esp_wifi_stop();esp_wifi_deinit();r->wifi_owned=false;}
    if(r->queue){vQueueDelete(r->queue);r->queue=NULL;}
    if(r->event_owned){esp_event_loop_delete_default();r->event_owned=false;}
}
esp_err_t race_radio_open(race_radio *r){
    memset(r,0,sizeof *r);wifi_mode_t mode;if(esp_wifi_get_mode(&mode)!=ESP_ERR_WIFI_NOT_INIT)return ESP_ERR_INVALID_STATE;
    esp_err_t err=esp_netif_init();if(err!=ESP_OK)return err;
    err=esp_event_loop_create_default();if(err!=ESP_OK&&err!=ESP_ERR_INVALID_STATE)return err;r->event_owned=err==ESP_OK;
    r->queue=xQueueCreate(12,sizeof(datagram));if(!r->queue){err=ESP_ERR_NO_MEM;goto failed;}
    wifi_init_config_t c=WIFI_INIT_CONFIG_DEFAULT();c.nvs_enable=0;c.static_rx_buf_num=4;c.dynamic_rx_buf_num=8;c.tx_buf_type=1;c.dynamic_tx_buf_num=8;c.cache_tx_buf_num=4;
    err=esp_wifi_init(&c);if(err!=ESP_OK)goto failed;r->wifi_owned=true;
    if((err=esp_wifi_set_storage(WIFI_STORAGE_RAM))!=ESP_OK||(err=esp_wifi_set_mode(WIFI_MODE_STA))!=ESP_OK||
       (err=esp_wifi_start())!=ESP_OK||(err=esp_wifi_set_channel(CONFIG_RACE_RADIO_CHANNEL,WIFI_SECOND_CHAN_NONE))!=ESP_OK||
       (err=esp_wifi_set_ps(WIFI_PS_NONE))!=ESP_OK||(err=esp_now_init())!=ESP_OK)goto failed;
    r->now_owned=true;portENTER_CRITICAL(&bridge_lock);callback_queue=r->queue;portEXIT_CRITICAL(&bridge_lock);
    err=esp_now_register_recv_cb(received);if(err!=ESP_OK)goto failed;return ESP_OK;
failed:race_radio_close(r);ESP_LOGE("race_radio","Nearby unavailable: %s; single-player remains available",esp_err_to_name(err));return err;
}
void race_radio_send(void *owner,const uint8_t mac[6],const uint8_t *data,size_t size){
    race_radio *r=owner;if(!r->now_owned)return;
    if(!esp_now_is_peer_exist(mac)){esp_now_peer_info_t peer={.channel=CONFIG_RACE_RADIO_CHANNEL,.ifidx=WIFI_IF_STA};memcpy(peer.peer_addr,mac,6);if(esp_now_add_peer(&peer)!=ESP_OK)return;}
    esp_now_send(mac,data,size);
}
void race_radio_poll(race_radio *r,race_link *l,uint32_t now){
    if(!r->queue)return;
    datagram d;for(int i=0;i<12&&xQueueReceive(r->queue,&d,0)==pdTRUE;i++)race_link_receive(l,d.mac,d.bytes,64,now);
}
