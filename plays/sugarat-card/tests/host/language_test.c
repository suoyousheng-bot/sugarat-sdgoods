/* SPDX-License-Identifier: Apache-2.0 */
/** Preference failures preserve the caller's language and never report a confirmed save. */
#include "card_language.h"
#include "nvs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char *record;
static esp_err_t open_error,get_error,set_error,commit_error;
static unsigned opened,closed;
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *handle)
{assert(!strcmp(name,"sugarat_card"));assert(mode==NVS_READONLY||mode==NVS_READWRITE);if(open_error)return open_error;*handle=1;++opened;return ESP_OK;}
esp_err_t nvs_get_str(nvs_handle_t handle,const char *key,char *value,size_t *size)
{assert(handle==1&&!strcmp(key,"lang"));if(get_error)return get_error;if(!record)return ESP_ERR_NVS_NOT_FOUND;if(strlen(record)+1>*size)return ESP_FAIL;strcpy(value,record);*size=strlen(record)+1;return ESP_OK;}
esp_err_t nvs_set_str(nvs_handle_t handle,const char *key,const char *value)
{assert(handle==1&&!strcmp(key,"lang"));if(set_error)return set_error;record=!strcmp(value,"en")?"en":"zh";return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t handle){assert(handle==1);return commit_error;}
void nvs_close(nvs_handle_t handle){assert(handle==1);++closed;}
int main(void)
{
    card_language_t language=CARD_ZH;
    open_error=ESP_ERR_NVS_NOT_FOUND;assert(card_language_load(&language)&&language==CARD_EN);
    open_error=ESP_FAIL;language=CARD_EN;assert(!card_language_load(&language)&&language==CARD_EN);assert(!card_language_save(CARD_ZH));
    open_error=ESP_OK;language=CARD_ZH;assert(card_language_load(&language)&&language==CARD_EN);
    assert(card_language_save(CARD_EN));assert(card_language_load(&language)&&language==CARD_EN);
    assert(card_language_save(CARD_ZH));assert(card_language_load(&language)&&language==CARD_ZH);
    record="xx";language=CARD_EN;assert(!card_language_load(&language)&&language==CARD_EN);
    get_error=ESP_FAIL;assert(!card_language_load(&language)&&language==CARD_EN);assert(!card_language_save(CARD_ZH));get_error=ESP_OK;
    set_error=ESP_FAIL;assert(!card_language_save(CARD_EN));set_error=ESP_OK;
    commit_error=ESP_FAIL;assert(!card_language_save(CARD_EN));commit_error=ESP_OK;
    assert(opened==closed);puts("PASS: independent NVS language default, persistence, invalid setting and unconfirmed writes");return 0;
}
