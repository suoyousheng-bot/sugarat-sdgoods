/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Wire validation, independent IMA blocks and deterministic two-peer PTT arbitration. */
#include "talk_core.h"
#include <string.h>

static const uint8_t broadcast[6]={255,255,255,255,255,255};
static const int steps[89]={7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767};
static const int indices[8]={-1,-1,-1,-1,2,4,6,8};
static int clamp(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(uint8_t *p,uint32_t n){for(int i=0;i<4;i++)p[i]=(uint8_t)(n>>(i*8));}
static int expand(int nibble,int *predictor,int *index)
{
    int step=steps[*index],delta=step/8;
    if(nibble&1)delta+=step/4;
    if(nibble&2)delta+=step/2;
    if(nibble&4)delta+=step;
    *predictor=clamp(*predictor+((nibble&8)?-delta:delta),-32768,32767);
    *index=clamp(*index+indices[nibble&7],0,88);
    return *predictor;
}
void talk_audio_encode(uint8_t *out,const int16_t *pcm)
{
    int predictor=pcm[0],index=40;
    out[0]=(uint8_t)predictor;out[1]=(uint8_t)((uint16_t)predictor>>8);out[2]=(uint8_t)index;out[3]=0;
    memset(out+4,0,TALK_SAMPLES/2);
    for(unsigned i=0;i<TALK_SAMPLES;i++){
        int delta=pcm[i]-predictor,step=steps[index],nibble=0;
        if(delta<0){nibble=8;delta=-delta;}
        if(delta>=step){nibble|=4;delta-=step;}
        if(delta>=step/2){nibble|=2;delta-=step/2;}
        if(delta>=step/4)nibble|=1;
        expand(nibble,&predictor,&index);
        out[4+i/2]|=(uint8_t)(nibble<<((i%2)*4));
    }
}
void talk_audio_decode(int16_t *out,const uint8_t *data)
{
    int predictor=(int16_t)((uint16_t)data[0]|(uint16_t)data[1]<<8),index=data[2];
    for(unsigned i=0;i<TALK_SAMPLES;i++)out[i]=(int16_t)expand((data[4+i/2]>>((i%2)*4))&15,&predictor,&index);
}
bool talk_packet_read(talk_packet_t *p,const uint8_t *d,size_t n)
{
    if(n<TALK_HEADER||n>TALK_PACKET_MAX||memcmp(d,"SQTK",4)||d[4]!=1||d[5]<1||d[5]>TALK_CHANNELS||d[7]!=0)return false;
    if(d[6]<TALK_HELLO||d[6]>TALK_END||d[20]!=n-TALK_HEADER||d[21]||d[22]||d[23])return false;
    size_t size=n-TALK_HEADER;talk_kind_t kind=(talk_kind_t)d[6];
    if(kind==TALK_AUDIO){if(size!=TALK_AUDIO_BYTES||d[TALK_HEADER+2]>88||d[TALK_HEADER+3])return false;}
    else if(kind==TALK_HELLO){if(size!=1||d[TALK_HEADER]>1)return false;}
    else if(size)return false;
    *p=(talk_packet_t){.channel=d[5],.kind=kind,.boot=get32(d+8),.session=get32(d+12),.sequence=get32(d+16),.payload=d+TALK_HEADER,.size=size};
    return p->boot!=0&&(kind==TALK_HELLO||p->session!=0);
}
static bool send_packet(talk_core_t *c,talk_kind_t kind,uint32_t session,const uint8_t *payload,size_t size)
{
    uint8_t d[TALK_PACKET_MAX]={0};memcpy(d,"SQTK",4);d[4]=1;d[5]=c->channel;d[6]=(uint8_t)kind;
    put32(d+8,c->boot);put32(d+12,session);put32(d+16,kind==TALK_AUDIO?c->next_sequence++:0);d[20]=(uint8_t)size;
    if(size)memcpy(d+TALK_HEADER,payload,size);
    bool ok=c->send(kind==TALK_HELLO?broadcast:c->peer,d,TALK_HEADER+size,c->user);
    if(!ok)c->transport_failed=true;
    return ok;
}
static void idle(talk_core_t *c)
{
    c->phase=c->paused?TALK_PAUSED:(c->paired&&c->peer_available?TALK_READY:TALK_WAITING);
    c->remote_session=0;c->have_sequence=false;
}
static void end(talk_core_t *c)
{
    if(c->phase==TALK_TRANSMITTING||c->phase==TALK_REQUESTING)send_packet(c,TALK_END,c->session,NULL,0);
    if(c->phase==TALK_RECEIVING)send_packet(c,TALK_END,c->remote_session,NULL,0);
    idle(c);
}
void talk_core_init(talk_core_t *c,const uint8_t mac[6],uint32_t boot,talk_send_fn send,talk_play_fn play,void *user)
{
    memset(c,0,sizeof(*c));memcpy(c->mac,mac,6);c->boot=boot?boot:1;c->channel=1;c->send=send;c->play=play;c->user=user;
}
void talk_core_tick(talk_core_t *c,uint64_t now)
{
    c->now=now;
    if(c->paired&&now-c->peer_seen>3000){end(c);c->paired=false;c->peer_available=false;idle(c);}
    if(c->phase==TALK_REQUESTING){
        if(now-c->started>900){end(c);c->transport_failed=true;}
        else if(now-c->request_at>=100){c->request_at=now;send_packet(c,TALK_REQUEST,c->session,NULL,0);}
    }
    if(c->phase==TALK_TRANSMITTING&&now-c->started>30000)end(c);
    if(c->phase==TALK_RECEIVING&&now-c->remote_audio>600)end(c);
    if(!c->hello_at||now-c->hello_at>=500){
        uint8_t available=c->paused?0:1;c->hello_at=now?now:1;send_packet(c,TALK_HELLO,0,&available,1);
    }
}
void talk_core_hold(talk_core_t *c,bool held)
{
    if(c->held==held)return;
    c->held=held;
    if(!held){if(c->phase==TALK_REQUESTING||c->phase==TALK_TRANSMITTING)end(c);return;}
    if(c->phase!=TALK_READY)return;
    c->session++;if(!c->session)c->session++;
    c->phase=TALK_REQUESTING;c->started=c->request_at=c->now;c->transport_failed=false;
    send_packet(c,TALK_REQUEST,c->session,NULL,0);
}
void talk_core_pause(talk_core_t *c,bool paused)
{
    if(c->paused==paused)return;
    end(c);c->held=false;c->paused=paused;idle(c);c->hello_at=0;
}
void talk_core_channel(talk_core_t *c,uint8_t channel)
{
    if(channel<1||channel>TALK_CHANNELS||channel==c->channel)return;
    end(c);c->held=false;c->paired=false;c->peer_available=false;c->channel=channel;c->hello_at=0;idle(c);
}
void talk_core_receive(talk_core_t *c,const uint8_t source[6],const uint8_t *d,size_t n)
{
    talk_packet_t p;
    if(!talk_packet_read(&p,d,n)){c->rejected++;return;}
    if(p.channel!=c->channel||!memcmp(source,c->mac,6)||(source[0]&1))return;
    if(p.kind==TALK_HELLO){
        if(c->paired&&memcmp(source,c->peer,6))return;
        if(c->paired&&c->peer_boot!=p.boot){end(c);c->paired=false;}
        memcpy(c->peer,source,6);c->paired=true;c->peer_boot=p.boot;c->peer_seen=c->now;c->peer_available=p.payload[0]!=0;
        if(!c->peer_available)end(c);
        if(c->phase==TALK_WAITING||c->phase==TALK_READY)idle(c);
        return;
    }
    if(!c->paired||memcmp(source,c->peer,6)||p.boot!=c->peer_boot)return;
    c->peer_seen=c->now;
    if(c->paused||!c->peer_available)return;
    if(p.kind==TALK_REQUEST){
        if(c->phase==TALK_TRANSMITTING)return;
        if(c->phase==TALK_REQUESTING&&memcmp(c->mac,c->peer,6)<0)return;
        if(c->phase==TALK_RECEIVING&&p.session!=c->remote_session)return;
        if(c->phase!=TALK_RECEIVING)c->have_sequence=false;
        c->phase=TALK_RECEIVING;c->remote_session=p.session;c->remote_audio=c->now;
        send_packet(c,TALK_GRANT,p.session,NULL,0);
    }else if(p.kind==TALK_GRANT){
        if(c->phase==TALK_REQUESTING&&c->held&&p.session==c->session){c->phase=TALK_TRANSMITTING;c->started=c->now;}
    }else if(p.kind==TALK_END){
        if((c->phase==TALK_RECEIVING&&p.session==c->remote_session)||
           ((c->phase==TALK_TRANSMITTING||c->phase==TALK_REQUESTING)&&p.session==c->session))idle(c);
    }else if(p.kind==TALK_AUDIO&&c->phase==TALK_RECEIVING&&p.session==c->remote_session){
        if(c->have_sequence){
            uint32_t delta=p.sequence-c->receive_sequence;
            if(!delta||delta>100000)return;
            c->lost+=delta-1;
        }
        c->have_sequence=true;c->receive_sequence=p.sequence;c->remote_audio=c->now;c->received++;
        int16_t pcm[TALK_SAMPLES];talk_audio_decode(pcm,p.payload);c->play(pcm,TALK_SAMPLES,c->user);
    }
}
void talk_core_audio(talk_core_t *c,const int16_t *samples)
{
    if(c->phase!=TALK_TRANSMITTING||!c->held||c->paused)return;
    uint8_t block[TALK_AUDIO_BYTES];talk_audio_encode(block,samples);
    if(!send_packet(c,TALK_AUDIO,c->session,block,sizeof(block)))end(c);
}
