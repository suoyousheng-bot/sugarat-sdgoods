/* SPDX-License-Identifier: Apache-2.0 */
#include "link.h"
#include <math.h>
#include <string.h>
enum{READY=1,START=2,COMMIT=4,PAUSED=8,ACK_START=16,CONCEDED=32};
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(uint8_t *p,uint32_t v){for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t checksum(const uint8_t *p){uint32_t h=2166136261U;for(int i=0;i<60;i++){h^=p[i];h*=16777619U;}return h;}
void race_link_begin(race_link *l,int mode,int map,const uint8_t mac[6],uint32_t nonce,race_send_fn send,void *owner){
    memset(l,0,sizeof *l);l->enabled=true;l->mode=mode;l->map=map;l->nonce=nonce?nonce:1;l->send=send;l->owner=owner;
    memcpy(l->mac,mac,6);l->remote.mode=mode;l->remote.map=map;
}
bool race_link_receive(race_link *l,const uint8_t mac[6],const uint8_t *p,size_t n,uint32_t now){
    if(!l->enabled||n!=64||memcmp(p,"SGR1",4)||p[4]!=3||p[5]!=l->mode||p[6]!=l->map||
       p[7]&~63||get32(p+60)!=checksum(p)||p[45]>RACE_FINISHED||p[44]>7)return false;
    uint32_t nonce=get32(p+8),target=get32(p+12),seq=get32(p+16);
    if(!nonce||(target&&target!=l->nonce)||!memcmp(mac,l->mac,6))return false;
    int32_t distance=(int32_t)get32(p+32),lateral=(int32_t)get32(p+36);
    unsigned x=p[40]|p[41]<<8,y=p[42]|p[43]<<8;
    if(distance<0||distance>(int32_t)(rr_track_length(l->map)*RACE_LAPS*1000)+1000||lateral< -5000||lateral>5000||x>=360||y>=360||get32(p+28)>86400000)return false;
    if(l->paired){if(memcmp(mac,l->peer,6)||nonce!=l->peer_nonce||(int32_t)(seq-l->peer_sequence)<=0)return false;}
    else{memcpy(l->peer,mac,6);l->peer_nonce=nonce;l->paired=true;l->host=memcmp(l->mac,mac,6)<0;}
    l->peer_sequence=seq;l->last_rx=now;l->lost=false;l->peer_ready=(p[7]&READY)!=0;l->peer_paused=(p[7]&PAUSED)!=0;l->peer_conceded=(p[7]&CONCEDED)!=0;
    l->remote.distance_m=distance*.001f;l->remote.lateral_m=lateral*.001f;l->remote.marble_x=(int)x;l->remote.marble_y=(int)y;
    l->remote.elapsed_ms=(int)get32(p+28);l->remote.checkpoint_mask=p[44];l->remote_phase=(race_phase)p[45];
    if(!l->host&&(p[7]&START)&&target==l->nonce&&get32(p+24)){
        uint32_t deadline=get32(p+24);int32_t remaining=(int32_t)(deadline-get32(p+20));
        if(remaining>5000||remaining< -86400000)return false;
        if(!l->armed){l->host_deadline=deadline;l->local_deadline=now+(uint32_t)remaining;l->armed=true;}
        if(deadline==l->host_deadline&&(p[7]&COMMIT))l->committed=true;
    }
    if(l->host&&l->armed&&(p[7]&ACK_START)&&get32(p+24)==l->host_deadline&&target==l->nonce){l->peer_ack=true;l->committed=true;}
    return true;
}
void race_link_tick(race_link *l,const race_game *g,uint32_t now){
    if(!l->enabled)return;
    if(l->paired&&now-l->last_rx>1500)l->lost=true;
    if(l->paired&&l->host&&l->ready&&l->peer_ready&&!l->armed&&!l->lost){l->armed=true;l->host_deadline=now+4000;l->local_deadline=l->host_deadline;}
    if(l->armed&&!l->committed&&(int32_t)(now-l->local_deadline)>0)l->lost=true;
    if(l->last_tx&&now-l->last_tx<66)return;
    l->last_tx=now;
    uint8_t p[64]={0};memcpy(p,"SGR1",4);p[4]=3;p[5]=(uint8_t)l->mode;p[6]=(uint8_t)l->map;
    p[7]=(l->ready?READY:0)|(l->armed?START:0)|(l->committed?COMMIT:0)|(l->paused?PAUSED:0)|(!l->host&&l->armed?ACK_START:0)|(l->conceded?CONCEDED:0);
    put32(p+8,l->nonce);put32(p+12,l->peer_nonce);put32(p+16,++l->sequence);put32(p+20,now);put32(p+24,l->host_deadline);
    put32(p+28,(uint32_t)g->frame.elapsed_ms);put32(p+32,(uint32_t)lroundf(g->frame.distance_m*1000));put32(p+36,(uint32_t)(int32_t)lroundf(g->frame.lateral_m*1000));
    p[40]=(uint8_t)g->frame.marble_x;p[41]=(uint8_t)(g->frame.marble_x>>8);p[42]=(uint8_t)g->frame.marble_y;p[43]=(uint8_t)(g->frame.marble_y>>8);p[44]=(uint8_t)g->checkpoint_mask;p[45]=(uint8_t)g->phase;
    put32(p+60,checksum(p));const uint8_t broadcast[6]={255,255,255,255,255,255};l->send(l->owner,l->paired?l->peer:broadcast,p,sizeof p);
}
bool race_link_peer_finished(const race_link *l){return l->paired&&l->remote_phase==RACE_FINISHED&&!l->peer_conceded;}
int race_link_rank(const race_link *l,const race_game *g){
    if(!l->paired)return 1;
    if(l->conceded)return 2;
    bool peer=race_link_peer_finished(l);
    if(g->phase==RACE_FINISHED){
        int mine=g->frame.elapsed_ms,theirs=l->remote.elapsed_ms;
        return peer&&(theirs<mine||(theirs==mine&&!l->host))?2:1;
    }
    if(peer)return 2;
    if(l->remote_phase!=RACE_FINISHED&&g->frame.mode==0&&l->remote.distance_m>g->frame.distance_m)return 2;
    return 1;
}
