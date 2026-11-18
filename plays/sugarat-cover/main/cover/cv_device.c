/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** One worker owns Wi-Fi, ESP-NOW and protocol transitions. A mutex serializes durable library operations. */
#include "cv_device.h"
#include "cv_jpeg.h"
#include "sdgoods_app_sdk.h"
#include "sdgoods_nvs.h"
#include "sdgoods_input.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
extern const unsigned char sample_start[] asm("_binary_sample_jpg_start");
extern const unsigned char sample_end[] asm("_binary_sample_jpg_end");
typedef struct { unsigned kind;uint8_t peer[6];uint32_t generation;cv_mode_t mode;char id[CV_ID_LENGTH+1];bool password_required; } command_t;
typedef struct { uint8_t mac[6],bytes[CV_PACKET];uint16_t size; } packet_t;
/* ESP-IDF has one callback slot. Revocation and callback queue borrowing share this lock. */
static struct { QueueHandle_t queue;portMUX_TYPE lock; } bridge={.lock=portMUX_INITIALIZER_UNLOCKED};
static uint64_t milliseconds(void){return (uint64_t)esp_timer_get_time()/1000;}
void cv_device_library_changed(cv_device_t *d)
{xSemaphoreTake(d->mutex,portMAX_DELAY);d->status.library=d->store.library;xSemaphoreGive(d->mutex);}
static void receive(const esp_now_recv_info_t *info,const uint8_t *bytes,int length)
{
    if(!info||length<20||length>CV_PACKET)return;
    packet_t p={.size=(uint16_t)length};memcpy(p.mac,info->src_addr,6);memcpy(p.bytes,bytes,(size_t)length);
    portENTER_CRITICAL(&bridge.lock);if(bridge.queue)xQueueSend(bridge.queue,&p,0);portEXIT_CRITICAL(&bridge.lock);
}
static bool write_settings(void *user,const char *current,cv_locale_t locale)
{
    cv_device_t *d=user;if(!d->settings_ready)return false;
    uint8_t record[36]={1,(uint8_t)locale,(uint8_t)d->seeded};memcpy(record+3,current,strlen(current)+1);
    return nvs_set_blob(d->nvs,"prefs",record,sizeof(record))==ESP_OK&&nvs_commit(d->nvs)==ESP_OK;
}
static bool transmit(void *user,const uint8_t mac[6],const uint8_t *bytes,size_t size)
{
    cv_device_t *d=user;if(!d->now)return false;
    if(!esp_now_is_peer_exist(mac)) {
        esp_now_peer_info_t peer={.ifidx=WIFI_IF_STA,.channel=CONFIG_COVER_CHANNEL};memcpy(peer.peer_addr,mac,6);
        if(esp_now_add_peer(&peer)!=ESP_OK)return false;
        esp_now_rate_config_t rate={.phymode=WIFI_PHY_MODE_11G,.rate=WIFI_PHY_RATE_6M};
        if(esp_now_set_peer_rate_config(mac,&rate)!=ESP_OK)return false;
    }
    return esp_now_send(mac,bytes,size)==ESP_OK;
}
static cv_error_t save_incoming(void *user,const uint8_t *bytes,size_t size,char id[CV_ID_LENGTH+1])
{
    cv_device_t *d=user;xSemaphoreTake(d->library_mutex,portMAX_DELAY);sdgoods_touch_flash_window(3000);
    cv_error_t error=cv_store_add(&d->store,bytes,size,id);sdgoods_touch_flash_window_end();
    cv_device_library_changed(d);xSemaphoreGive(d->library_mutex);return error;
}
static bool network_fault(cv_device_t *d)
{
    xSemaphoreTake(d->mutex,portMAX_DELAY);d->faulted=true;d->status.error=CV_RADIO;d->status.ready=false;d->status.changing=false;xSemaphoreGive(d->mutex);
    ESP_LOGE("sugarat_cover","Wireless shutdown unconfirmed; resources retained until restart");return false;
}
static bool network_close(cv_device_t *d)
{
    if(!cv_http_stop(d))return network_fault(d);
    if(d->dns){sugarat_wifi_dns_stop(d->dns);d->dns=NULL;}
    if(d->now) {
        cv_link_cancel(&d->link);
        portENTER_CRITICAL(&bridge.lock);bridge.queue=NULL;portEXIT_CRITICAL(&bridge.lock);
        esp_now_unregister_recv_cb();esp_err_t stopped=esp_now_deinit();
        if(stopped!=ESP_OK&&stopped!=ESP_ERR_ESPNOW_NOT_INIT)return network_fault(d);
        d->now=false;
    }
    cv_link_close(&d->link);free(d->outgoing);d->outgoing=NULL;
    if(d->wifi){
        esp_err_t stopped=esp_wifi_stop();if(stopped!=ESP_OK&&stopped!=ESP_ERR_WIFI_NOT_STARTED)return network_fault(d);
        stopped=esp_wifi_deinit();if(stopped!=ESP_OK&&stopped!=ESP_ERR_WIFI_NOT_INIT)return network_fault(d);d->wifi=false;
    }
    if(d->ap){esp_netif_destroy_default_wifi(d->ap);d->ap=NULL;}
    xQueueReset(d->packets);
    xSemaphoreTake(d->mutex,portMAX_DELAY);
    d->status.mode=CV_IDLE;d->status.phase=L_OFF;d->status.ready=false;d->status.stations=0;
    memset(d->status.password,0,sizeof(d->status.password));memset(d->status.token,0,sizeof(d->status.token));
    xSemaphoreGive(d->mutex);
    return true;
}
static bool network_open(cv_device_t *d,cv_mode_t mode,bool password_required)
{
    wifi_init_config_t config=WIFI_INIT_CONFIG_DEFAULT();config.nvs_enable=false;
    config.static_rx_buf_num=4;config.dynamic_rx_buf_num=6;config.dynamic_tx_buf_num=6;
    if(esp_wifi_init(&config)!=ESP_OK)return false;d->wifi=true;
    if(esp_wifi_set_storage(WIFI_STORAGE_RAM)!=ESP_OK||esp_wifi_set_mode(mode==CV_PHONE_MODE?WIFI_MODE_AP:WIFI_MODE_STA)!=ESP_OK)return false;
    uint8_t own[6];if(esp_wifi_get_mac(mode==CV_PHONE_MODE?WIFI_IF_AP:WIFI_IF_STA,own)!=ESP_OK)return false;
    if(mode==CV_PHONE_MODE) {
        d->ap=esp_netif_create_default_wifi_ap();if(!d->ap)return false;
        uint8_t random[16];esp_fill_random(random,sizeof(random));
        wifi_config_t ap={.ap={.channel=CONFIG_COVER_CHANNEL,.max_connection=1,.authmode=password_required?WIFI_AUTH_WPA2_PSK:WIFI_AUTH_OPEN}};
        snprintf((char *)ap.ap.ssid,sizeof(ap.ap.ssid),"Sugarat-%02X%02X",own[4],own[5]);
        if(password_required)snprintf((char *)ap.ap.password,sizeof(ap.ap.password),"%08lu",(unsigned long)(esp_random()%100000000U));
        if(esp_wifi_set_config(WIFI_IF_AP,&ap)!=ESP_OK)return false;
        xSemaphoreTake(d->mutex,portMAX_DELAY);
        strcpy(d->status.ssid,(char *)ap.ap.ssid);strcpy(d->status.password,(char *)ap.ap.password);
        for(unsigned i=0;i<16;i++)snprintf(d->status.token+i*2,3,"%02x",random[i]);
        xSemaphoreGive(d->mutex);
    }
    if(esp_wifi_start()!=ESP_OK||esp_wifi_set_ps(WIFI_PS_NONE)!=ESP_OK)return false;
    if(mode!=CV_PHONE_MODE) {
        if(esp_wifi_set_channel(CONFIG_COVER_CHANNEL,WIFI_SECOND_CHAN_NONE)!=ESP_OK||esp_now_init()!=ESP_OK)return false;
        d->now=true;
        portENTER_CRITICAL(&bridge.lock);bridge.queue=d->packets;portEXIT_CRITICAL(&bridge.lock);
        if(esp_now_register_recv_cb(receive)!=ESP_OK||!cv_link_open(&d->link,mode==CV_RECEIVE_MODE,own,transmit,save_incoming,d,milliseconds()))return false;
    } else {
        if(!cv_http_start(d)||sugarat_wifi_dns_start(d->ap,&d->dns)!=ESP_OK)return false;
        static char portal[]=CV_PORTAL_ORIGIN "/captive-portal";
        if(esp_netif_dhcps_stop(d->ap)!=ESP_OK||
           esp_netif_dhcps_option(d->ap,ESP_NETIF_OP_SET,ESP_NETIF_CAPTIVEPORTAL_URI,portal,sizeof(portal)-1)!=ESP_OK||
           esp_netif_dhcps_start(d->ap)!=ESP_OK)return false;
    }
    xSemaphoreTake(d->mutex,portMAX_DELAY);memcpy(d->status.own,own,6);d->status.mode=mode;d->status.ready=true;xSemaphoreGive(d->mutex);
    return true;
}
static void worker(void *user)
{
    cv_device_t *d=user;
    while(true) {
        command_t command;
        if(xQueueReceive(d->commands,&command,pdMS_TO_TICKS(10))==pdTRUE) {
            if(d->faulted)continue;
            if(command.kind==0) {
                bool closed=network_close(d);bool ok=closed&&(command.mode==CV_IDLE||network_open(d,command.mode,command.password_required));
                xSemaphoreTake(d->mutex,portMAX_DELAY);bool stale=command.generation!=d->generation;xSemaphoreGive(d->mutex);
                if((!ok||stale)&&closed)network_close(d);
                xSemaphoreTake(d->mutex,portMAX_DELAY);if(!stale){d->status.error=ok&&!d->faulted?CV_OK:CV_RADIO;d->status.changing=false;}xSemaphoreGive(d->mutex);
                ESP_LOGI("sugarat_cover","MODE %u ready=%u",(unsigned)command.mode,(unsigned)ok);
            } else if(command.kind==1&&d->now) {
                if(d->link.phase!=L_DISCOVER)continue;
                free(d->outgoing);d->outgoing=NULL;size_t size=0;
                xSemaphoreTake(d->library_mutex,portMAX_DELAY);cv_error_t error=cv_store_read(&d->store,command.id,&d->outgoing,&size);xSemaphoreGive(d->library_mutex);
                unsigned index=0;for(;index<d->link.peer_count;index++)if(!memcmp(d->link.peers[index].mac,command.peer,6))break;
                if(error==CV_OK&&!cv_link_offer(&d->link,index,d->outgoing,size))error=CV_BUSY;
                if(error!=CV_OK){xSemaphoreTake(d->mutex,portMAX_DELAY);d->status.error=error;xSemaphoreGive(d->mutex);}
            } else if(command.kind==2&&d->now)cv_link_confirm(&d->link);
        }
        if(d->faulted)continue;
        if(d->now) {
            cv_link_tick(&d->link,milliseconds());packet_t packet;
            for(unsigned i=0;i<4&&xQueueReceive(d->packets,&packet,0)==pdTRUE;i++)cv_link_receive(&d->link,packet.mac,packet.bytes,packet.size);
            xSemaphoreTake(d->mutex,portMAX_DELAY);d->status.phase=d->link.phase;d->status.peer_count=d->link.peer_count;
            for(unsigned i=0;i<d->link.peer_count;i++)memcpy(d->status.peers[i],d->link.peers[i].mac,6);
            memcpy(d->status.peer,d->link.peer,6);d->status.code=d->link.code;strcpy(d->status.saved,d->link.saved);
            d->status.progress=d->link.total?(unsigned)((uint64_t)d->link.offset*100/d->link.total):0;
            if(d->link.error!=CV_OK)d->status.error=d->link.error;
            xSemaphoreGive(d->mutex);
        } else if(d->http) {
            wifi_sta_list_t stations;if(esp_wifi_ap_get_sta_list(&stations)==ESP_OK){xSemaphoreTake(d->mutex,portMAX_DELAY);d->status.stations=stations.num;xSemaphoreGive(d->mutex);}
        }
    }
}
static bool snapshot(void *user,cv_status_t *status)
{
    cv_device_t *d=user;if(xSemaphoreTake(d->mutex,0)!=pdTRUE)return false;*status=d->status;xSemaphoreGive(d->mutex);return true;
}
static cv_error_t pixels(void *user,const char *id,uint16_t *out)
{
    cv_device_t *d=user;xSemaphoreTake(d->library_mutex,portMAX_DELAY);uint8_t *bytes=NULL;size_t size=0;
    cv_error_t error=cv_store_load(&d->store,id,&bytes,&size);
    if(error==CV_OK)error=cv_jpeg(bytes,size,out);free(bytes);xSemaphoreGive(d->library_mutex);return error;
}
static cv_error_t select_photo(void *user,const char *id)
{
    cv_device_t *d=user;xSemaphoreTake(d->library_mutex,portMAX_DELAY);sdgoods_touch_flash_window(3000);
    cv_error_t error=cv_store_select(&d->store,id);sdgoods_touch_flash_window_end();cv_device_library_changed(d);xSemaphoreGive(d->library_mutex);return error;
}
static void preferences_worker(void *user)
{
    cv_device_t *d=user;cv_locale_t value;
    while(true) {
        if(xQueueReceive(d->preferences,&value,portMAX_DELAY)!=pdTRUE)continue;
        xSemaphoreTake(d->library_mutex,portMAX_DELAY);sdgoods_touch_flash_window(500);
        cv_error_t error=cv_store_locale(&d->store,value);sdgoods_touch_flash_window_end();
        xSemaphoreTake(d->mutex,portMAX_DELAY);d->status.library=d->store.library;d->status.locale_pending=false;d->status.locale_error=error;d->status.locale_result++;xSemaphoreGive(d->mutex);
        xSemaphoreGive(d->library_mutex);
    }
}
static bool request_locale(void *user,cv_locale_t value)
{
    cv_device_t *d=user;xSemaphoreTake(d->mutex,portMAX_DELAY);
    bool ok=!d->status.locale_pending&&xQueueSend(d->preferences,&value,0)==pdTRUE;
    if(ok)d->status.locale_pending=true;
    xSemaphoreGive(d->mutex);return ok;
}
static bool request_mode(cv_device_t *d,cv_mode_t value,bool password_required)
{
    command_t c={.kind=0,.mode=value,.password_required=password_required};
    xSemaphoreTake(d->mutex,portMAX_DELAY);
    if(value==CV_IDLE)xQueueReset(d->commands);
    bool allowed=!d->faulted&&(value==CV_IDLE||!d->status.changing);
    if(allowed)c.generation=++d->generation;
    bool ok=allowed&&xQueueSend(d->commands,&c,0)==pdTRUE;
    if(ok){d->status.changing=true;d->status.error=CV_OK;d->password_required=password_required;}
    xSemaphoreGive(d->mutex);return ok;
}
static bool mode(void *user,cv_mode_t value)
{cv_device_t *d=user;return request_mode(d,value,d->password_required);}
static bool phone_security(void *user,bool password_required)
{return request_mode(user,CV_PHONE_MODE,password_required);}
static bool offer(void *user,const uint8_t peer[6],const char *id)
{
    cv_device_t *d=user;if(!cv_id_valid(id))return false;
    command_t c={.kind=1};memcpy(c.peer,peer,6);strcpy(c.id,id);return xQueueSend(d->commands,&c,0)==pdTRUE;
}
static bool confirm(void *user){cv_device_t *d=user;command_t c={.kind=2};return xQueueSend(d->commands,&c,0)==pdTRUE;}
bool cv_device_start(cv_device_t *d,cv_backend_t *backend)
{
    memset(d,0,sizeof(*d));d->mutex=xSemaphoreCreateMutex();d->library_mutex=xSemaphoreCreateMutex();d->commands=xQueueCreate(4,sizeof(command_t));d->packets=xQueueCreate(4,sizeof(packet_t));
    d->preferences=xQueueCreate(1,sizeof(cv_locale_t));
    if(!d->mutex||!d->library_mutex||!d->commands||!d->packets||!d->preferences)return false;
    esp_err_t error=esp_event_loop_create_default();if(error!=ESP_OK&&error!=ESP_ERR_INVALID_STATE)return false;
    if(esp_netif_init()!=ESP_OK)return false;
    sdgoods_nvs_ensure();d->settings_ready=nvs_open("sugarat_cover",NVS_READWRITE,&d->nvs)==ESP_OK;
    char current[CV_ID_LENGTH+1]={0};uint8_t language=0;cv_error_t settings_error=CV_OK;
    if(d->settings_ready){
        uint8_t record[36];size_t length=sizeof(record);esp_err_t loaded=nvs_get_blob(d->nvs,"prefs",record,&length);
        if(loaded==ESP_OK&&length==sizeof(record)&&record[0]==1&&record[1]<=1&&record[2]<=1&&record[35]==0&&
           (record[3]==0||cv_id_valid((const char *)record+3))){language=record[1];d->seeded=record[2];strcpy(current,(const char *)record+3);}
        else if(loaded!=ESP_ERR_NVS_NOT_FOUND){d->settings_ready=false;settings_error=CV_CORRUPT;}
    }
    char directory[192];sdgoods_touch_flash_window(3000);
    bool mounted=sdgoods_appdata_begin("sugarat_cover",directory,sizeof(directory))==ESP_OK;
    cv_error_t stored=cv_store_open(&d->store,mounted?directory:NULL,current,language==1?CV_ZH:CV_EN,write_settings,d);
    if(stored==CV_OK&&!d->seeded&&!d->store.library.count&&d->settings_ready) {
        char id[CV_ID_LENGTH+1];stored=cv_store_add(&d->store,sample_start,(size_t)(sample_end-sample_start),id);
        if(stored==CV_OK)stored=cv_store_select(&d->store,id);
    }
    if(stored==CV_OK&&!d->seeded&&d->store.library.count&&d->settings_ready){d->seeded=true;if(!write_settings(d,d->store.current,d->store.library.locale))stored=CV_SETTINGS;}
    sdgoods_touch_flash_window_end();
    if(!d->settings_ready&&stored==CV_OK)stored=CV_SETTINGS;
    if(settings_error!=CV_OK)stored=settings_error;
    if(stored!=CV_OK){d->store.library.writable=false;d->store.library.error=stored;}
    cv_device_library_changed(d);
    *backend=(cv_backend_t){.user=d,.status=snapshot,.pixels=pixels,.select=select_photo,.request_locale=request_locale,.mode=mode,.phone_security=phone_security,.offer=offer,.confirm=confirm};
    if(xTaskCreate(preferences_worker,"cover_prefs",4096,d,3,&d->preferences_worker)!=pdPASS)return false;
    if(xTaskCreate(worker,"cover_service",12288,d,4,&d->worker)!=pdPASS)return false;
    return true;
}
void cv_device_suspend(cv_device_t *d)
{
    if(!d->worker)return;
    if(!mode(d,CV_IDLE))ESP_LOGE("sugarat_cover","Radio cancellation queue full; worker still owns resources");
}
