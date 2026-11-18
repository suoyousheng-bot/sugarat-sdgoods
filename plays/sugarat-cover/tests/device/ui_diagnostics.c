/* SPDX-License-Identifier: Apache-2.0 */
/** Diagnostic-only pointer workloads use the board input driver and wait for a painted UI result. */
#include "sdgoods_console.h"
#include "sdgoods_tap.h"
#include "sdgoods_lvgl.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"
#include <string.h>
static struct {lv_timer_t *timer;const char *expected;int64_t start;char command;bool active;} run;
static bool contains(lv_obj_t *object,const char *text)
{
    if(lv_obj_check_type(object,&lv_label_class)&&!strcmp(lv_label_get_text(object),text))return true;
    for(unsigned i=0;i<lv_obj_get_child_cnt(object);i++)if(contains(lv_obj_get_child(object,(int)i),text))return true;
    return false;
}
static void observe(lv_timer_t *timer)
{
    (void)timer;if(!run.active)return;
    bool ready=run.expected?contains(lv_scr_act(),run.expected):(contains(lv_scr_act(),"Phone upload")||contains(lv_scr_act(),"手机上传"));
    int64_t elapsed=esp_timer_get_time()-run.start;
    if(!ready&&elapsed<6000000)return;
    if(ready)lv_refr_now(NULL);
    elapsed=esp_timer_get_time()-run.start;run.active=false;
    ESP_LOGI("cover_measure","command=%c ready=%u elapsed_us=%lld free=%u minimum_since_boot=%u",run.command,(unsigned)ready,(long long)elapsed,(unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),(unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
}
static void inject(void *argument)
{
    char command=(char)(intptr_t)argument;if(run.active)return;
    if(!run.timer)run.timer=lv_timer_create(observe,10,NULL);
    int x=180,y=145,dx=0,frames=1;
    bool english=contains(lv_scr_act(),"Phone upload");
    switch(command) {
    case 'M':run.expected=NULL;break;
    case 'L':run.expected=english?"手机上传":"Phone upload";x=270;y=90;break;
    case 'U':run.expected=english?"Connect phone":"连接手机";y=198;break;
    case 'B':run.expected=NULL;x=90;y=150;dx=25;frames=7;break;
    default:return;
    }
    run.command=command;run.start=esp_timer_get_time();run.active=true;
    sdgoods_tap_synth_now(x,y,dx,0,frames);
}
void sdgoods_console_ext_cmd(char command)
{if(command=='M'||command=='L'||command=='U'||command=='B')sdgoods_lvgl_post(inject,(void *)(intptr_t)command);}
