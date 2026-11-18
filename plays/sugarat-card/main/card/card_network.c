/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "card_network.h"
#include "esp_event.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define T(zh,en) ((card_message_t){zh,en})

typedef struct {uint8_t mac[6],bytes[CARD_PACKET_MAX];uint16_t size;} datagram_t;
/* ESP-IDF has one receive callback. The foreground application revokes this
 * borrowed queue under the same lock used by the callback before releasing it. */
static QueueHandle_t radio_queue;
static portMUX_TYPE radio_lock=portMUX_INITIALIZER_UNLOCKED;
static void received(const esp_now_recv_info_t *info,const uint8_t *data,int size)
{
    if(!info||size<=0||size>CARD_PACKET_MAX)return;
    datagram_t frame;memcpy(frame.mac,info->src_addr,6);memcpy(frame.bytes,data,size);frame.size=(uint16_t)size;
    portENTER_CRITICAL(&radio_lock);if(radio_queue)xQueueSend(radio_queue,&frame,0);portEXIT_CRITICAL(&radio_lock);
}
static bool wifi_begin(card_network_t *n)
{
    if(n->server||n->dns||n->netif||n->event_owned||n->wifi_owned||n->mutex||n->datagrams){n->notice=T("网络未能关闭，请重启设备","Network could not close. Restart badge.");return false;}
    memset(n,0,sizeof(*n));wifi_mode_t mode;
    if(esp_wifi_get_mode(&mode)!=ESP_ERR_WIFI_NOT_INIT){n->notice=T("无线网络正忙，请稍后重试","Wireless is busy. Try again later.");return false;}
    esp_err_t err=esp_event_loop_create_default();
    if(err!=ESP_OK&&err!=ESP_ERR_INVALID_STATE){n->notice=T("网络事件初始化失败","Network setup failed. Restart badge.");return false;}
    n->event_owned=err==ESP_OK;
    wifi_init_config_t config=WIFI_INIT_CONFIG_DEFAULT();
    if(esp_wifi_init(&config)!=ESP_OK){n->notice=T("无线网络初始化失败","Wireless setup failed. Restart badge.");return false;}
    n->wifi_owned=true;return esp_wifi_set_storage(WIFI_STORAGE_RAM)==ESP_OK;
}
bool card_network_close(card_network_t *n)
{
    if(n->server){if(httpd_stop(n->server)!=ESP_OK)goto failed;n->server=NULL;}
    if(n->dns){sugarat_wifi_dns_stop(n->dns);n->dns=NULL;}
    if(n->now_owned) {
        portENTER_CRITICAL(&radio_lock);radio_queue=NULL;portEXIT_CRITICAL(&radio_lock);
        esp_now_unregister_recv_cb();if(esp_now_deinit()!=ESP_OK)goto failed;n->now_owned=false;
    }
    if(n->wifi_owned){
        esp_err_t stopped=esp_wifi_stop();if(stopped!=ESP_OK&&stopped!=ESP_ERR_WIFI_NOT_STARTED)goto failed;
        if(esp_wifi_deinit()!=ESP_OK)goto failed;
        n->wifi_owned=false;
    }
    if(n->netif){esp_netif_destroy_default_wifi(n->netif);n->netif=NULL;}
    if(n->event_owned){if(esp_event_loop_delete_default()!=ESP_OK)goto failed;n->event_owned=false;}
    if(n->datagrams)vQueueDelete(n->datagrams);
    if(n->mutex)vSemaphoreDelete(n->mutex);
    memset(n,0,sizeof(*n));return true;
failed:
    n->notice=T("网络未能关闭，请重启设备","Network could not close. Restart badge.");return false;
}
bool card_network_radio(card_network_t *n)
{
    if(!wifi_begin(n))goto failed;
    n->datagrams=xQueueCreate(16,sizeof(datagram_t));if(!n->datagrams)goto failed;
    if(esp_wifi_set_mode(WIFI_MODE_STA)!=ESP_OK||esp_wifi_start()!=ESP_OK||
       esp_wifi_set_channel(CONFIG_CARD_WIFI_CHANNEL,WIFI_SECOND_CHAN_NONE)!=ESP_OK||esp_now_init()!=ESP_OK)goto failed;
    n->now_owned=true;
    portENTER_CRITICAL(&radio_lock);radio_queue=n->datagrams;portEXIT_CRITICAL(&radio_lock);
    if(esp_now_register_recv_cb(received)!=ESP_OK)goto failed;
    n->mode=CARD_NET_RADIO;return true;
failed:;
    card_message_t notice=n->notice;if(!card_network_close(n))return false;n->notice=notice.zh?notice:T("无线交换无法启动，请返回重试","Exchange could not start. Go back and retry.");return false;
}
bool card_network_send(void *context,const uint8_t mac[6],const uint8_t *bytes,size_t size)
{
    card_network_t *n=context;if(!n->now_owned||size>CARD_PACKET_MAX)return false;
    if(!esp_now_is_peer_exist(mac)) {
        esp_now_peer_info_t peer={.channel=CONFIG_CARD_WIFI_CHANNEL,.ifidx=WIFI_IF_STA};memcpy(peer.peer_addr,mac,6);
        if(esp_now_add_peer(&peer)!=ESP_OK)return false;
    }
    return esp_now_send(mac,bytes,size)==ESP_OK;
}
void card_network_poll(card_network_t *n,card_exchange_t *e,uint64_t now)
{
    if(n->mode!=CARD_NET_RADIO)return;
    datagram_t frame;
    for(unsigned i=0;i<16&&xQueueReceive(n->datagrams,&frame,0)==pdTRUE;++i)card_exchange_receive(e,frame.mac,frame.bytes,frame.size,now);
}
bool card_network_portal(card_network_t *n,const card_profile_t *profile,uint64_t now,bool password_required,card_language_t language)
{
    if(!wifi_begin(n))goto failed;
    uint8_t secret[16],mac[6];
    if(!card_random_bytes(secret,sizeof(secret)))goto failed;
    card_id_hex(secret,n->token);
    if(password_required) {
        uint32_t password;if(!card_random_bytes((uint8_t *)&password,sizeof(password)))goto failed;
        snprintf(n->password,sizeof(n->password),"%08lu",(unsigned long)(password%100000000U));
    }
    if(esp_wifi_get_mac(WIFI_IF_AP,mac)!=ESP_OK)goto failed;
    snprintf(n->ssid,sizeof(n->ssid),"Sugarat-%02X%02X",mac[4],mac[5]);
    snprintf(n->url,sizeof(n->url),"http://192.168.4.1/edit?lang=%s#%s",language==CARD_EN?"en":"zh",n->token);
    n->mutex=xSemaphoreCreateMutex();if(!n->mutex)goto failed;n->published=*profile;
    esp_err_t init=esp_netif_init();if(init!=ESP_OK&&init!=ESP_ERR_INVALID_STATE)goto failed;
    n->netif=esp_netif_create_default_wifi_ap();if(!n->netif)goto failed;
    wifi_config_t config={0};strcpy((char *)config.ap.ssid,n->ssid);strcpy((char *)config.ap.password,n->password);
    config.ap.ssid_len=strlen(n->ssid);config.ap.channel=CONFIG_CARD_WIFI_CHANNEL;config.ap.max_connection=1;config.ap.authmode=password_required?WIFI_AUTH_WPA2_PSK:WIFI_AUTH_OPEN;
    if(esp_wifi_set_mode(WIFI_MODE_AP)!=ESP_OK||esp_wifi_set_config(WIFI_IF_AP,&config)!=ESP_OK||esp_wifi_start()!=ESP_OK)goto failed;
    if(!card_http_start(n)||sugarat_wifi_dns_start(n->netif,&n->dns)!=ESP_OK)goto failed;
    static char portal[]="http://192.168.4.1/captive-portal";
    if(esp_netif_dhcps_stop(n->netif)!=ESP_OK||
       esp_netif_dhcps_option(n->netif,ESP_NETIF_OP_SET,ESP_NETIF_CAPTIVEPORTAL_URI,portal,sizeof(portal)-1)!=ESP_OK||
       esp_netif_dhcps_start(n->netif)!=ESP_OK)goto failed;
    n->mode=CARD_NET_PORTAL;n->deadline=now+(uint64_t)CONFIG_CARD_EDIT_SECONDS*1000;return true;
failed:;
    card_message_t notice=n->notice;if(!card_network_close(n))return false;n->notice=notice.zh?notice:T("热点编辑无法启动，请返回重试","Hotspot could not start. Go back and retry.");return false;
}
bool card_network_pending(card_network_t *n,card_profile_t *out)
{
    if(n->mode!=CARD_NET_PORTAL)return false;
    xSemaphoreTake(n->mutex,portMAX_DELAY);bool queued=n->queued;
    if(queued){*out=n->pending;n->queued=false;}xSemaphoreGive(n->mutex);return queued;
}
void card_network_complete(card_network_t *n,bool success,const card_profile_t *profile)
{
    xSemaphoreTake(n->mutex,portMAX_DELAY);if(success)n->published=*profile;n->failed=!success;n->finished=true;xSemaphoreGive(n->mutex);
}
bool card_network_connected(card_network_t *n,bool *connected)
{
    wifi_sta_list_t stations;
    if(n->mode!=CARD_NET_PORTAL||esp_wifi_ap_get_sta_list(&stations)!=ESP_OK)return false;
    *connected=stations.num>0;return true;
}
