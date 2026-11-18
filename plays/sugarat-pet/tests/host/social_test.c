/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Two actual state machines and crypto implementations; the datagram medium alone is simulated. */
#include "pet_social.h"
#include "pet_family_store.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
typedef struct endpoint endpoint_t;
typedef struct {unsigned from;uint8_t bytes[250];size_t size;} packet_t;
static packet_t packets[4096];static unsigned head,tail,sent,dropped,duplicate;
static endpoint_t *endpoints[2];
struct endpoint {pet_family_t family;pet_social_t social;uint8_t saved[PET_FAMILY_BYTES];unsigned index,writes,fail_at;bool crashed,ambiguous;};
static bool save(void *context)
{
    endpoint_t *e=context;e->writes++;
    if(e->writes==e->fail_at) {
        if(e->ambiguous)assert(pet_family_encode(&e->family,e->writes+1,e->saved));
        e->crashed=true;return false;
    }
    assert(pet_family_encode(&e->family,e->writes+1,e->saved));return true;
}
static bool transmit(void *context,const uint8_t mac[6],const uint8_t *bytes,size_t size)
{
    endpoint_t *e=context;(void)mac;assert(size<=250);sent++;
    if(sent%5==0) {dropped++;return true;}
    assert(tail-head<4094);packet_t *p=&packets[tail++%4096];p->from=e->index;p->size=size;memcpy(p->bytes,bytes,size);
    if(sent%11==0) {packets[tail++%4096]=*p;duplicate++;}
    return true;
}
static void initialize(endpoint_t *e,unsigned index)
{
    memset(e,0,sizeof(*e));e->index=index;
    pet_model_t m;pet_model_init(&m,0);m.pet.age_ms=900000;m.pet.energy=90;
    pet_id_t id={{0}};id.bytes[0]=(uint8_t)(index+1);pet_family_init(&e->family,&m,id);
    e->family.members[0].profile.blood[0]=e->family.members[0].profile.blood[1]=(uint8_t)index;
    assert(pet_family_encode(&e->family,1,e->saved));
}
static void restart_endpoint(endpoint_t *e,uint64_t now)
{
    pet_social_close(&e->social);uint32_t sequence;
    assert(pet_family_decode(&e->family,&sequence,e->saved,sizeof(e->saved))==PET_FAMILY_OK);
    e->fail_at=0;e->crashed=false;
    assert(pet_social_open(&e->social,&e->family,transmit,save,e,now));
}
static bool complete(endpoint_t e[2])
{return e[0].family.has_egg&&e[1].family.has_egg&&!e[0].family.pending.phase&&!e[1].family.pending.phase&&e[0].social.state==PET_SOCIAL_LINKED&&e[1].social.state==PET_SOCIAL_LINKED;}
static bool run(endpoint_t e[2],uint64_t start)
{
    bool manual=!(e[0].family.friend_count&&e[0].family.friends[0].bonded&&e[1].family.friend_count&&e[1].family.friends[0].bonded);
    for(uint64_t t=start;t<start+180000;t+=100) {
        for(unsigned i=0;i<2;i++) {
            pet_social_t *s=&e[i].social;pet_social_tick(s,t);
            if(s->state==PET_SOCIAL_SEARCH&&s->nearby_count)assert(pet_social_select(s,0));
            if(manual&&s->state==PET_SOCIAL_VERIFY)assert(pet_social_confirm(s));
            if(s->state==PET_SOCIAL_LINKED) {
                assert(s->code==e[i^1].social.code);
                pet_friend_t *f=pet_family_friend(&e[i].family,&s->peer,false);assert(f);
                if(i==0) {if(!(f->interactions&1))pet_social_interact(s,0);if(!(f->interactions&2))pet_social_interact(s,1);}
                else if(!(f->interactions&4))pet_social_interact(s,2);
                if(!s->wants_egg&&pet_family_can_breed(&e[i].family,&s->peer))assert(pet_social_breed(s));
            }
            if(e[i].crashed)return false;
        }
        /* Reverse pairs periodically, inject authenticated-header and ciphertext damage,
         * then deliver the valid datagram: failures must not consume its replay slot. */
        if(t%700==0&&tail-head>=2) {packet_t p=packets[head%4096];packets[head%4096]=packets[(head+1)%4096];packets[(head+1)%4096]=p;}
        unsigned budget=32;
        while(head<tail&&budget--) {
            packet_t p=packets[head++%4096];unsigned target=p.from^1;uint8_t mac[6]={0};mac[5]=(uint8_t)(p.from+1);
            if(p.bytes[5]>=3) {p.bytes[28]^=1;pet_social_receive(&e[target].social,mac,p.bytes,p.size);p.bytes[28]^=1;}
            pet_social_receive(&e[target].social,mac,p.bytes,p.size);
            if(e[target].crashed)return false;
        }
        if(complete(e))return true;
    }
    fprintf(stderr,"stalled: states %u/%u phases %u/%u notices %s / %s\n",e[0].social.state,e[1].social.state,e[0].family.pending.phase,e[1].family.pending.phase,e[0].social.notice,e[1].social.notice);
    return false;
}
static void handshake_only(endpoint_t e[2],uint64_t start)
{
    for(uint64_t t=start;t<start+15000;t+=100) {
        for(unsigned i=0;i<2;i++) {
            pet_social_tick(&e[i].social,t);
            if(e[i].social.state==PET_SOCIAL_SEARCH&&e[i].social.nearby_count)assert(pet_social_select(&e[i].social,0));
        }
        unsigned budget=32;
        while(head<tail&&budget--) {
            packet_t p=packets[head++%4096];uint8_t mac[6]={0};mac[5]=(uint8_t)(p.from+1);
            pet_social_receive(&e[p.from^1].social,mac,p.bytes,p.size);
        }
    }
}
static void consent_tests(void)
{
    endpoint_t e[2];head=tail=sent=0;
    for(unsigned i=0;i<2;i++){initialize(&e[i],i);assert(pet_social_open(&e[i].social,&e[i].family,transmit,save,&e[i],0));}
    handshake_only(e,100);
    assert(e[0].social.state==PET_SOCIAL_VERIFY&&e[1].social.state==PET_SOCIAL_VERIFY);
    assert(e[0].social.code==e[1].social.code&&!e[0].family.friend_count&&!e[1].family.friend_count);
    assert(pet_social_confirm(&e[0].social));handshake_only(e,16000);
    assert(e[0].social.state!=PET_SOCIAL_LINKED&&e[1].social.state!=PET_SOCIAL_LINKED);
    assert(!e[0].family.friend_count&&!e[1].family.friend_count);
    assert(pet_social_confirm(&e[1].social));handshake_only(e,32000);
    assert(e[0].social.state==PET_SOCIAL_LINKED&&e[1].social.state==PET_SOCIAL_LINKED);
    assert(!e[0].family.has_egg&&!e[1].family.has_egg);
    for(unsigned i=0;i<2;i++)restart_endpoint(&e[i],50000);head=tail=0;
    handshake_only(e,50100);
    assert(e[0].social.known_friend&&e[1].social.known_friend);
    assert(e[0].social.state==PET_SOCIAL_LINKED&&e[1].social.state==PET_SOCIAL_LINKED);
    assert(pet_social_interact(&e[0].social,0)&&!e[0].family.friends[0].interactions);
    handshake_only(e,66000);assert(e[0].family.friends[0].interactions==1&&e[1].family.friends[0].interactions==1);
    for(unsigned i=0;i<2;i++)restart_endpoint(&e[i],82000);head=tail=0;e[0].family.friends[0].key[0]^=1;
    handshake_only(e,82100);
    assert(e[0].social.state==PET_SOCIAL_VERIFY&&e[1].social.state==PET_SOCIAL_VERIFY);
    for(unsigned i=0;i<2;i++)pet_social_close(&e[i].social);
    puts("PASS: no first bond without both code confirmations, no unsolicited egg, durable activity acknowledgement, automatic known-friend authentication and wrong-key rejection");
}
static void protocol_tests(void)
{
    for(unsigned side=0;side<2;side++)for(unsigned fail=0;fail<=10;fail++)for(unsigned ambiguous=0;ambiguous<2;ambiguous++) {
        endpoint_t e[2];head=tail=sent=0;
        for(unsigned i=0;i<2;i++) {initialize(&e[i],i);endpoints[i]=&e[i];assert(pet_social_open(&e[i].social,&e[i].family,transmit,save,&e[i],0));}
        e[side].fail_at=fail;e[side].ambiguous=ambiguous;
        if(!run(e,100)) {
            assert(e[0].crashed||e[1].crashed);head=tail=0;
            for(unsigned i=0;i<2;i++)restart_endpoint(&e[i],200000);
            if(!run(e,200100)) {fprintf(stderr,"case side=%u fail=%u ambiguous=%u confirmations %d/%d peer %d/%d seq %u/%u rx %u/%u\n",side,fail,ambiguous,e[0].social.confirmed,e[1].social.confirmed,e[0].social.peer_confirmed,e[1].social.peer_confirmed,e[0].social.tx_sequence,e[1].social.tx_sequence,e[0].social.rx_sequence,e[1].social.rx_sequence);abort();}
        }
        assert(complete(e));
        assert(e[0].family.receipt_count==1&&e[1].family.receipt_count==1);
        assert(pet_id_equal(e[0].family.receipts[0],e[1].family.receipts[0]));
        assert(!pet_id_equal(e[0].family.egg.id,e[1].family.egg.id));
        assert(pet_profile_kind(&e[0].family.egg)==PET_SPROUT&&pet_profile_kind(&e[1].family.egg)==PET_SPROUT);
        assert(memcmp(e[0].family.egg.traits,e[1].family.egg.traits,PET_TRAITS_COUNT));
        for(unsigned i=0;i<2;i++)pet_social_close(&e[i].social);
    }
    assert(dropped&&duplicate);
    puts("PASS: 44 two-device scenarios, real P-256/AES-GCM, loss/reordering/replay/tampering, save failures before/after durable write and reboot reconciliation; one distinct egg per device");
}
static void store_tests(void)
{
    char directory[]="/tmp/sugarat-family-XXXXXX";assert(mkdtemp(directory));
    endpoint_t e;initialize(&e,0);pet_family_store_t s;
    assert(pet_family_store_open(&s,directory,&e.family)&&!s.loaded);
    assert(pet_family_store_save(&s,&e.family));
    e.family.members[0].profile.age_ms=12345;assert(pet_family_store_save(&s,&e.family));
    pet_family_t restored;assert(pet_family_store_open(&s,directory,&restored)&&s.loaded&&restored.members[0].profile.age_ms==12345);
    char a[256],b[256];snprintf(a,sizeof(a),"%s/family-a.dat",directory);snprintf(b,sizeof(b),"%s/family-b.dat",directory);
    assert(truncate(b,5)==0);assert(pet_family_store_open(&s,directory,&restored)&&restored.members[0].profile.age_ms==900000);
    FILE *file=fopen(b,"wb");assert(file);assert(fwrite(e.saved,1,8192,file)==8192);assert(fclose(file)==0);
    file=fopen(b,"r+b");assert(file);uint8_t record[8192];assert(fread(record,1,8192,file)==8192);record[4]=3;
    uint32_t hash=2166136261u;for(unsigned i=0;i<8188;i++)hash=(hash^record[i])*16777619u;
    for(unsigned i=0;i<4;i++)record[8188+i]=(uint8_t)(hash>>(8*i));rewind(file);assert(fwrite(record,1,8192,file)==8192);assert(fclose(file)==0);
    assert(!pet_family_store_open(&s,directory,&restored)&&s.loaded&&!s.writable);
    assert(!pet_family_store_save(&s,&restored));
    assert(unlink(a)==0&&unlink(b)==0&&rmdir(directory)==0);
    puts("PASS: real appdata file sync/readback, previous-generation recovery and unknown-version write protection");
}
int main(void) {pet_species_register();consent_tests();protocol_tests();store_tests();return 0;}
