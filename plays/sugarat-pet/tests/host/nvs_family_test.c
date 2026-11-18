/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Compact-provider recovery and capacity tests; NVS is the only replaced driver. */
#include "pet_family_nvs.h"
#include "pet_crypto.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static struct {uint8_t bytes[2][PET_NVS_RECORD_MAX+1];size_t sizes[2];unsigned writes,commits;size_t available;bool fail_write,fail_commit,fail_read;} disk;
static unsigned slot(const char *key){assert(!strcmp(key,"family_a")||!strcmp(key,"family_b"));return !strcmp(key,"family_b");}
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *handle){assert(!strcmp(name,"sugarat_pet")&&mode==NVS_READWRITE);*handle=1;return ESP_OK;}
void nvs_close(nvs_handle_t handle){assert(handle==1);}
esp_err_t nvs_get_stats(const char *part,nvs_stats_t *s){assert(!part);memset(s,0,sizeof(*s));s->available_entries=disk.available;return ESP_OK;}
esp_err_t nvs_get_blob(nvs_handle_t handle,const char *key,void *out,size_t *size)
{
    assert(handle==1);if(disk.fail_read)return ESP_FAIL;unsigned i=slot(key);
    if(!disk.sizes[i])return ESP_ERR_NVS_NOT_FOUND;
    if(!out){*size=disk.sizes[i];return ESP_OK;}
    if(*size<disk.sizes[i])return ESP_ERR_NVS_INVALID_LENGTH;
    *size=disk.sizes[i];memcpy(out,disk.bytes[i],*size);return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t handle,const char *key,const void *data,size_t size)
{
    assert(handle==1&&size<=PET_NVS_RECORD_MAX);if(disk.fail_write)return ESP_FAIL;
    unsigned i=slot(key);memcpy(disk.bytes[i],data,size);disk.sizes[i]=size;disk.writes++;return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t handle){assert(handle==1);disk.commits++;return disk.fail_commit?ESP_FAIL:ESP_OK;}
static pet_id_t identity(unsigned n){pet_id_t id={{0}};id.bytes[0]=(uint8_t)n;return id;}
static void initial(pet_family_t *family)
{
    pet_model_t model;pet_model_init(&model,0);model.pet.age_ms=900000;pet_family_init(family,&model,identity(1));
}
int main(void)
{
    pet_species_register();memset(&disk,0,sizeof(disk));disk.available=600;
    pet_family_t family,restored;initial(&family);pet_family_nvs_t nvs;pet_family_store_t store;
    assert(pet_family_nvs_open(&nvs,&store,&family)&&!nvs.present&&!store.loaded);
    assert(pet_family_store_save(&store,&family)&&disk.sizes[0]<256&&store.sequence==1);
    family.members[0].care[0]=37;assert(pet_family_store_save(&store,&family));pet_family_nvs_close(&nvs);
    assert(pet_family_nvs_open(&nvs,&store,&restored)&&restored.members[0].care[0]==37&&store.sequence==2);
    disk.bytes[1][50]^=1;pet_family_nvs_close(&nvs);
    assert(pet_family_nvs_open(&nvs,&store,&restored)&&store.sequence==1&&restored.members[0].care[0]!=37);
    disk.bytes[1][50]^=1;disk.bytes[1][4]=3;pet_family_nvs_close(&nvs);
    assert(!pet_family_nvs_open(&nvs,&store,&restored)&&nvs.present&&store.loaded&&!store.writable);pet_family_nvs_close(&nvs);
    disk.bytes[1][4]=2;
    assert(pet_family_nvs_open(&nvs,&store,&restored));unsigned writes=disk.writes;disk.available=200;
    assert(!pet_family_store_save(&store,&family)&&disk.writes==writes&&!store.writable);pet_family_nvs_close(&nvs);disk.available=600;
    for(unsigned kind=0;kind<3;kind++) {
        assert(pet_family_nvs_open(&nvs,&store,&restored));
        family=restored;family.members[0].care[0]=(float)(45+kind);
        disk.fail_write=kind==0;disk.fail_commit=kind==1;disk.fail_read=kind==2;
        assert(!pet_family_store_save(&store,&family)&&!store.writable);pet_family_nvs_close(&nvs);
        disk.fail_write=disk.fail_commit=disk.fail_read=false;
        assert(pet_family_nvs_open(&nvs,&store,&restored));
        assert(restored.members[0].care[0]==(kind==0?37:(float)(45+kind)));pet_family_nvs_close(&nvs);
    }
    initial(&family);family.count=5;
    for(unsigned i=1;i<5;i++){family.members[i]=family.members[0];family.members[i].profile.id=identity(i+1);}
    for(unsigned i=0;i<16;i++) {
        pet_profile_t profile=family.members[0].profile;profile.id=identity(60+i);profile.blood[0]=profile.blood[1]=1;
        pet_friend_t *friend=pet_family_friend(&family,&profile,true);assert(friend);friend->interactions=7;friend->bonded=true;
    }
    for(unsigned i=0;i<15;i++)family.receipts[family.receipt_count++]=identity(100+i);
    pet_profile_t parents[]={family.members[0].profile,family.friends[0].profile};uint8_t seed[32]={8};
    family.pending=(pet_pending_t){.id={{9}},.peer=family.friends[0].profile.id,.phase=PET_TX_COMMIT,.coordinator=true};
    assert(pet_genetics(parents,seed,family.pending.children)&&pet_family_apply_commit(&family));
    assert(pet_family_nvs_open(&nvs,&store,&restored)&&pet_family_store_save(&store,&family));pet_family_nvs_close(&nvs);
    assert(pet_family_nvs_open(&nvs,&store,&restored)&&restored.has_egg&&restored.receipt_count==16&&restored.count==5&&restored.friend_count==16);
    assert(restored.pending.phase==PET_TX_APPLIED&&pet_id_equal(restored.egg.id,family.egg.id));
    size_t maximum=disk.sizes[0]>disk.sizes[1]?disk.sizes[0]:disk.sizes[1];assert(maximum<=3072);
    pet_family_nvs_close(&nvs);
    printf("PASS: compact family NVS records (%zu bytes at full roster/album/receipt capacity), alternating recovery, unknown-version protection, shared-space reserve, commit/readback failures and pending-egg restoration\n",maximum);
    return 0;
}
