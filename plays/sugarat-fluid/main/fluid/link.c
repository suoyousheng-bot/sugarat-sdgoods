/* SPDX-License-Identifier: Apache-2.0 */
/** Host-authoritative, acknowledgement-paced liquid transfer over lossy datagrams. */
#include "link.h"
#include <math.h>
#include <string.h>
static const uint8_t broadcast[6] = {255,255,255,255,255,255};
static void put32(uint8_t *p, uint32_t n) { for (unsigned i = 0; i < 4; i++) p[i] = (uint8_t)(n >> (8*i)); }
static uint32_t get32(const uint8_t *p) { uint32_t n = 0; for (unsigned i = 0; i < 4; i++) n |= (uint32_t)p[i] << (8*i); return n; }
void bottle_packet_encode(const bottle_packet_t *p, uint8_t b[BOTTLE_PACKET_SIZE])
{
    memset(b, 0, BOTTLE_PACKET_SIZE); memcpy(b, "SFB1", 4);
    b[4] = p->type; b[5] = p->input.pose; b[6] = p->input.rate;
    put32(b+8,p->room); put32(b+12,p->sender); put32(b+16,p->target);
    put32(b+20,p->sequence); put32(b+24,p->acknowledgement);
    for (unsigned j=0;j<2;j++) for(unsigned i=0;i<4;i++) {
        unsigned at=28+j*8+i*2; b[at]=(uint8_t)p->liquid[j].part[i]; b[at+1]=(uint8_t)(p->liquid[j].part[i]>>8);
    }
}
bool bottle_packet_decode(bottle_packet_t *p, const uint8_t *b, size_t size)
{
    if (size != BOTTLE_PACKET_SIZE || memcmp(b,"SFB1",4) || b[7] ||
        b[4] < BOTTLE_HELLO || b[4] > BOTTLE_LEAVE || b[5] > BOTTLE_POUR ||
        b[6] > 6 || (b[5] == BOTTLE_POUR ? b[6] == 0 : b[6] != 0)) return false;
    memset(p,0,sizeof(*p)); p->type=b[4]; p->input.pose=b[5]; p->input.rate=b[6];
    p->room=get32(b+8); p->sender=get32(b+12); p->target=get32(b+16);
    p->sequence=get32(b+20); p->acknowledgement=get32(b+24);
    if (!p->room || !p->sender || (p->type != BOTTLE_HELLO && !p->target)) return false;
    for(unsigned j=0;j<2;j++) {
        for(unsigned i=0;i<4;i++) { unsigned at=28+j*8+i*2; p->liquid[j].part[i]=(uint16_t)(b[at]|((uint16_t)b[at+1]<<8)); }
        if (!bottle_liquid_valid(&p->liquid[j])) return false;
    }
    return true;
}
unsigned bottle_link_local_index(const bottle_link_t *l) { return l->host ? 0 : 1; }
void bottle_link_solo(bottle_link_t *l, unsigned ingredient, unsigned units)
{
    memset(l,0,sizeof(*l)); l->phase=BOTTLE_SOLO;
    bottle_liquid_fill(&l->liquid[1],units,ingredient);
}
void bottle_link_host(bottle_link_t *l, uint32_t id, uint32_t now)
{
    bottle_liquid_t own=l->liquid[bottle_link_local_index(l)];
    memset(l,0,sizeof(*l)); l->host=true; l->self=id; l->room=id;
    l->liquid[0]=own; l->phase=BOTTLE_HOSTING; l->began_ms=now; l->rearm=true;
}
void bottle_link_scan(bottle_link_t *l, uint32_t id, uint32_t now)
{
    uint32_t ignored=l->phase==BOTTLE_FOUND?l->room:0;
    bottle_liquid_t own=l->liquid[bottle_link_local_index(l)];
    memset(l,0,sizeof(*l)); l->self=id; l->liquid[1]=own;
    l->phase=BOTTLE_SCANNING; l->ignored_room=ignored; l->began_ms=now; l->rearm=true;
}
void bottle_link_join(bottle_link_t *l, uint32_t now)
{
    if(l->phase!=BOTTLE_FOUND) return;
    l->phase=BOTTLE_JOINING; l->began_ms=now; l->last_sent_ms=now-BOTTLE_LINK_INTERVAL_MS;
}
void bottle_link_accept(bottle_link_t *l, uint32_t now)
{
    if(l->phase!=BOTTLE_INVITED) return;
    l->phase=BOTTLE_LINKED; l->revision=1; l->last_received_ms=now;
    l->last_sent_ms=now-BOTTLE_LINK_INTERVAL_MS; l->last_step_ms=now; l->rearm=true;
}
bottle_input_t bottle_input_from_gravity(float x,float y,float z,bool available)
{
    bottle_input_t input={BOTTLE_CLOSED,0};
    float length=sqrtf(x*x+y*y+z*z);
    if(!available || !isfinite(length) || length<1.0f) return input;
    y/=length;
    if(y>0.65f) input.pose=BOTTLE_RECEIVE;
    else if(y< -0.25f) { input.pose=BOTTLE_POUR; input.rate=(uint8_t)(1+fminf(5,(-y-0.25f)*7)); }
    return input;
}
void bottle_link_input(bottle_link_t *l,bottle_input_t input)
{
    if(l->phase!=BOTTLE_LINKED) { l->local=(bottle_input_t){BOTTLE_CLOSED,0}; l->rearm=true; return; }
    if(input.pose==BOTTLE_RECEIVE) l->rearm=false;
    l->local=l->rearm?(bottle_input_t){BOTTLE_CLOSED,0}:input;
}
static bool session_peer(const bottle_link_t *l,const bottle_packet_t *p,const uint8_t mac[6])
{
    return p->room==l->room && p->sender==l->peer && p->target==l->self && !memcmp(mac,l->peer_mac,6);
}
void bottle_link_receive(bottle_link_t *l,const uint8_t mac[6],const uint8_t *bytes,size_t size,uint32_t now)
{
    bottle_packet_t p;
    if(!bottle_packet_decode(&p,bytes,size)) return;
    if(p.type==BOTTLE_HELLO && (l->phase==BOTTLE_SCANNING || l->phase==BOTTLE_FOUND)) {
        if(p.room==l->ignored_room || (l->phase==BOTTLE_FOUND && p.room!=l->room)) return;
        l->room=p.room; l->peer=p.sender; l->liquid[0]=p.liquid[0]; memcpy(l->peer_mac,mac,6);
        l->phase=BOTTLE_FOUND; l->last_received_ms=now; return;
    }
    if(p.type==BOTTLE_JOIN && l->phase==BOTTLE_HOSTING && p.room==l->room && p.target==l->self && p.sender!=l->self) {
        l->peer=p.sender; memcpy(l->peer_mac,mac,6); l->liquid[1]=p.liquid[1];
        l->phase=BOTTLE_INVITED; l->began_ms=now; return;
    }
    if(!session_peer(l,&p,mac)) return;
    if(p.type==BOTTLE_LEAVE) {
        l->phase=BOTTLE_LOST; l->flow=0; l->local=(bottle_input_t){0}; l->remote=l->local; l->rearm=true;
        return;
    }
    bool connected=l->phase==BOTTLE_LINKED || l->phase==BOTTLE_LOST;
    if(p.type==BOTTLE_INPUT && l->host && connected) {
        if(!p.sequence || p.sequence<=l->peer_sequence || p.acknowledgement>l->revision || p.acknowledgement<l->acknowledged) return;
        l->peer_sequence=p.sequence; l->acknowledged=p.acknowledgement; l->remote=p.input;
        l->last_received_ms=now;
        if(l->phase==BOTTLE_LOST) { l->phase=BOTTLE_LINKED; l->rearm=true; l->local=(bottle_input_t){0}; }
    } else if(p.type==BOTTLE_STATE && !l->host && (connected || l->phase==BOTTLE_JOINING)) {
        if(!p.sequence || p.sequence<l->revision || !p.acknowledgement || p.acknowledgement<=l->peer_sequence) return;
        if(l->phase==BOTTLE_JOINING && (p.sequence!=1 || memcmp(p.liquid,l->liquid,sizeof(l->liquid)))) return;
        if(connected) for(unsigned i=0;i<BOTTLE_INGREDIENTS;i++)
            if(p.liquid[0].part[i]+p.liquid[1].part[i]!=l->liquid[0].part[i]+l->liquid[1].part[i]) return;
        if(p.sequence==l->revision && memcmp(p.liquid,l->liquid,sizeof(l->liquid))) return;
        if(l->phase!=BOTTLE_LINKED) { l->rearm=true; l->local=(bottle_input_t){0}; }
        if(p.sequence>l->revision && l->revision) l->last_transfer_ms=now;
        l->phase=BOTTLE_LINKED; l->peer_sequence=p.acknowledgement; l->revision=p.sequence; memcpy(l->liquid,p.liquid,sizeof(l->liquid));
        l->remote=p.input; l->last_received_ms=now;
    }
}
static void emit(bottle_link_t *l,bottle_message_t type,bottle_send_fn send,void *context)
{
    bottle_packet_t p={.type=type,.room=l->room,.sender=l->self,.target=l->peer,
        .sequence=l->host?l->revision:++l->input_sequence,.acknowledgement=l->host?++l->input_sequence:l->revision,.input=l->local};
    memcpy(p.liquid,l->liquid,sizeof(p.liquid));
    uint8_t bytes[BOTTLE_PACKET_SIZE]; bottle_packet_encode(&p,bytes);
    send(context,type==BOTTLE_HELLO?broadcast:l->peer_mac,bytes,sizeof(bytes));
}
void bottle_link_leave(bottle_link_t *l,bottle_send_fn send,void *context)
{
    if(l->peer && l->room) emit(l,BOTTLE_LEAVE,send,context);
}
void bottle_link_tick(bottle_link_t *l,uint32_t now,bottle_send_fn send,void *context)
{
    l->flow=0;
    if(l->phase==BOTTLE_FOUND && now-l->last_received_ms>BOTTLE_LINK_TIMEOUT_MS) { l->phase=BOTTLE_SCANNING; l->room=0; l->peer=0; }
    if((l->phase==BOTTLE_INVITED || l->phase==BOTTLE_JOINING) && now-l->began_ms>BOTTLE_INVITE_TIMEOUT_MS) {
        l->phase=BOTTLE_LOST; l->rearm=true; l->local=(bottle_input_t){0};
    }
    if(l->phase==BOTTLE_LINKED && now-l->last_received_ms>BOTTLE_LINK_TIMEOUT_MS) {
        l->phase=BOTTLE_LOST; l->rearm=true; l->local=(bottle_input_t){0}; l->remote=l->local;
    }
    if(l->phase==BOTTLE_LINKED && l->host && now-l->last_step_ms>=BOTTLE_LINK_INTERVAL_MS) {
        l->last_step_ms=now;
        if(l->acknowledged==l->revision) {
            int source=-1;
            if(l->local.pose==BOTTLE_POUR && l->remote.pose==BOTTLE_RECEIVE) source=0;
            if(l->remote.pose==BOTTLE_POUR && l->local.pose==BOTTLE_RECEIVE) source=1;
            if(source>=0) {
                unsigned rate=source?l->remote.rate:l->local.rate;
                if(bottle_liquid_transfer(&l->liquid[source],&l->liquid[1-source],rate)) { l->revision++; l->last_transfer_ms=now; }
            }
        }
    }
    if(l->phase==BOTTLE_LINKED && l->last_transfer_ms && now-l->last_transfer_ms<250 && now-l->last_received_ms<=BOTTLE_LINK_TIMEOUT_MS) {
        if(l->local.pose==BOTTLE_POUR && l->remote.pose==BOTTLE_RECEIVE && bottle_liquid_total(&l->liquid[bottle_link_local_index(l)]) && bottle_liquid_total(&l->liquid[1-bottle_link_local_index(l)])<BOTTLE_CAPACITY) l->flow=l->host?1:-1;
        if(l->remote.pose==BOTTLE_POUR && l->local.pose==BOTTLE_RECEIVE && bottle_liquid_total(&l->liquid[1-bottle_link_local_index(l)]) && bottle_liquid_total(&l->liquid[bottle_link_local_index(l)])<BOTTLE_CAPACITY) l->flow=l->host?-1:1;
    }
    if(now-l->last_sent_ms<BOTTLE_LINK_INTERVAL_MS) return;
    l->last_sent_ms=now;
    switch(l->phase) {
        case BOTTLE_HOSTING: emit(l,BOTTLE_HELLO,send,context); break;
        case BOTTLE_JOINING: emit(l,BOTTLE_JOIN,send,context); break;
        case BOTTLE_LINKED: case BOTTLE_LOST:
            if(l->revision) emit(l,l->host?BOTTLE_STATE:BOTTLE_INPUT,send,context);
            break;
        default: break;
    }
}
