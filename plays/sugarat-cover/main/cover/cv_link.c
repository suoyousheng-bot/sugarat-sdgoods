/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "cv_link.h"
#include <stdlib.h>
#include <string.h>
enum { HELLO=1,OFFER,ANSWER,CONFIRM,DATA,ACK,FINISH,DONE,CANCEL,ERROR_PACKET };
enum { HEADER=20,TAG=16 };
static const uint8_t broadcast[6]={255,255,255,255,255,255};
static void put32(uint8_t *p,uint32_t n){for(int i=3;i>=0;i--){p[i]=(uint8_t)n;n>>=8;}}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static uint64_t get64(const uint8_t *p){uint64_t n=0;for(unsigned i=0;i<8;i++)n=n*256+p[i];return n;}
static void failed(cv_link_t *l,cv_error_t error){l->phase=L_FAILED;l->error=error;l->cached_size=0;}
static bool emit(cv_link_t *l,unsigned type,const uint8_t *payload,size_t size,bool encrypted,bool remember)
{
    uint8_t packet[CV_PACKET]={0};if(size+HEADER+(encrypted?TAG:0)>sizeof(packet))return false;
    memcpy(packet,"SCV1",4);packet[4]=(uint8_t)type;packet[5]=encrypted;
    memcpy(packet+8,type==HELLO?l->epoch:l->session,8);
    if(encrypted) {
        if(l->sequence>=0x7ffffffeU){failed(l,CV_AUTH);return false;}
        uint32_t sequence=++l->sequence|(l->receiver?0x80000000U:0);put32(packet+16,sequence);
    }
    if(size)memcpy(packet+HEADER,payload,size);
    if(encrypted) {
        uint8_t nonce[12];memcpy(nonce,packet+8,12);
        if(!l->key_ready||!cv_encrypt(l->key,nonce,packet,HEADER,packet+HEADER,size,packet+HEADER+size))return false;
    }
    size_t length=HEADER+size+(encrypted?TAG:0);
    if(remember){memcpy(l->cached,packet,length);l->cached_size=length;l->attempts=1;l->last_send=l->now;}
    return l->send(l->user,type==HELLO?broadcast:l->peer,packet,length);
}
static bool agree(cv_link_t *l)
{
    uint8_t transcript[218],*p=transcript,hash[32];
    memcpy(p,l->receiver?l->peer:l->own,6);p+=6;memcpy(p,l->receiver?l->own:l->peer,6);p+=6;
    memcpy(p,l->receiver?l->remote_public:l->crypto.public_key,65);p+=65;
    memcpy(p,l->receiver?l->crypto.public_key:l->remote_public,65);p+=65;
    memcpy(p,l->receiver?l->remote_nonce:l->local_nonce,16);p+=16;
    memcpy(p,l->receiver?l->local_nonce:l->remote_nonce,16);p+=16;
    put32(p,l->total);p+=4;memcpy(p,l->digest,32);p+=32;memcpy(p,l->session,8);p+=8;
    if(!cv_sha256(transcript,(size_t)(p-transcript),hash)||!cv_crypto_agree(&l->crypto,l->remote_public,hash,l->key))return false;
    uint8_t code[32];if(!cv_hmac(l->key,"display-code",12,code))return false;
    l->code=get32(code)%1000000;l->key_ready=true;return true;
}
static void answer(cv_link_t *l,bool remember)
{
    uint8_t bytes[81];memcpy(bytes,l->crypto.public_key,65);memcpy(bytes+65,l->local_nonce,16);
    emit(l,ANSWER,bytes,sizeof(bytes),false,remember);
}
bool cv_link_open(cv_link_t *l,bool receiver,const uint8_t own[6],cv_send_t send,cv_save_t save,void *user,uint64_t now)
{
    memset(l,0,sizeof(*l));l->receiver=receiver;memcpy(l->own,own,6);l->send=send;l->save=save;l->user=user;l->now=now;
    if(!cv_random_bytes(l->epoch,8))return false;
    l->phase=receiver?L_ADVERTISE:L_DISCOVER;l->started=now;return true;
}
void cv_link_close(cv_link_t *l)
{
    cv_crypto_close(&l->crypto);free(l->incoming);memset(l,0,sizeof(*l));
}
bool cv_link_offer(cv_link_t *l,unsigned peer,const uint8_t *data,size_t size)
{
    if(l->phase!=L_DISCOVER||peer>=l->peer_count||size<4||size>CV_MAX_JPEG||l->now-l->peers[peer].seen>4000)return false;
    if(!cv_crypto_open(&l->crypto)||!cv_random_bytes(l->session,8)||!cv_random_bytes(l->local_nonce,16)||!cv_sha256(data,size,l->digest)) {
        failed(l,CV_AUTH);return false;
    }
    l->total=(uint32_t)size;l->outgoing=data;memcpy(l->peer,l->peers[peer].mac,6);
    uint8_t payload[125];memcpy(payload,l->crypto.public_key,65);memcpy(payload+65,l->local_nonce,16);
    put32(payload+81,l->total);memcpy(payload+85,l->digest,32);
    uint64_t epoch=l->peers[peer].epoch;for(int i=7;i>=0;i--){payload[117+i]=(uint8_t)epoch;epoch>>=8;}
    l->phase=L_PAIRING;l->started=l->now;emit(l,OFFER,payload,sizeof(payload),false,true);return true;
}
void cv_link_confirm(cv_link_t *l)
{
    if(l->phase!=L_CONFIRM&&l->phase!=L_WAIT)return;
    l->confirmed=true;l->phase=L_WAIT;l->started=l->now;
    emit(l,CONFIRM,NULL,0,true,true);
}
void cv_link_cancel(cv_link_t *l)
{
    if(l->key_ready&&l->phase!=L_DONE)emit(l,CANCEL,NULL,0,true,false);
    if(l->phase!=L_DONE)failed(l,CV_CANCELLED);
}
static void send_next(cv_link_t *l)
{
    if(l->offset==l->total) {l->finishing=true;emit(l,FINISH,NULL,0,true,true);return;}
    uint8_t payload[CV_CHUNK+4];put32(payload,l->offset);
    size_t n=l->total-l->offset;if(n>CV_CHUNK)n=CV_CHUNK;
    memcpy(payload+4,l->outgoing+l->offset,n);emit(l,DATA,payload,n+4,true,true);
}
static void acknowledge(cv_link_t *l){uint8_t data[4];put32(data,l->offset);emit(l,ACK,data,4,true,false);}
void cv_link_receive(cv_link_t *l,const uint8_t source[6],const uint8_t *packet,size_t size)
{
    if(l->phase==L_OFF||l->phase==L_FAILED||size<HEADER||size>CV_PACKET||memcmp(packet,"SCV1",4)||packet[6]||packet[7])return;
    unsigned type=packet[4];bool encrypted=packet[5]==1;
    if(packet[5]>1)return;
    if(type==HELLO&&!encrypted&&size==HEADER&&l->phase==L_DISCOVER) {
        unsigned i=0;for(;i<l->peer_count;i++)if(!memcmp(l->peers[i].mac,source,6))break;
        if(i==l->peer_count){if(i==CV_PEERS)return;l->peer_count++;}
        memcpy(l->peers[i].mac,source,6);l->peers[i].epoch=get64(packet+8);l->peers[i].seen=l->now;return;
    }
    if(type==OFFER&&!encrypted&&size==HEADER+125&&l->receiver) {
        if(l->phase!=L_ADVERTISE) {
            if(!memcmp(source,l->peer,6)&&!memcmp(packet+8,l->session,8))answer(l,false);
            return;
        }
        const uint8_t *p=packet+HEADER;uint32_t total=get32(p+81);
        if(memcmp(p+117,l->epoch,8)||total<4||total>CV_MAX_JPEG)return;
        if(!cv_crypto_open(&l->crypto)||!cv_random_bytes(l->local_nonce,16)){failed(l,CV_AUTH);return;}
        memcpy(l->session,packet+8,8);memcpy(l->peer,source,6);memcpy(l->remote_public,p,65);memcpy(l->remote_nonce,p+65,16);
        l->total=total;memcpy(l->digest,p+85,32);
        if(!agree(l)){failed(l,CV_AUTH);return;}
        l->phase=L_CONFIRM;l->started=l->now;answer(l,true);return;
    }
    if(memcmp(source,l->peer,6)||memcmp(packet+8,l->session,8))return;
    if(type==ANSWER&&!encrypted&&!l->receiver&&size==HEADER+81&&l->phase==L_PAIRING) {
        memcpy(l->remote_public,packet+HEADER,65);memcpy(l->remote_nonce,packet+HEADER+65,16);
        if(!agree(l)){failed(l,CV_AUTH);return;}
        l->phase=L_CONFIRM;l->cached_size=0;l->started=l->now;return;
    }
    if(!encrypted||!l->key_ready||size<HEADER+TAG)return;
    uint32_t sequence=get32(packet+16);if(!sequence||((sequence>>31)!=(!l->receiver)))return;
    uint8_t data[CV_PACKET],nonce[12];size_t length=size-HEADER-TAG;
    memcpy(data,packet+HEADER,length);memcpy(nonce,packet+8,12);
    if(!cv_decrypt(l->key,nonce,packet,HEADER,data,length,packet+size-TAG))return;
    if(type==CANCEL&&length==0){if(l->phase!=L_DONE)failed(l,CV_CANCELLED);return;}
    if(type==ERROR_PACKET&&length==1&&data[0]>CV_OK&&data[0]<CV_ERROR_COUNT){failed(l,(cv_error_t)data[0]);return;}
    if(type==CONFIRM&&length==0) {
        l->remote_confirmed=true;
        if(l->confirmed&&!l->receiver&&l->phase==L_WAIT){l->phase=L_SEND;l->started=l->now;send_next(l);}
        return;
    }
    if(type==DATA&&l->receiver&&l->confirmed&&length>=5&&(l->phase==L_WAIT||l->phase==L_RECEIVE||l->phase==L_DONE)) {
        uint32_t offset=get32(data);size_t count=length-4;
        if(count>CV_CHUNK||offset>l->total||count>l->total-offset)return;
        if(l->phase==L_DONE){acknowledge(l);return;}
        if(!l->incoming){l->incoming=malloc(l->total);if(!l->incoming){uint8_t error=CV_MEMORY;emit(l,ERROR_PACKET,&error,1,true,false);failed(l,CV_MEMORY);return;}}
        l->phase=L_RECEIVE;l->remote_confirmed=true;l->cached_size=0;
        if(offset==l->offset){memcpy(l->incoming+offset,data+4,count);l->offset+=(uint32_t)count;l->started=l->now;}
        acknowledge(l);return;
    }
    if(type==ACK&&length==4&&!l->receiver&&l->phase==L_SEND&&!l->finishing) {
        uint32_t expected=l->offset+((l->total-l->offset)>CV_CHUNK?CV_CHUNK:l->total-l->offset);
        if(get32(data)!=expected)return;
        l->offset=expected;l->started=l->now;send_next(l);return;
    }
    if(type==FINISH&&length==0&&l->receiver) {
        if(l->phase==L_DONE){emit(l,DONE,NULL,0,true,true);return;}
        if(l->phase!=L_RECEIVE||l->offset!=l->total)return;
        uint8_t digest[32];cv_error_t error=CV_CORRUPT;
        if(cv_sha256(l->incoming,l->total,digest)&&cv_secure_equal(digest,l->digest,32))error=l->save(l->user,l->incoming,l->total,l->saved);
        if(error!=CV_OK){uint8_t e=(uint8_t)error;emit(l,ERROR_PACKET,&e,1,true,false);failed(l,error);return;}
        l->phase=L_DONE;l->cached_size=0;l->started=l->now;emit(l,DONE,NULL,0,true,true);return;
    }
    if(type==DONE&&length==0&&!l->receiver&&l->phase==L_SEND&&l->finishing) {
        l->phase=L_DONE;l->cached_size=0;l->started=l->now;acknowledge(l);return;
    }
    if(type==ACK&&length==4&&l->receiver&&l->phase==L_DONE&&get32(data)==l->total)l->cached_size=0;
}
void cv_link_tick(cv_link_t *l,uint64_t now)
{
    l->now=now;
    if(l->phase==L_ADVERTISE&&now-l->last_hello>=650){emit(l,HELLO,NULL,0,false,false);l->last_hello=now;}
    if(l->phase==L_DISCOVER)for(unsigned i=0;i<l->peer_count;) {
        if(now-l->peers[i].seen>4000){memmove(&l->peers[i],&l->peers[i+1],(--l->peer_count-i)*sizeof(l->peers[0]));}
        else i++;
    }
    if(l->phase==L_WAIT&&!l->receiver&&l->confirmed&&l->remote_confirmed){l->phase=L_SEND;l->started=now;send_next(l);}
    if(l->phase>=L_PAIRING&&l->phase<L_DONE&&now-l->started>60000){failed(l,l->finishing?CV_UNKNOWN:CV_TIMEOUT);return;}
    if(l->cached_size&&now-l->last_send>=350) {
        if(l->phase==L_DONE&&now-l->started>5000){l->cached_size=0;return;}
        if(l->attempts>=100){failed(l,l->finishing?CV_UNKNOWN:CV_TIMEOUT);return;}
        l->send(l->user,l->peer,l->cached,l->cached_size);l->last_send=now;l->attempts++;
    }
}
