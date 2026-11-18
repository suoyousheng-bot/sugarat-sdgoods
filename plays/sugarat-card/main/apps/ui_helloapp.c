/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Firmware-lifetime owner. Only this LVGL thread mutates cards and exchange state. */
#include "ui_helloapp.h"
#include "card_view.h"
#include "card_store.h"
#include "card_network.h"
#include "sdgoods_board.h"
#include "sdgoods_cc.h"
#include "sdgoods_input.h"
#include "sdgoods_swipe_back.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>
#define T(zh,en) ((card_message_t){zh,en})

typedef struct {
    card_state_t state;
    card_store_t store;
    card_exchange_t exchange;
    card_network_t network;
    card_view_t view;
    unsigned rendered_generation;
    bool ready,network_blocked;
} card_application_t;
static card_application_t application;
static uint64_t now_ms(void){return (uint64_t)esp_timer_get_time()/1000;}
static bool close_network(card_application_t *a)
{
    bool ok=card_network_close(&a->network);card_exchange_close(&a->exchange);a->rendered_generation=0;
    if(!ok){a->network_blocked=true;a->view.notice=T("网络未能关闭，请重启设备","Network could not close. Restart badge.");a->view.page=CARD_PAGE_NOTICE;}
    return ok;
}
static card_result_t commit(card_application_t *a,const card_state_t *candidate)
{
    sdgoods_touch_flash_window(3000);card_result_t result=card_store_save(&a->store,candidate);sdgoods_touch_flash_window_end();
    if(result==CARD_OK)a->state=*candidate;else a->view.notice=T("存储未能确认，请重启后检查","Save unconfirmed. Restart and check.");return result;
}
static card_result_t receive_card(void *context,const uint8_t id[16],const card_profile_t *profile)
{
    card_application_t *a=context;if(!card_view_profile_supported(profile))return CARD_INVALID;
    card_state_t *candidate=malloc(sizeof(*candidate));if(!candidate)return CARD_IO;*candidate=a->state;
    card_result_t result=card_state_receive(candidate,id,profile);if(result==CARD_OK)result=commit(a,candidate);free(candidate);return result;
}
static bool send_card(void *context,const uint8_t mac[6],const uint8_t *bytes,size_t size)
{return card_network_send(&((card_application_t *)context)->network,mac,bytes,size);}
static void error_page(card_application_t *a,card_message_t notice)
{a->view.notice=notice;a->view.page=CARD_PAGE_NOTICE;}
static void back(void)
{
    card_application_t *a=&application;card_view_t *v=&a->view;if(!a->ready||v->paused)return;
    if(v->visible){card_view_drawer(v,false);return;}
    if(!close_network(a)){card_view_refresh(v);return;}
    v->page=CARD_PAGE_HOME;v->selected=0;
    if(a->store.writable)v->notice=(card_message_t){0};
    card_view_refresh(v);
}
static void action(void *context,card_action_t action)
{
    card_application_t *a=context;card_view_t *v=&a->view;
    if(a->network_blocked){error_page(a,T("网络未能关闭，请重启设备","Network could not close. Restart badge."));card_view_refresh(v);return;}
    switch(action) {
    case CARD_ACT_LANGUAGE: {
        card_language_t next=v->language==CARD_ZH?CARD_EN:CARD_ZH;
        sdgoods_touch_flash_window(500);bool saved=card_language_save(next);sdgoods_touch_flash_window_end();
        if(saved){v->language=next;if(a->store.writable)v->notice=(card_message_t){0};}
        else {v->notice=T("语言保存未确认，请重启后检查","Language save unconfirmed. Restart and check.");card_view_drawer(v,false);}
        break;
    }
    case CARD_ACT_COUNT:break;
    case CARD_ACT_QR:card_view_pause(v,false);v->page=CARD_PAGE_QR;break;
    case CARD_ACT_EXCHANGE:
        card_view_pause(v,false);if(!close_network(a))break;v->selected=0;v->notice=(card_message_t){0};
        if(!a->store.writable){error_page(a,T("存储不可写，不能交换名片","Storage unavailable. Exchange is disabled."));break;}
        if(!card_profile_valid(&a->state.profile,true)){error_page(a,T("请先编辑并保存名片","Edit and save your card first."));break;}
        if(!card_network_radio(&a->network)){a->network_blocked=a->network.wifi_owned||a->network.server;error_page(a,a->network.notice);break;}
        if(!card_exchange_open(&a->exchange,&a->state,send_card,receive_card,a,now_ms())){
            card_message_t notice=a->exchange.notice;if(close_network(a))error_page(a,notice);break;
        }
        v->page=CARD_PAGE_NEARBY;break;
    case CARD_ACT_EDIT:
        card_view_pause(v,false);if(!close_network(a))break;v->notice=(card_message_t){0};v->edit_connected=false;
        if(!a->store.writable){error_page(a,T("存储不可写，不能保存编辑","Storage unavailable. Editing is disabled."));break;}
        if(!card_network_portal(&a->network,&a->state.profile,now_ms(),false,v->language)){a->network_blocked=a->network.wifi_owned||a->network.server;error_page(a,a->network.notice);break;}
        v->ssid=a->network.ssid;v->password=a->network.password;v->page=CARD_PAGE_EDIT;break;
    case CARD_ACT_CONTACTS:
        card_view_pause(v,false);if(!close_network(a))break;v->page=CARD_PAGE_CONTACTS;v->selected=0;break;
    case CARD_ACT_SELECT:
        if(card_exchange_select(&a->exchange,v->selected,now_ms()))v->page=CARD_PAGE_VERIFY;
        else if(a->exchange.phase==CARD_EX_ERROR)error_page(a,a->exchange.notice);
        break;
    case CARD_ACT_CONFIRM:card_exchange_confirm(&a->exchange);break;
    case CARD_ACT_CANCEL:back();return;
    case CARD_ACT_NEXT:
        if(v->page==CARD_PAGE_NEARBY&&a->exchange.count)v->selected=(v->selected+1)%a->exchange.count;
        else if(v->page==CARD_PAGE_CONTACTS&&v->selected+1<a->state.count)++v->selected;
        break;
    case CARD_ACT_EDIT_SECURITY: {
        bool password_required=!a->network.password[0];
        if(!close_network(a))break;
        v->edit_connected=false;v->notice=(card_message_t){0};
        if(!card_network_portal(&a->network,&a->state.profile,now_ms(),password_required,v->language)) {
            a->network_blocked=a->network.wifi_owned||a->network.server;error_page(a,a->network.notice);break;
        }
        v->ssid=a->network.ssid;v->password=a->network.password;break;
    }
    case CARD_ACT_PREV:if(v->page==CARD_PAGE_CONTACTS&&v->selected)--v->selected;break;
    }
    card_view_refresh(v);
}
static void pause_app(void)
{
    card_application_t *a=&application;card_view_pause(&a->view,true);
    if(!close_network(a)){card_view_refresh(&a->view);return;}a->view.page=CARD_PAGE_HOME;
    if(a->store.writable)a->view.notice=(card_message_t){0};
    card_view_refresh(&a->view);
}
static void resume_app(void){card_view_pause(&application.view,false);card_view_refresh(&application.view);}
esp_err_t __wrap_sdgoods_launcher_return_to_launcher(void)
{
    extern esp_err_t __real_sdgoods_launcher_return_to_launcher(void);
    if(application.ready&&!close_network(&application)){card_view_refresh(&application.view);return ESP_FAIL;}
    return __real_sdgoods_launcher_return_to_launcher();
}
void sdgoods_app_pre_poweroff(void)
{if(application.ready&&!close_network(&application))ESP_LOGE("sugarat_card","Network shutdown unconfirmed before power-off");}
void ui_helloapp_start(void)
{
    card_application_t *a=&application;
    if(a->ready){lv_scr_load(a->view.screen);return;}
    sw_init();card_result_t result=CARD_IO;uint8_t fresh[16];
    if(card_random_bytes(fresh,sizeof(fresh))&&card_store_open(&a->store)) {
        sdgoods_touch_flash_window(3000);result=card_store_load(&a->store,&a->state,fresh);sdgoods_touch_flash_window_end();
    }
    card_view_create(&a->view,&a->state,&a->exchange,action,a);
    if(!card_language_load(&a->view.language))a->view.notice=T("语言读取失败，请重启后检查","Language unavailable. Restart and check.");
    card_view_refresh(&a->view);
    if(result!=CARD_OK){a->view.notice=T("存储不可用，原有资料不会覆盖","Storage unavailable. Existing cards are kept.");a->store.writable=false;card_view_refresh(&a->view);}
    lv_scr_load(a->view.screen);sdgoods_swipe_back_bind(a->view.screen,back);sdgoods_app_shell_bind(a->view.screen);
    sdgoods_app_shell_set_pause_cb(pause_app);sdgoods_app_shell_set_resume_cb(resume_app);a->ready=true;
    ESP_LOGI("sugarat_card","SUGARAT_CARD_READY version=0.1.2 writable=%d contacts=%u",a->store.writable,a->state.count);
}
void ui_helloapp_poll(void)
{
    card_application_t *a=&application;if(!a->ready||a->view.paused||a->network_blocked)return;uint64_t now=now_ms();
    if(a->network.mode==CARD_NET_RADIO) {
        card_network_poll(&a->network,&a->exchange,now);card_exchange_tick(&a->exchange,now);
        if(a->exchange.phase==CARD_EX_DONE&&now>=a->exchange.deadline) {
            if(!card_network_close(&a->network)){a->network_blocked=true;error_page(a,T("网络未能关闭，请重启设备","Network could not close. Restart badge."));}
            card_crypto_close(&a->exchange.crypto);memset(a->exchange.tx_key,0,32);memset(a->exchange.rx_key,0,32);
            memset(a->exchange.outgoing,0,sizeof(a->exchange.outgoing));memset(a->exchange.incoming,0,sizeof(a->exchange.incoming));card_view_refresh(&a->view);return;
        }
        if(a->exchange.phase==CARD_EX_ERROR){card_message_t notice=a->exchange.notice;if(close_network(a))error_page(a,notice);card_view_refresh(&a->view);}
        else if(a->rendered_generation!=a->exchange.generation){a->rendered_generation=a->exchange.generation;card_view_refresh(&a->view);}
    } else if(a->network.mode==CARD_NET_PORTAL) {
        if(now>=a->network.deadline){if(close_network(a))error_page(a,T("编辑热点已到期，请重新打开","Hotspot expired. Open Edit again."));card_view_refresh(&a->view);return;}
        bool connected;
        if(!card_network_connected(&a->network,&connected)) {
            if(close_network(a))error_page(a,T("热点状态读取失败，请返回重试","Hotspot status unavailable. Go back and retry."));
            card_view_refresh(&a->view);return;
        }
        if(connected!=a->view.edit_connected){a->view.edit_connected=connected;card_view_refresh(&a->view);}
        card_profile_t profile;
        if(card_network_pending(&a->network,&profile)) {
            bool ok=card_view_profile_supported(&profile);card_state_t *candidate=ok?malloc(sizeof(*candidate)):NULL;
            a->view.notice=ok?(card_message_t){0}:T("含未支持的文字，请调整后重试","Unsupported characters. Edit and retry.");
            if(candidate){*candidate=a->state;candidate->profile=profile;ok=commit(a,candidate)==CARD_OK;free(candidate);}
            else {if(ok)a->view.notice=T("内存不足，请稍后重试","Not enough memory. Try again later.");ok=false;}
            card_network_complete(&a->network,ok,&profile);
            if(!ok)ESP_LOGW("sugarat_card","Profile save rejected or unconfirmed");
            card_view_refresh(&a->view);
        }
    }
    if(a->view.render_pending&&!a->view.animating&&!a->view.dragging)card_view_refresh(&a->view);
}
