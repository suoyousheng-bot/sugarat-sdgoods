/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "card_language.h"
#include "nvs.h"
#include <string.h>

bool card_language_load(card_language_t *language)
{
    nvs_handle_t handle;
    esp_err_t result=nvs_open("sugarat_card",NVS_READONLY,&handle);
    if(result==ESP_ERR_NVS_NOT_FOUND){*language=CARD_EN;return true;}
    if(result!=ESP_OK)return false;
    char value[3];size_t size=sizeof(value);
    result=nvs_get_str(handle,"lang",value,&size);nvs_close(handle);
    if(result==ESP_ERR_NVS_NOT_FOUND){*language=CARD_EN;return true;}
    if(result!=ESP_OK)return false;
    if(!strcmp(value,"zh")){*language=CARD_ZH;return true;}
    if(!strcmp(value,"en")){*language=CARD_EN;return true;}
    return false;
}
bool card_language_save(card_language_t language)
{
    nvs_handle_t handle;
    if(nvs_open("sugarat_card",NVS_READWRITE,&handle)!=ESP_OK)return false;
    const char *value=language==CARD_EN?"en":"zh";
    char saved[3];size_t size=sizeof(saved);
    bool ok=nvs_set_str(handle,"lang",value)==ESP_OK&&nvs_commit(handle)==ESP_OK&&
        nvs_get_str(handle,"lang",saved,&size)==ESP_OK&&!strcmp(saved,value);
    nvs_close(handle);return ok;
}
