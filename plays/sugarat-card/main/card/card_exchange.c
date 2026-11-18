/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "card_exchange.h"
#include <stdio.h>
#include <string.h>
#define T(zh,en) ((card_message_t){zh,en})

static const uint8_t broadcast[6]={255,255,255,255,255,255};
enum {HELLO=1,CONFIRM=2,CHUNK=3,ACK=4};
#define HEADER 30
static void fault(card_exchange_t *e,card_message_t notice)
{e->phase=CARD_EX_ERROR;e->notice=notice;++e->generation;}
static void number(uint8_t *out,uint64_t n,unsigned size)
{for(unsigned i=0;i<size;++i){out[i]=(uint8_t)n;n>>=8;}}
static uint64_t read_number(const uint8_t *in,unsigned size)
{uint64_t n=0;for(unsigned i=0;i<size;++i)n|=(uint64_t)in[i]<<(i*8);return n;}
void card_exchange_close(card_exchange_t *e)
{card_crypto_close(&e->crypto);memset(e,0,sizeof(*e));}
bool card_exchange_open(card_exchange_t *e,const card_state_t *state,card_send_fn send,card_receive_fn receive,void *context,uint64_t now)
{
    memset(e,0,sizeof(*e));e->send=send;e->receive=receive;e->context=context;
    memcpy(e->self.id,state->id,16);strcpy(e->self.name,state->profile.name);
    e->outgoing_size=card_profile_json(&state->profile,e->outgoing,sizeof(e->outgoing));
    if(!card_profile_valid(&state->profile,true)||!e->outgoing_size||!card_crypto_open(&e->crypto)||
       !card_random_bytes(e->self.nonce,16)||!card_sha256(e->outgoing,e->outgoing_size,e->tx_digest)){
        fault(e,T("交换初始化失败，请返回重试","Exchange setup failed. Go back and retry."));return false;
    }
    memcpy(e->self.public_key,e->crypto.public_key,65);e->phase=CARD_EX_DISCOVER;e->deadline=now+120000;e->generation=1;return true;
}
static void hello(card_exchange_t *e)
{
    uint8_t bytes[155]={"SGC1"};bytes[4]=HELLO;memcpy(bytes+5,e->self.id,16);memcpy(bytes+21,e->self.nonce,16);
    memcpy(bytes+37,e->self.public_key,65);size_t name=strlen(e->self.name);bytes[102]=(uint8_t)name;memcpy(bytes+103,e->self.name,name);
    e->send(e->context,broadcast,bytes,103+name);
}
bool card_exchange_select(card_exchange_t *e,unsigned index,uint64_t now)
{
    if(e->phase!=CARD_EX_DISCOVER||index>=e->count||now-e->peers[index].seen>5000)return false;
    e->peer=e->peers[index];bool low=memcmp(e->self.id,e->peer.id,16)<0;
    const card_peer_t *a=low?&e->self:&e->peer,*b=low?&e->peer:&e->self;
    uint8_t transcript[194],hash[32],base[32],code[32];
    memcpy(transcript,a->id,16);memcpy(transcript+16,a->nonce,16);memcpy(transcript+32,a->public_key,65);
    memcpy(transcript+97,b->id,16);memcpy(transcript+113,b->nonce,16);memcpy(transcript+129,b->public_key,65);
    bool ok=card_sha256(transcript,sizeof(transcript),hash)&&card_crypto_agree(&e->crypto,e->peer.public_key,hash,base)&&
        card_hmac(base,low?"low-to-high":"high-to-low",11,e->tx_key)&&card_hmac(base,low?"high-to-low":"low-to-high",11,e->rx_key)&&
        card_hmac(base,"verify",6,code);
    memset(base,0,sizeof(base));if(!ok){fault(e,T("配对校验失败，请返回重试","Pairing failed. Go back and retry."));return false;}
    e->code=(unsigned)(read_number(code,4)%1000000);e->phase=CARD_EX_VERIFY;e->deadline=now+60000;e->next_send=0;++e->generation;return true;
}
void card_exchange_confirm(card_exchange_t *e)
{if(e->phase==CARD_EX_VERIFY&&!e->local_confirmed){e->local_confirmed=true;++e->generation;}}
static bool send_encrypted(card_exchange_t *e,uint8_t type,const uint8_t *payload,size_t size)
{
    if(size>CARD_PACKET_MAX-HEADER-16||e->tx_sequence==UINT64_MAX)return false;
    uint8_t bytes[CARD_PACKET_MAX]={"SGC1"},nonce[12];bytes[4]=type;bytes[5]=1;
    memcpy(bytes+6,e->self.nonce,16);number(bytes+22,++e->tx_sequence,8);if(size)memcpy(bytes+HEADER,payload,size);
    memcpy(nonce,e->tx_key,4);number(nonce+4,e->tx_sequence,8);
    if(!card_encrypt(e->tx_key,nonce,bytes,HEADER,bytes+HEADER,size,bytes+HEADER+size))return false;
    return e->send(e->context,e->peer.mac,bytes,HEADER+size+16);
}
static void read_hello(card_exchange_t *e,const uint8_t mac[6],const uint8_t *bytes,size_t size,uint64_t now)
{
    if(e->phase!=CARD_EX_DISCOVER||size<103||bytes[102]>48||size!=(size_t)103+bytes[102]||!bytes[102]||!memcmp(bytes+5,e->self.id,16))return;
    card_profile_t check={0};memcpy(check.name,bytes+103,bytes[102]);if(!card_profile_valid(&check,true))return;
    uint8_t any=0;for(unsigned i=0;i<16;++i)any|=bytes[5+i];if(!any)return;
    unsigned slot=0;while(slot<e->count&&memcmp(e->peers[slot].id,bytes+5,16))++slot;
    if(slot==e->count){if(e->count==CARD_PEERS_MAX)return;++e->count;++e->generation;}
    card_peer_t *p=&e->peers[slot];
    if(memcmp(p->nonce,bytes+21,16)||strcmp(p->name,check.name))++e->generation;
    memset(p,0,sizeof(*p));memcpy(p->mac,mac,6);memcpy(p->id,bytes+5,16);memcpy(p->nonce,bytes+21,16);memcpy(p->public_key,bytes+37,65);strcpy(p->name,check.name);p->seen=now;
}
void card_exchange_receive(card_exchange_t *e,const uint8_t mac[6],const uint8_t *bytes,size_t size,uint64_t now)
{
    if(e->phase==CARD_EX_OFF||e->phase==CARD_EX_ERROR||size<5||size>CARD_PACKET_MAX||memcmp(bytes,"SGC1",4))return;
    if(bytes[4]==HELLO){read_hello(e,mac,bytes,size,now);return;}
    if(e->phase==CARD_EX_DISCOVER||size<HEADER+16||bytes[5]!=1||memcmp(mac,e->peer.mac,6)||memcmp(bytes+6,e->peer.nonce,16))return;
    uint64_t sequence=read_number(bytes+22,8);if(!sequence)return;
    if(sequence<=e->rx_sequence&&(e->rx_sequence-sequence>=64||((e->rx_window>>(e->rx_sequence-sequence))&1)))return;
    uint8_t payload[CARD_PACKET_MAX],nonce[12];size_t n=size-HEADER-16;memcpy(payload,bytes+HEADER,n);
    memcpy(nonce,e->rx_key,4);number(nonce+4,sequence,8);
    if(!card_decrypt(e->rx_key,nonce,bytes,HEADER,payload,n,bytes+HEADER+n))return;
    if(sequence>e->rx_sequence){uint64_t shift=sequence-e->rx_sequence;e->rx_window=shift>=64?1:(e->rx_window<<shift)|1;e->rx_sequence=sequence;}
    else e->rx_window|=UINT64_C(1)<<(e->rx_sequence-sequence);
    if(bytes[4]==CONFIRM&&n==0) {
        if(!e->peer_confirmed){e->peer_confirmed=true;++e->generation;}return;
    }
    if(!e->local_confirmed||!e->peer_confirmed)return;
    if(bytes[4]==ACK&&n==32&&card_secure_equal(payload,e->tx_digest,32)) {
        if(!e->acknowledged){e->acknowledged=true;++e->generation;}return;
    }
    if(bytes[4]!=CHUNK||n<4)return;
    unsigned total=(unsigned)read_number(payload,2),index=payload[2],length=payload[3];
    if(!total||total>=CARD_JSON_MAX||index>=sizeof(e->received)||length>CARD_CHUNK||n!=4+length||
       index*CARD_CHUNK+length>total||length!=((total-index*CARD_CHUNK)>CARD_CHUNK?CARD_CHUNK:(total-index*CARD_CHUNK)))return;
    if(e->incoming_size&&e->incoming_size!=total)return;
    e->incoming_size=total;
    if(e->saved)return;
    memcpy(e->incoming+index*CARD_CHUNK,payload+4,length);e->received[index]=1;
    for(unsigned i=0;i<(total+CARD_CHUNK-1)/CARD_CHUNK;++i)if(!e->received[i])return;
    card_profile_t profile;
    if(card_profile_parse(e->incoming,total,&profile)!=CARD_OK||!card_sha256(e->incoming,total,e->rx_digest)){fault(e,T("收到的名片无效，请重新交换","Invalid card received. Try exchanging again."));return;}
    card_result_t stored=e->receive(e->context,e->peer.id,&profile);
    if(stored!=CARD_OK){fault(e,stored==CARD_FULL?T("名片夹已满，未保存新名片","Card holder is full. New card was not saved."):T("名片保存未确认，请检查存储","Card save unconfirmed. Check storage."));return;}
    e->saved=true;++e->generation;
}
void card_exchange_tick(card_exchange_t *e,uint64_t now)
{
    if(e->phase==CARD_EX_OFF||e->phase==CARD_EX_ERROR)return;
    if(now>=e->deadline) {
        if(e->phase!=CARD_EX_DONE)fault(e,e->saved?T("本机已保存，对方结果未确认","Saved here. Check the other badge."):T("交换已超时，请返回重试","Exchange timed out. Go back and retry."));
        return;
    }
    if(e->phase==CARD_EX_DISCOVER) {
        for(unsigned i=0;i<e->count;)if(now-e->peers[i].seen>5000){e->peers[i]=e->peers[--e->count];++e->generation;}else ++i;
    }
    if(now>=e->next_hello){hello(e);e->next_hello=now+650;}
    if(e->phase==CARD_EX_DISCOVER||!e->local_confirmed||now<e->next_send)return;
    e->next_send=now+180;
    send_encrypted(e,CONFIRM,NULL,0);
    if(!e->peer_confirmed)return;
    if(e->phase==CARD_EX_VERIFY){e->phase=CARD_EX_TRANSFER;++e->generation;}
    if(!e->acknowledged) {
        unsigned count=(unsigned)((e->outgoing_size+CARD_CHUNK-1)/CARD_CHUNK),index=e->chunk++%count;
        size_t size=e->outgoing_size-index*CARD_CHUNK;if(size>CARD_CHUNK)size=CARD_CHUNK;
        uint8_t payload[CARD_CHUNK+4];number(payload,e->outgoing_size,2);payload[2]=(uint8_t)index;payload[3]=(uint8_t)size;memcpy(payload+4,e->outgoing+index*CARD_CHUNK,size);
        send_encrypted(e,CHUNK,payload,size+4);
    }
    if(e->saved)send_encrypted(e,ACK,e->rx_digest,32);
    if(e->saved&&e->acknowledged&&e->phase!=CARD_EX_DONE){e->phase=CARD_EX_DONE;e->notice=T("双方名片已保存","Both cards saved.");++e->generation;}
}
