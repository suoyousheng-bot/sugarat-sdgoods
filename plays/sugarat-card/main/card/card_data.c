/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "card_data.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool utf8(const char *s, size_t capacity)
{
    size_t n=strnlen(s,capacity);if(n==capacity)return false;
    for(size_t i=0;i<n;) {
        unsigned char c=(unsigned char)s[i++];unsigned cp=c,remaining=0,min=0;
        if(c<32||c==127)return false;
        if(c<128)continue;
        if(c>=0xc2&&c<=0xdf){cp=c&31;remaining=1;min=0x80;}
        else if(c>=0xe0&&c<=0xef){cp=c&15;remaining=2;min=0x800;}
        else if(c>=0xf0&&c<=0xf4){cp=c&7;remaining=3;min=0x10000;}else return false;
        while(remaining--) {if(i>=n)return false;c=(unsigned char)s[i++];if((c&0xc0)!=0x80)return false;cp=(cp<<6)|(c&63);}
        if(cp<min||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return false;
    }
    return true;
}
bool card_profile_valid(const card_profile_t *p,bool require_name)
{
    if(!utf8(p->name,sizeof(p->name))||!utf8(p->company,sizeof(p->company))||!utf8(p->role,sizeof(p->role))||
       !utf8(p->contact,sizeof(p->contact))||!utf8(p->link,sizeof(p->link))||p->kind>CARD_WECHAT||p->kind<CARD_EMAIL||
       (require_name&&!p->name[0]))return false;
    if(p->link[0]) {
        if(strncmp(p->link,"https://",8)&&strncmp(p->link,"http://",7))return false;
        for(const unsigned char *s=(const unsigned char *)p->link;*s;++s)if(*s<=32||*s>=127)return false;
    }
    if(p->contact[0]&&p->kind==CARD_EMAIL&&!strchr(p->contact,'@'))return false;
    return true;
}
static cJSON *profile_object(const card_profile_t *p)
{
    cJSON *o=cJSON_CreateObject();if(!o)return NULL;
    if(!cJSON_AddStringToObject(o,"name",p->name)||!cJSON_AddStringToObject(o,"company",p->company)||
       !cJSON_AddStringToObject(o,"role",p->role)||!cJSON_AddStringToObject(o,"contact",p->contact)||
       !cJSON_AddStringToObject(o,"link",p->link)||!cJSON_AddNumberToObject(o,"kind",p->kind)){cJSON_Delete(o);return NULL;}
    return o;
}
static bool string_field(cJSON *o,const char *name,char *out,size_t cap)
{
    cJSON *item=cJSON_GetObjectItemCaseSensitive(o,name);
    if(!cJSON_IsString(item)||strlen(item->valuestring)>=cap)return false;
    strcpy(out,item->valuestring);return true;
}
static bool read_profile(cJSON *o,card_profile_t *out,bool require_name)
{
    card_profile_t p={0};cJSON *kind=cJSON_GetObjectItemCaseSensitive(o,"kind");
    if(!cJSON_IsObject(o)||cJSON_GetArraySize(o)!=6||!cJSON_IsNumber(kind)||kind->valuedouble<0||kind->valuedouble>2||kind->valuedouble!=(int)kind->valuedouble||
       !string_field(o,"name",p.name,sizeof(p.name))||!string_field(o,"company",p.company,sizeof(p.company))||
       !string_field(o,"role",p.role,sizeof(p.role))||!string_field(o,"contact",p.contact,sizeof(p.contact))||
       !string_field(o,"link",p.link,sizeof(p.link)))return false;
    p.kind=(card_contact_kind_t)kind->valueint;if(!card_profile_valid(&p,require_name))return false;*out=p;return true;
}
static cJSON *parse(const char *json,size_t size,size_t maximum)
{
    if(!size||size>maximum||memchr(json,0,size))return NULL;
    unsigned depth=0;bool quoted=false;
    for(size_t i=0;i<size;++i) {
        if(json[i]=='\\'&&quoted){if(i+5<size&&!memcmp(json+i,"\\u0000",6))return NULL;++i;continue;}
        if(json[i]=='"'){quoted=!quoted;continue;}
        if(!quoted&&(json[i]=='{'||json[i]=='[')){if(++depth>4)return NULL;}
        else if(!quoted&&(json[i]=='}'||json[i]==']')){if(!depth)return NULL;--depth;}
    }
    const char *end=NULL;cJSON *o=cJSON_ParseWithLengthOpts(json,size,&end,false);
    if(!o)return NULL;
    while(end<json+size&&(*end==' '||*end=='\n'||*end=='\r'||*end=='\t'))++end;
    if(end!=json+size){cJSON_Delete(o);return NULL;}return o;
}
card_result_t card_profile_parse(const char *json,size_t size,card_profile_t *out)
{
    cJSON *o=parse(json,size,CARD_JSON_MAX);if(!o)return CARD_INVALID;
    bool ok=read_profile(o,out,true);cJSON_Delete(o);return ok?CARD_OK:CARD_INVALID;
}
static size_t print(cJSON *o,char *out,size_t capacity)
{
    if(!o)return 0;
    bool ok=capacity<=INT32_MAX&&cJSON_PrintPreallocated(o,out,(int)capacity,false);
    cJSON_Delete(o);return ok?strlen(out):0;
}
size_t card_profile_json(const card_profile_t *p,char *out,size_t capacity)
{return card_profile_valid(p,false)?print(profile_object(p),out,capacity):0;}
static bool append(char *out,size_t capacity,size_t *n,const char *s)
{
    size_t size=strlen(s);if(*n+size>=capacity)return false;memcpy(out+*n,s,size+1);*n+=size;return true;
}
static bool vfield(char *out,size_t capacity,size_t *n,const char *key,const char *value)
{
    if(!value[0])return true;
    if(!append(out,capacity,n,key))return false;
    for(const char *p=value;*p;++p) {
        if((*p==','||*p==';'||*p=='\\')&&!append(out,capacity,n,"\\"))return false;
        char s[]={*p,0};if(!append(out,capacity,n,s))return false;
    }
    return append(out,capacity,n,"\r\n");
}
size_t card_profile_vcard(const card_profile_t *p,char *out,size_t capacity)
{
    size_t n=0;if(!card_profile_valid(p,true)||!capacity)return 0;out[0]=0;
    const char *contact_keys[]={"EMAIL:","TEL;TYPE=CELL:","NOTE:WeChat "};
    if(!append(out,capacity,&n,"BEGIN:VCARD\r\nVERSION:3.0\r\n")||!vfield(out,capacity,&n,"FN:",p->name)||
       !vfield(out,capacity,&n,"ORG:",p->company)||!vfield(out,capacity,&n,"TITLE:",p->role)||
       !vfield(out,capacity,&n,contact_keys[p->kind],p->contact)||!vfield(out,capacity,&n,"URL:",p->link)||
       !append(out,capacity,&n,"END:VCARD\r\n"))return 0;
    return n;
}
void card_id_hex(const uint8_t id[16],char out[33])
{static const char hex[]="0123456789abcdef";for(unsigned i=0;i<16;++i){out[2*i]=hex[id[i]>>4];out[2*i+1]=hex[id[i]&15];}out[32]=0;}
static bool read_id(cJSON *o,uint8_t out[16])
{
    if(!cJSON_IsString(o)||strlen(o->valuestring)!=32)return false;
    for(unsigned i=0;i<16;++i) {
        unsigned byte=0;for(unsigned j=0;j<2;++j){char c=o->valuestring[i*2+j];if(c>='0'&&c<='9')byte=byte*16+(unsigned)(c-'0');else if(c>='a'&&c<='f')byte=byte*16+(unsigned)(c-'a'+10);else return false;}
        out[i]=(uint8_t)byte;
    }
    uint8_t any=0;for(unsigned i=0;i<16;++i)any|=out[i];return any!=0;
}
size_t card_state_json(const card_state_t *s,char *out,size_t capacity)
{
    if(s->count>CARD_CONTACTS_MAX)return 0;
    cJSON *o=cJSON_CreateObject(),*a=cJSON_CreateArray();if(!o||!a){cJSON_Delete(o);cJSON_Delete(a);return 0;}
    char id[33];card_id_hex(s->id,id);
    if(!cJSON_AddItemToObject(o,"contacts",a)){cJSON_Delete(a);cJSON_Delete(o);return 0;}
    if(!cJSON_AddNumberToObject(o,"version",1)||!cJSON_AddStringToObject(o,"id",id)||
       !cJSON_AddItemToObject(o,"profile",profile_object(&s->profile))){cJSON_Delete(o);return 0;}
    for(unsigned i=0;i<s->count;++i) {
        cJSON *item=cJSON_CreateObject();card_id_hex(s->contacts[i].id,id);
        if(!item||!cJSON_AddStringToObject(item,"id",id)||!cJSON_AddItemToObject(item,"profile",profile_object(&s->contacts[i].profile))||!cJSON_AddItemToArray(a,item)){
            cJSON_Delete(item);cJSON_Delete(o);return 0;
        }
    }
    return print(o,out,capacity);
}
card_result_t card_state_parse(const char *json,size_t size,card_state_t *out)
{
    cJSON *o=parse(json,size,CARD_STORE_MAX);if(!o)return CARD_INVALID;
    cJSON *version=cJSON_GetObjectItemCaseSensitive(o,"version"),*contacts=cJSON_GetObjectItemCaseSensitive(o,"contacts");
    if(!cJSON_IsNumber(version)||version->valuedouble!=1){cJSON_Delete(o);return CARD_VERSION;}
    card_state_t *s=calloc(1,sizeof(*s));if(!s){cJSON_Delete(o);return CARD_IO;}
    bool ok=cJSON_GetArraySize(o)==4&&read_id(cJSON_GetObjectItemCaseSensitive(o,"id"),s->id)&&
        read_profile(cJSON_GetObjectItemCaseSensitive(o,"profile"),&s->profile,false)&&cJSON_IsArray(contacts)&&cJSON_GetArraySize(contacts)<=CARD_CONTACTS_MAX;
    for(cJSON *item=ok?contacts->child:NULL;item&&ok;item=item->next) {
        card_contact_t *c=&s->contacts[s->count];ok=cJSON_IsObject(item)&&cJSON_GetArraySize(item)==2&&
            read_id(cJSON_GetObjectItemCaseSensitive(item,"id"),c->id)&&memcmp(c->id,s->id,16)&&
            read_profile(cJSON_GetObjectItemCaseSensitive(item,"profile"),&c->profile,true);
        for(unsigned i=0;i<s->count&&ok;++i)if(!memcmp(c->id,s->contacts[i].id,16))ok=false;
        ++s->count;
    }
    if(ok)*out=*s;
    free(s);cJSON_Delete(o);return ok?CARD_OK:CARD_INVALID;
}
card_result_t card_state_receive(card_state_t *s,const uint8_t id[16],const card_profile_t *p)
{
    uint8_t any=0;for(unsigned i=0;i<16;++i)any|=id[i];
    if(!any||!card_profile_valid(p,true)||!memcmp(s->id,id,16))return CARD_INVALID;
    unsigned slot=0;while(slot<s->count&&memcmp(s->contacts[slot].id,id,16))++slot;
    if(slot==CARD_CONTACTS_MAX)return CARD_FULL;
    memcpy(s->contacts[slot].id,id,16);s->contacts[slot].profile=*p;if(slot==s->count)++s->count;return CARD_OK;
}
