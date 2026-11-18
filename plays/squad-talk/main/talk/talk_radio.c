/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Wi-Fi callbacks copy bounded packets; one worker owns protocol, audio decoding and playback. */
#include "talk_radio.h"
#include "sugarat_audio.h"
#include "sdgoods_nvs.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>

typedef struct { uint8_t source[6]; uint16_t size; uint8_t data[TALK_PACKET_MAX]; } received_t;
typedef struct { uint32_t generation; int16_t pcm[TALK_SAMPLES]; } microphone_t;
static struct {
    QueueHandle_t network, microphone;
    SemaphoreHandle_t sent;
    TaskHandle_t worker;
    talk_status_t status;
    bool held, paused, stop, capture;
    uint8_t channel;
    uint32_t generation, dropped;
    esp_now_send_status_t send_result;
} radio;
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static const char *TAG="squad_talk";
static bool flush_audio(uint32_t timeout)
{
    if(sugarat_audio_stream_flush_wait(timeout))return true;
    portENTER_CRITICAL(&lock);radio.status.fault=TALK_FAULT_AUDIO;radio.paused=true;portEXIT_CRITICAL(&lock);
    ESP_LOGE(TAG,"Audio flush acknowledgement timed out; transmission paused");
    return false;
}

static void receive(const esp_now_recv_info_t *info,const uint8_t *data,int length)
{
    if(length<TALK_HEADER||length>TALK_PACKET_MAX||!radio.network)return;
    received_t p={.size=(uint16_t)length};memcpy(p.source,info->src_addr,6);memcpy(p.data,data,(size_t)length);
    if(xQueueSend(radio.network,&p,0)!=pdTRUE){portENTER_CRITICAL(&lock);radio.dropped++;portEXIT_CRITICAL(&lock);}
}
static void sent(const wifi_tx_info_t *info,esp_now_send_status_t status)
{
    (void)info;radio.send_result=status;xSemaphoreGive(radio.sent);
}
static bool send_packet(const uint8_t *destination,const uint8_t *data,size_t size,void *user)
{
    (void)user;
    portENTER_CRITICAL(&lock);bool broken=radio.stop&&radio.status.fault==TALK_FAULT_RADIO;portEXIT_CRITICAL(&lock);
    if(broken)return false;
    if(!esp_now_is_peer_exist(destination)){
        esp_now_peer_info_t peer={.channel=6,.ifidx=WIFI_IF_STA,.encrypt=false};memcpy(peer.peer_addr,destination,6);
        if(esp_now_add_peer(&peer)!=ESP_OK)return false;
    }
    /* Exactly one outstanding packet keeps callback completions associated with their send. */
    if(esp_now_send(destination,data,size)!=ESP_OK)return false;
    if(xSemaphoreTake(radio.sent,pdMS_TO_TICKS(80))!=pdTRUE){
        portENTER_CRITICAL(&lock);radio.stop=true;radio.status.fault=TALK_FAULT_RADIO;portEXIT_CRITICAL(&lock);
        return false;
    }
    return radio.send_result==ESP_NOW_SEND_SUCCESS;
}
static void play(const int16_t *samples,size_t count,void *user)
{
    (void)user;int16_t limited[TALK_SAMPLES];
    for(size_t i=0;i<count;i++)limited[i]=samples[i]>9000?9000:samples[i]<-9000?-9000:samples[i];
    if(sugarat_audio_stream_write(limited,count)!=count){
        flush_audio(200);portENTER_CRITICAL(&lock);radio.dropped++;portEXIT_CRITICAL(&lock);
    }
}
static void microphone(const int16_t *samples,size_t count,void *user)
{
    (void)user;microphone_t m;
    portENTER_CRITICAL(&lock);bool capture=radio.capture;m.generation=radio.generation;portEXIT_CRITICAL(&lock);
    if(!capture||count!=TALK_SAMPLES)return;
    int peak=0;
    for(size_t i=0;i<count;i++){
        int n=samples[i];if(n>9000)n=9000;if(n<-9000)n=-9000;m.pcm[i]=(int16_t)n;
        int magnitude=n<0?-n:n;if(magnitude>peak)peak=magnitude;
    }
    portENTER_CRITICAL(&lock);radio.status.level=(uint8_t)(peak*100/9000);portEXIT_CRITICAL(&lock);
    if(xQueueSend(radio.microphone,&m,0)!=pdTRUE){portENTER_CRITICAL(&lock);radio.dropped++;portEXIT_CRITICAL(&lock);}
}
static void audio_event(const sugarat_audio_event_t *event,void *user)
{
    (void)user;
    if(event->type==SUGARAT_AUDIO_EVENT_ERROR){
        ESP_LOGE(TAG,"Audio failed: %s",esp_err_to_name(event->error));
        portENTER_CRITICAL(&lock);radio.status.fault=TALK_FAULT_AUDIO;radio.paused=true;portEXIT_CRITICAL(&lock);
    }
}
static esp_err_t initialize(bool *wifi,bool *now,bool *audio)
{
    sdgoods_nvs_ensure();
    sugarat_audio_config_t config=sugarat_audio_default_config();config.on_event=audio_event;
    esp_err_t err=sugarat_audio_init(&config);if(err!=ESP_OK)return err;
    *audio=true;
    err=esp_event_loop_create_default();if(err!=ESP_OK&&err!=ESP_ERR_INVALID_STATE)return err;
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();init.nvs_enable=false;
    init.static_rx_buf_num=4;init.dynamic_rx_buf_num=8;init.dynamic_tx_buf_num=8;
    err=esp_wifi_init(&init);if(err!=ESP_OK)return err;*wifi=true;
    if((err=esp_wifi_set_storage(WIFI_STORAGE_RAM))!=ESP_OK)return err;
    if((err=esp_wifi_set_mode(WIFI_MODE_STA))!=ESP_OK)return err;
    if((err=esp_wifi_start())!=ESP_OK)return err;
    if((err=esp_wifi_set_ps(WIFI_PS_NONE))!=ESP_OK)return err;
    if((err=esp_wifi_set_channel(6,WIFI_SECOND_CHAN_NONE))!=ESP_OK)return err;
    if((err=esp_now_init())!=ESP_OK)return err;
    *now=true;
    if((err=esp_now_register_recv_cb(receive))!=ESP_OK)return err;
    if((err=esp_now_register_send_cb(sent))!=ESP_OK)return err;
    sugarat_audio_stream_config_t stream={.on_mic=microphone,.playback_buffer_ms=400};
    err=sugarat_audio_stream_start(&stream);
    if(err!=ESP_OK)return err;
    return flush_audio(1000)?ESP_OK:ESP_ERR_TIMEOUT;
}
static void run(void *arg)
{
    (void)arg;bool wifi=false,now=false,audio=false;
    esp_err_t err=initialize(&wifi,&now,&audio);
    if(err!=ESP_OK){
        ESP_LOGE(TAG,"Start failed: %s",esp_err_to_name(err));
        portENTER_CRITICAL(&lock);radio.status.fault=audio?TALK_FAULT_RADIO:TALK_FAULT_AUDIO;portEXIT_CRITICAL(&lock);
    }else{
        uint8_t mac[6];err=esp_wifi_get_mac(WIFI_IF_STA,mac);
        if(err!=ESP_OK){portENTER_CRITICAL(&lock);radio.status.fault=TALK_FAULT_RADIO;portEXIT_CRITICAL(&lock);}
        else{
            talk_core_t core;talk_core_init(&core,mac,esp_random(),send_packet,play,NULL);
            talk_phase_t previous=TALK_WAITING;
            uint64_t logged=0,last_loop=0,max_gap=0;uint32_t sent_audio=0;
            ESP_LOGI(TAG,"SQUAD_TALK_READY version=0.1.3 RF=6 groups=1..8 MAC=" MACSTR,MAC2STR(mac));
            while(true){
                portENTER_CRITICAL(&lock);bool held=radio.held,paused=radio.paused,stop=radio.stop;
                uint8_t channel=radio.channel;talk_fault_t fault=radio.status.fault;portEXIT_CRITICAL(&lock);
                if(stop)break;
                core.now=(uint64_t)(esp_timer_get_time()/1000);
                if(last_loop&&core.now-last_loop>max_gap)max_gap=core.now-last_loop;
                last_loop=core.now;
                talk_core_channel(&core,channel);talk_core_pause(&core,paused||fault!=TALK_FAULT_NONE);
                talk_core_hold(&core,held&&!paused&&fault==TALK_FAULT_NONE);
                received_t packet;
                for(unsigned i=0;i<12&&xQueueReceive(radio.network,&packet,0)==pdTRUE;i++){
                    /* The first valid group announcement adds the peer on demand when granting a request. */
                    talk_phase_t before=core.phase;
                    talk_core_receive(&core,packet.source,packet.data,packet.size);
                    if(before!=TALK_RECEIVING&&core.phase==TALK_RECEIVING)flush_audio(200);
                }
                talk_core_tick(&core,core.now);
                bool capture=core.phase==TALK_TRANSMITTING;
                if(core.phase!=previous){
                    ESP_LOGI(TAG,"STATE %u -> %u CH=%u",(unsigned)previous,(unsigned)core.phase,core.channel);
                    if(capture||core.phase==TALK_PAUSED||channel!=radio.status.channel)flush_audio(200);
                    portENTER_CRITICAL(&lock);radio.generation++;radio.capture=capture;portEXIT_CRITICAL(&lock);
                    xQueueReset(radio.microphone);previous=core.phase;
                }
                microphone_t mic;
                if(xQueueReceive(radio.microphone,&mic,0)==pdTRUE){
                    portENTER_CRITICAL(&lock);bool current=radio.capture&&mic.generation==radio.generation&&radio.held&&!radio.paused;portEXIT_CRITICAL(&lock);
                    if(current){talk_core_audio(&core,mic.pcm);sent_audio++;}
                }
                sugarat_audio_status_t sound;sugarat_audio_get_status(&sound);
                portENTER_CRITICAL(&lock);
                radio.status.phase=core.phase;radio.status.channel=core.channel;radio.status.online=core.paired&&core.peer_available;
                radio.status.link_failed=core.transport_failed;radio.status.received=core.received;radio.status.lost=core.lost;radio.status.queue_drops=radio.dropped;
                if(!capture)radio.status.level=sound.out_level_percent;
                portEXIT_CRITICAL(&lock);
                if(core.now-logged>=1000){
                    if(core.phase==TALK_TRANSMITTING||core.phase==TALK_RECEIVING||sent_audio){
                        ESP_LOGI(TAG,"FLOW phase=%u tx=%lu rx=%lu lost=%lu drops=%lu queued=%u loop_max=%llu ms",
                            (unsigned)core.phase,(unsigned long)sent_audio,(unsigned long)core.received,
                            (unsigned long)core.lost,(unsigned long)radio.status.queue_drops,
                            (unsigned)sugarat_audio_stream_queued(),(unsigned long long)max_gap);
                    }
                    logged=core.now;sent_audio=0;max_gap=0;
                }
                vTaskDelay(pdMS_TO_TICKS(5)+1);
            }
            talk_core_pause(&core,true);
        }
    }
    portENTER_CRITICAL(&lock);radio.capture=false;portEXIT_CRITICAL(&lock);
    if(now){esp_now_unregister_recv_cb();esp_now_unregister_send_cb();esp_now_deinit();}
    if(wifi){esp_wifi_stop();esp_wifi_deinit();}
    if(audio)sugarat_audio_stop();
    portENTER_CRITICAL(&lock);radio.worker=NULL;radio.status.phase=TALK_PAUSED;radio.status.online=false;portEXIT_CRITICAL(&lock);
    vTaskDelete(NULL);
}
esp_err_t talk_radio_start(void)
{
    if(radio.network)return ESP_ERR_INVALID_STATE;
    radio.channel=1;radio.status.channel=1;
    radio.network=xQueueCreate(20,sizeof(received_t));radio.microphone=xQueueCreate(6,sizeof(microphone_t));radio.sent=xSemaphoreCreateBinary();
    if(!radio.network||!radio.microphone||!radio.sent){radio.status.fault=TALK_FAULT_MEMORY;return ESP_ERR_NO_MEM;}
    if(xTaskCreatePinnedToCore(run,"squad_radio",7168,NULL,7,&radio.worker,1)!=pdPASS){radio.status.fault=TALK_FAULT_MEMORY;return ESP_ERR_NO_MEM;}
    return ESP_OK;
}
void talk_radio_status(talk_status_t *out){portENTER_CRITICAL(&lock);*out=radio.status;portEXIT_CRITICAL(&lock);}
void talk_radio_input(bool held,uint8_t channel,bool paused)
{portENTER_CRITICAL(&lock);radio.held=held;radio.channel=channel;radio.paused=paused;portEXIT_CRITICAL(&lock);}
void talk_radio_stop(void)
{portENTER_CRITICAL(&lock);radio.held=false;radio.paused=true;radio.stop=true;portEXIT_CRITICAL(&lock);}
