/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Actual record, commit and encrypted exchange code with deterministic datagram scheduling. */
#include "card_store.h"
#include "card_exchange.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static card_profile_t sample(const char *name)
{
    card_profile_t p={.company="Deotaland",.role="硬件开发者",.contact="lin@example.org",.link="https://example.org/lin",.kind=CARD_EMAIL};
    strcpy(p.name,name);return p;
}
typedef struct {char bytes[CARD_STORE_MAX];size_t size;unsigned writes;bool fail,after_write,bad_read;} disk_t;
static card_result_t read_disk(void *context,char *out,size_t *size)
{
    disk_t *d=context;if(!d->size)return CARD_FULL;if(d->size>*size)return CARD_IO;memcpy(out,d->bytes,d->size);*size=d->size;
    if(d->bad_read)out[0]='!';return CARD_OK;
}
static bool write_disk(void *context,const char *bytes,size_t size)
{
    disk_t *d=context;++d->writes;if(d->fail&&!d->after_write)return false;memcpy(d->bytes,bytes,size);d->size=size;return !d->fail;
}
static void data_tests(void)
{
    card_profile_t p=sample("小林"),parsed={0};char json[CARD_JSON_MAX],vcard[1600];
    size_t n=card_profile_json(&p,json,sizeof(json));assert(n&&card_profile_parse(json,n,&parsed)==CARD_OK&&!memcmp(&p,&parsed,sizeof(p)));
    assert(card_profile_parse("{}",2,&parsed)==CARD_INVALID);
    assert(card_profile_parse(json,n-1,&parsed)==CARD_INVALID);
    const char nul[]="{\"name\":\"x\\u0000y\",\"company\":\"\",\"role\":\"\",\"contact\":\"\",\"link\":\"\",\"kind\":0}";
    assert(card_profile_parse(nul,strlen(nul),&parsed)==CARD_INVALID);
    assert(card_profile_parse("[[[[[[0]]]]]]",13,&parsed)==CARD_INVALID);
    char invalid[]="{\"name\":\"x\",\"company\":\"\",\"role\":\"\",\"contact\":\"\",\"link\":\"javascript:alert(1)\",\"kind\":0}";
    assert(card_profile_parse(invalid,strlen(invalid),&parsed)==CARD_INVALID);
    p.kind=CARD_WECHAT;strcpy(p.contact,"xiaolin_01");strcpy(p.company,"A;B,C\\D");
    assert(card_profile_vcard(&p,vcard,sizeof(vcard))&&strstr(vcard,"ORG:A\\;B\\,C\\\\D\r\n")&&strstr(vcard,"NOTE:WeChat xiaolin_01"));
    p.name[0]=(char)0xc0;p.name[1]=(char)0x80;p.name[2]=0;assert(!card_profile_valid(&p,true));
    p=sample("小林");strcpy(p.name,"bad\nname");assert(!card_profile_valid(&p,true));
    disk_t *disk=calloc(1,sizeof(*disk));card_state_t *state=calloc(1,sizeof(*state)),*copy=calloc(1,sizeof(*copy));assert(disk&&state&&copy);
    card_store_t store={disk,read_disk,write_disk,true,CARD_OK};uint8_t id[16]={1};
    assert(card_store_load(&store,state,id)==CARD_OK&&disk->writes==1);
    state->profile=sample("小林");assert(card_store_save(&store,state)==CARD_OK);
    uint8_t next_id[16]={9};assert(card_store_load(&store,copy,next_id)==CARD_OK&&!memcmp(state,copy,sizeof(*state)));
    for(unsigned i=0;i<CARD_CONTACTS_MAX;++i){uint8_t remote[16]={(uint8_t)(i+2)};card_profile_t person=sample("阿宁");assert(card_state_receive(state,remote,&person)==CARD_OK);}
    uint8_t remote[16]={22};p=sample("同好");*copy=*state;assert(card_state_receive(state,remote,&p)==CARD_FULL&&!memcmp(state,copy,sizeof(*state)));
    remote[0]=2;assert(card_state_receive(state,remote,&p)==CARD_OK&&state->count==CARD_CONTACTS_MAX&&!strcmp(state->contacts[0].profile.name,"同好"));
    assert(card_store_save(&store,state)==CARD_OK);assert(card_store_load(&store,copy,id)==CARD_OK&&!memcmp(state,copy,sizeof(*state)));
    disk->bad_read=true;assert(card_store_save(&store,state)==CARD_IO&&!store.writable);unsigned writes=disk->writes;
    assert(card_store_save(&store,state)==CARD_READONLY&&disk->writes==writes);disk->bad_read=false;
    for(unsigned after=0;after<2;++after){disk->fail=true;disk->after_write=after;store.writable=true;assert(card_store_save(&store,state)==CARD_IO&&!store.writable);}
    disk->fail=false;memcpy(disk->bytes,"{\"version\":99}",14);disk->size=14;store.writable=true;writes=disk->writes;
    assert(card_store_load(&store,copy,id)==CARD_VERSION&&!store.writable&&disk->writes==writes);
    free(disk);free(state);free(copy);
}
typedef struct endpoint endpoint_t;
typedef struct {unsigned sender;uint8_t mac[6],bytes[CARD_PACKET_MAX];size_t size;} frame_t;
typedef struct {frame_t frames[128];unsigned count,sent,chunks;endpoint_t *endpoints[2];bool lossy,tamper,reverse;} bus_t;
struct endpoint {unsigned index,saves;card_exchange_t exchange;card_state_t state;bus_t *bus;bool reject;};
static bool send_frame(void *context,const uint8_t mac[6],const uint8_t *bytes,size_t size)
{
    endpoint_t *e=context;bus_t *b=e->bus;++b->sent;if(bytes[4]==3)++b->chunks;
    if(b->lossy&&b->sent%7==0)return false;assert(b->count<128);
    frame_t *f=&b->frames[b->count++];f->sender=e->index;memcpy(f->mac,mac,6);memcpy(f->bytes,bytes,size);f->size=size;
    if(b->tamper&&b->sent%11==0&&bytes[4]!=1)f->bytes[size-1]^=128;return true;
}
static card_result_t save_received(void *context,const uint8_t id[16],const card_profile_t *p)
{
    endpoint_t *e=context;++e->saves;if(e->reject)return CARD_IO;return card_state_receive(&e->state,id,p);
}
static void deliver(bus_t *b,uint64_t now)
{
    unsigned count=b->count;b->count=0;
    for(unsigned i=0;i<count;++i) {
        frame_t *f=&b->frames[b->reverse?count-i-1:i];unsigned other=1-f->sender;uint8_t from[6]={(uint8_t)(f->sender+1)};
        if(f->mac[0]!=255&&f->mac[0]!=other+1)continue;
        card_exchange_receive(&b->endpoints[other]->exchange,from,f->bytes,f->size,now);
        card_exchange_receive(&b->endpoints[other]->exchange,from,f->bytes,f->size,now);
    }
}
static void run(bus_t *bus,uint64_t start,unsigned duration)
{
    for(uint64_t now=start;now<start+duration;now+=100){for(unsigned i=0;i<2;++i)card_exchange_tick(&bus->endpoints[i]->exchange,now);deliver(bus,now);}
}
static void exchange_test(bool loss,bool tamper,bool reverse,bool reject)
{
    endpoint_t *a=calloc(1,sizeof(*a)),*b=calloc(1,sizeof(*b));bus_t *bus=calloc(1,sizeof(*bus));assert(a&&b&&bus);
    bus->endpoints[0]=a;bus->endpoints[1]=b;bus->lossy=loss;bus->tamper=tamper;bus->reverse=reverse;
    a->index=0;b->index=1;a->bus=b->bus=bus;b->reject=reject;a->state.id[0]=1;b->state.id[0]=2;
    a->state.profile=sample("小林");b->state.profile=sample("阿宁");
    assert(card_exchange_open(&a->exchange,&a->state,send_frame,save_received,a,0));
    assert(card_exchange_open(&b->exchange,&b->state,send_frame,save_received,b,0));run(bus,0,2500);
    assert(a->exchange.count==1&&b->exchange.count==1);
    assert(card_exchange_select(&a->exchange,0,2500)&&card_exchange_select(&b->exchange,0,2500));assert(a->exchange.code==b->exchange.code);
    card_exchange_confirm(&a->exchange);run(bus,2500,3000);assert(!a->saves&&!b->saves&&!bus->chunks);
    card_exchange_confirm(&b->exchange);run(bus,5500,20000);
    if(!reject){assert(a->exchange.phase==CARD_EX_DONE&&b->exchange.phase==CARD_EX_DONE);assert(a->saves==1&&b->saves==1&&a->state.count==1&&b->state.count==1);}
    else {assert(b->exchange.phase==CARD_EX_ERROR);assert(a->exchange.phase!=CARD_EX_DONE&&!b->state.count);run(bus,25500,45000);assert(a->exchange.phase==CARD_EX_ERROR);}
    card_exchange_close(&a->exchange);card_exchange_close(&b->exchange);assert(a->exchange.phase==CARD_EX_OFF);
    free(bus);free(a);free(b);
}
static void file_tests(const char *directory)
{
    card_store_t store;card_state_t state={0},loaded={0};uint8_t id[16]={7};
    assert(card_store_files_open(&store,directory));assert(card_store_load(&store,&state,id)==CARD_OK);
    state.profile=sample("小林");assert(card_store_save(&store,&state)==CARD_OK);strcpy(state.profile.name,"阿宁");assert(card_store_save(&store,&state)==CARD_OK);
    card_store_close(&store);assert(card_store_files_open(&store,directory));assert(card_store_load(&store,&loaded,id)==CARD_OK&&!strcmp(loaded.profile.name,"阿宁"));card_store_close(&store);
    char path[2048];snprintf(path,sizeof(path),"%s/card-a.dat",directory);FILE *f=fopen(path,"r+b");assert(f);fputc('!',f);fclose(f);
    assert(card_store_files_open(&store,directory));assert(card_store_load(&store,&loaded,id)==CARD_IO&&!store.writable);
    assert(card_store_save(&store,&state)==CARD_READONLY);card_store_close(&store);
    f=fopen(path,"rb");assert(f&&fgetc(f)=='!');fclose(f);
}
int main(int argc,char **argv)
{
    assert(argc==2);file_tests(argv[1]);
    data_tests();for(unsigned mode=0;mode<8;++mode)exchange_test(mode&1,mode&2,mode&4,false);exchange_test(true,true,true,true);
    puts("PASS: bounded UTF-8/JSON/vCard, version protection, full-store refusal, verified writes, 9 two-endpoint consent/loss/reorder/replay/tamper/save-failure scenarios");return 0;
}
