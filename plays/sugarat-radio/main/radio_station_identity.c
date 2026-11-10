/* Modified for Sugarat Radio by Deotaland, 2026. */
/*
 * 谷仓共创计划 · 谷仓 SDGOODS 开放平台基础工程
 * 应用层示例
 * https://github.com/SDGOODS/SDGOODS-ESP32S3
 *
 * Copyright (c) 2026 深圳希德创新网络有限公司 (SDGOODS)
 * 「谷仓共创计划」与「谷仓 SDGOODS 开放平台」项目、谷仓次元屏（谷仓电子徽章）设备，
 *   以及本基础代码的著作权与相关权利，均归深圳希德创新网络有限公司所有。
 * SPDX-License-Identifier: Apache-2.0
 *
 * 本文件属于应用层，以 Apache-2.0 发布：可自由商用、可闭源分发，
 * 只需保留本声明并携带 NOTICE 文件。详见 LICENSING.md。
 */

/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 深圳希德创新网络有限公司 (SDGOODS)
 * US station identity references are in PLAY.md.
 */
#include "radio_station_identity.h"
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

typedef struct { const char *scope, *alias, *title; float mhz; } station_identity_t;
static const station_identity_t s_identities[]={
    {NULL,"KEXP","KEXP 90.3",90.3f},
    {NULL,"KCRW","KCRW 89.9",89.9f},
    {NULL,"WNYC FM","WNYC 93.9",93.9f},
    {NULL,"WBEZ","WBEZ 91.5",91.5f},
    {NULL,"WBUR","WBUR 90.9",90.9f},
    {NULL,"KQED","KQED 88.5",88.5f},
};

static const station_identity_t *identity(const char *name)
{
    if(!name)return NULL;
    for(size_t i=0;i<sizeof(s_identities)/sizeof(s_identities[0]);++i){
        const station_identity_t *entry=&s_identities[i];
        const char *hit = strstr(name, entry->alias);
        if (hit && (hit == name || !isalnum((unsigned char)hit[-1])) &&
            !isalnum((unsigned char)hit[strlen(entry->alias)])) return entry;
    }
    return NULL;
}

float radio_station_known_frequency(const char *name)
{
    const station_identity_t *entry=identity(name);
    return entry?entry->mhz:0.0f;
}

void radio_station_copy_utf8(const char *name, char *out, size_t capacity)
{
    if(!out || !capacity)return;
    if(!name)name="";
    size_t used=0;
    while(*name){
        unsigned char lead=(unsigned char)*name;
        size_t length=lead<0x80?1:(lead&0xe0)==0xc0?2:(lead&0xf0)==0xe0?3:4;
        bool complete=true;
        for(size_t i=1;i<length;++i)if(!name[i] || ((unsigned char)name[i]&0xc0)!=0x80){complete=false;break;}
        if(!complete || used+length>=capacity)break;
        memcpy(out+used,name,length);used+=length;name+=length;
    }
    out[used]=0;
}

void radio_station_short_name(const char *name, char *out, size_t capacity)
{
    if(!out || !capacity)return;
    const station_identity_t *entry=identity(name);
    if(entry){radio_station_copy_utf8(entry->title,out,capacity);return;}
    if(!name)name="";
    radio_station_copy_utf8(name, out, capacity);
}
