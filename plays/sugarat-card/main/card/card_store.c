/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "card_store.h"
#include <stdlib.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
static void *buffer_alloc(size_t size){return heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
#else
static void *buffer_alloc(size_t size){return malloc(size);}
#endif

card_result_t card_store_save(card_store_t *s,const card_state_t *candidate)
{
    if(!s->writable)return CARD_READONLY;
    char *encoded=buffer_alloc(CARD_STORE_MAX),*readback=buffer_alloc(CARD_STORE_MAX);size_t capacity=CARD_STORE_MAX;
    if(!encoded||!readback){free(encoded);free(readback);return CARD_IO;}
    size_t size=card_state_json(candidate,encoded,CARD_STORE_MAX);
    bool ok=size&&s->write(s->context,encoded,size)&&s->read(s->context,readback,&capacity)==CARD_OK&&capacity==size&&!memcmp(encoded,readback,size);
    free(encoded);free(readback);s->result=ok?CARD_OK:CARD_IO;if(!ok)s->writable=false;return s->result;
}
card_result_t card_store_load(card_store_t *s,card_state_t *state,const uint8_t fresh_id[16])
{
    char *bytes=buffer_alloc(CARD_STORE_MAX);if(!bytes){s->writable=false;return s->result=CARD_IO;}
    size_t size=CARD_STORE_MAX;card_result_t result=s->read(s->context,bytes,&size);
    if(result==CARD_FULL) {
        memset(state,0,sizeof(*state));memcpy(state->id,fresh_id,16);free(bytes);
        return card_store_save(s,state);
    }
    if(result==CARD_OK)result=card_state_parse(bytes,size,state);
    free(bytes);s->result=result;if(result!=CARD_OK)s->writable=false;return result;
}
#include "card_crypto.h"
#ifdef ESP_PLATFORM
#include "nvs.h"
#include "sdgoods_nvs.h"
#include "sdgoods_app_sdk.h"
#endif
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

typedef struct {
#ifdef ESP_PLATFORM
    nvs_handle_t nvs;
#endif
    char directory[192];uint64_t sequence;unsigned slot;
} backend_t;
static uint64_t decode_number(const uint8_t *p,unsigned count)
{uint64_t n=0;for(unsigned i=0;i<count;++i)n|=(uint64_t)p[i]<<(8*i);return n;}
static void encode_number(uint8_t *p,uint64_t n,unsigned count)
{for(unsigned i=0;i<count;++i){p[i]=(uint8_t)n;n>>=8;}}
static void file_path(const backend_t *b,unsigned slot,char path[240])
{snprintf(path,240,"%s/card-%c.dat",b->directory,slot?'b':'a');}
static card_result_t file_read(void *context,char *out,size_t *size)
{
    backend_t *b=context;char *candidate=buffer_alloc(CARD_STORE_MAX);if(!candidate)return CARD_IO;
    bool found=false;uint64_t latest=0;size_t length=0;card_result_t result=CARD_FULL;
    for(unsigned slot=0;slot<2;++slot) {
        char path[240];file_path(b,slot,path);FILE *f=fopen(path,"rb");
        if(!f){if(errno==ENOENT)continue;result=CARD_IO;goto done;}
        uint8_t header[48],hash[32];bool ok=fread(header,1,48,f)==48&&!memcmp(header,"SGF1",4);
        size_t n=ok?(size_t)decode_number(header+12,4):0;uint64_t sequence=ok?decode_number(header+4,8):0;
        ok=ok&&sequence&&n&&n<=CARD_STORE_MAX&&n<=*size&&fread(candidate,1,n,f)==n&&fgetc(f)==EOF&&!ferror(f)&&
            card_sha256(candidate,n,hash)&&card_secure_equal(hash,header+16,32);
        if(fclose(f))ok=false;
        if(!ok){result=CARD_IO;goto done;}
        if(found&&sequence==latest&&(length!=n||memcmp(out,candidate,n))){result=CARD_IO;goto done;}
        if(!found||sequence>latest){memcpy(out,candidate,n);length=n;latest=sequence;b->slot=slot;found=true;}
    }
    if(found){b->sequence=latest;*size=length;result=CARD_OK;}
 done:
    free(candidate);return result;
}
static bool file_write(void *context,const char *data,size_t size)
{
    backend_t *b=context;if(b->sequence==UINT64_MAX)return false;uint8_t header[48]={"SGF1"};
    encode_number(header+4,b->sequence+1,8);encode_number(header+12,size,4);if(!card_sha256(data,size,header+16))return false;
    char path[240];file_path(b,b->sequence?1-b->slot:0,path);FILE *f=fopen(path,"wb");if(!f)return false;
    bool ok=fwrite(header,1,48,f)==48&&fwrite(data,1,size,f)==size&&fflush(f)==0&&fsync(fileno(f))==0;
    if(fclose(f))ok=false;
    return ok;
}
bool card_store_files_open(card_store_t *store,const char *directory)
{
    memset(store,0,sizeof(*store));if(!directory[0]||strlen(directory)>=192)return false;
    backend_t *b=calloc(1,sizeof(*b));if(!b)return false;strcpy(b->directory,directory);
    *store=(card_store_t){.context=b,.read=file_read,.write=file_write,.writable=true};return true;
}
#ifdef ESP_PLATFORM
static card_result_t nvs_read(void *context,char *out,size_t *size)
{
    backend_t *b=context;esp_err_t e=nvs_get_blob(b->nvs,"state",out,size);
    return e==ESP_OK?CARD_OK:e==ESP_ERR_NVS_NOT_FOUND?CARD_FULL:CARD_IO;
}
static bool nvs_write(void *context,const char *data,size_t size)
{
    nvs_stats_t stats;
    if(nvs_get_stats(NULL,&stats)!=ESP_OK||stats.available_entries<size/32+160)return false;
    backend_t *b=context;return nvs_set_blob(b->nvs,"state",data,size)==ESP_OK&&nvs_commit(b->nvs)==ESP_OK;
}
bool card_store_open(card_store_t *store)
{
    memset(store,0,sizeof(*store));sdgoods_nvs_ensure();nvs_handle_t probe=0;size_t size=0;
    esp_err_t opened=nvs_open("sugarat_card",NVS_READONLY,&probe);
    if(opened!=ESP_OK&&opened!=ESP_ERR_NVS_NOT_FOUND)return false;
    esp_err_t existing=opened==ESP_OK?nvs_get_blob(probe,"state",NULL,&size):ESP_ERR_NVS_NOT_FOUND;
    if(probe)nvs_close(probe);
    if(existing!=ESP_OK&&existing!=ESP_ERR_NVS_NOT_FOUND)return false;
    if(existing==ESP_ERR_NVS_NOT_FOUND) {
        char directory[192];esp_err_t mounted=sdgoods_appdata_begin("sugarat_card",directory,sizeof(directory));
        if(mounted==ESP_OK)return card_store_files_open(store,directory);
        if(mounted!=ESP_ERR_NOT_FOUND)return false;
    }
    backend_t *b=calloc(1,sizeof(*b));if(!b)return false;
    if(nvs_open("sugarat_card",NVS_READWRITE,&b->nvs)!=ESP_OK){free(b);return false;}
    *store=(card_store_t){.context=b,.read=nvs_read,.write=nvs_write,.writable=true};return true;
}
#endif
void card_store_close(card_store_t *store)
{
    backend_t *b=store->context;
    if(b){
#ifdef ESP_PLATFORM
        if(b->nvs)nvs_close(b->nvs);
#endif
        free(b);
    }
    memset(store,0,sizeof(*store));
}
