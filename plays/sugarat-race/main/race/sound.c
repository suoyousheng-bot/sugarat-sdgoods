/* SPDX-License-Identifier: Apache-2.0 */
#include "sound.h"
#include "board_pins.h"
#include "sdgoods_audio.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>
#define RATE 16000u
#define BLOCK 160u /* 10 ms */
typedef struct { race_sfx sfx; float strength; } request;
static const char *TAG="race_sound";
static i2s_chan_handle_t channel;
static TaskHandle_t task;
static QueueHandle_t requests;
static SemaphoreHandle_t stopped;
static portMUX_TYPE params_lock=portMUX_INITIALIZER_UNLOCKED;
static race_synth_params shared;
static volatile bool running;
static race_synth synth;
static int16_t mono[BLOCK],stereo[BLOCK*2];
static void amplifier(bool on){gpio_set_level(BOARD_AUDIO_PA_EN_GPIO,on?BOARD_AUDIO_PA_EN_ACTIVE_LEVEL:!BOARD_AUDIO_PA_EN_ACTIVE_LEVEL);}
static void write_block(void){size_t written;i2s_channel_write(channel,stereo,sizeof stereo,&written,pdMS_TO_TICKS(100));}
static void worker(void *argument){
    (void)argument;
    memset(stereo,0,sizeof stereo);for(int i=0;i<4;i++)write_block();
    float gain=0;bool amp=false;
    while(running){
        request r;while(xQueueReceive(requests,&r,0)==pdTRUE)race_synth_trigger(&synth,r.sfx,r.strength);
        race_synth_params p;portENTER_CRITICAL(&params_lock);p=shared;portEXIT_CRITICAL(&params_lock);
        race_synth_render(&synth,&p,mono,BLOCK);
        int volume=sdgoods_audio_get_volume();
        /* The control centre may switch the amplifier off; keep it on while volume is above zero. */
        bool want=volume>0;if(want||amp!=want){amplifier(want);amp=want;}
        float target=volume/100.0f,step=(target-gain)/BLOCK;
        for(unsigned i=0;i<BLOCK;i++){gain+=step;int16_t v=(int16_t)(mono[i]*gain);stereo[2*i]=v;stereo[2*i+1]=v;}
        write_block();
    }
    for(unsigned i=0;i<BLOCK;i++){float fade=gain*(1-(float)i/BLOCK);int16_t v=(int16_t)(mono[i]*fade);stereo[2*i]=v;stereo[2*i+1]=v;}
    write_block();memset(stereo,0,sizeof stereo);for(int i=0;i<3;i++)write_block();
    amplifier(false);
    xSemaphoreGive(stopped);vTaskDelete(NULL);
}
static void release(void){
    if(channel){i2s_channel_disable(channel);i2s_del_channel(channel);channel=NULL;}
    if(requests){vQueueDelete(requests);requests=NULL;}
    if(stopped){vSemaphoreDelete(stopped);stopped=NULL;}
}
esp_err_t race_sound_start(void){
    if(task)return ESP_ERR_INVALID_STATE;
    gpio_config_t pa={.pin_bit_mask=1ULL<<BOARD_AUDIO_PA_EN_GPIO,.mode=GPIO_MODE_INPUT_OUTPUT};
    esp_err_t err=gpio_config(&pa);if(err!=ESP_OK)return err;
    amplifier(false);
    requests=xQueueCreate(16,sizeof(request));stopped=xSemaphoreCreateBinary();
    if(!requests||!stopped){release();return ESP_ERR_NO_MEM;}
    i2s_chan_config_t chan=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,I2S_ROLE_MASTER);
    chan.auto_clear=true;chan.dma_desc_num=4;chan.dma_frame_num=BLOCK;
    if((err=i2s_new_channel(&chan,&channel,NULL))!=ESP_OK){channel=NULL;release();return err;}
    i2s_std_config_t std={
        .clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(RATE),
        .slot_cfg=I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_STEREO),
        .gpio_cfg={.mclk=BOARD_SPK_I2S_GPIO_MCLK,.bclk=BOARD_SPK_I2S_GPIO_BCLK,.ws=BOARD_SPK_I2S_GPIO_WS,.dout=BOARD_SPK_I2S_GPIO_DOUT,.din=GPIO_NUM_NC},
    };
    std.clk_cfg.mclk_multiple=I2S_MCLK_MULTIPLE_384;
    if((err=i2s_channel_init_std_mode(channel,&std))!=ESP_OK||(err=i2s_channel_enable(channel))!=ESP_OK){
        i2s_del_channel(channel);channel=NULL;release();return err;
    }
    race_synth_init(&synth,RATE);memset(&shared,0,sizeof shared);running=true;
    if(xTaskCreatePinnedToCore(worker,"race_sound",4096,NULL,6,&task,0)!=pdPASS){task=NULL;running=false;release();return ESP_ERR_NO_MEM;}
    ESP_LOGI(TAG,"Speaker ready; volume %d%%",sdgoods_audio_get_volume());
    return ESP_OK;
}
void race_sound_stop(void){
    if(!task)return;
    running=false;
    if(xSemaphoreTake(stopped,pdMS_TO_TICKS(500))!=pdTRUE){ESP_LOGE(TAG,"Sound task did not stop; speaker stays claimed");return;}
    task=NULL;release();
}
void race_sound_set(const race_synth_params *p){portENTER_CRITICAL(&params_lock);shared=*p;portEXIT_CRITICAL(&params_lock);}
void race_sound_play(race_sfx sfx,float strength){
    if(!requests)return;
    request r={sfx,strength};xQueueSend(requests,&r,0);
}
