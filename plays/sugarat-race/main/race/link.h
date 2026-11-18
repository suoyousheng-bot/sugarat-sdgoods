/* SPDX-License-Identifier: Apache-2.0 */
/** Two nearby badges pin fresh peer identities; control messages repeat in every state packet. */
#pragma once
#include "game.h"
#include <stddef.h>
#define RACE_PACKET_SIZE 64
typedef void (*race_send_fn)(void *owner,const uint8_t mac[6],const uint8_t *bytes,size_t size);
typedef struct {
    bool enabled,paired,host,ready,peer_ready,armed,committed,peer_ack,lost,paused,peer_paused;
    /* conceded: this race ended because the peer reached the finish first; sent so the peer never reads it as a finish. */
    bool conceded,peer_conceded;
    uint8_t mac[6],peer[6];
    uint32_t nonce,peer_nonce,sequence,peer_sequence,last_rx,last_tx,host_deadline,local_deadline;
    int mode,map;
    rr_state remote;
    race_phase remote_phase;
    race_send_fn send;
    void *owner;
} race_link;
/** Nonce must be fresh for each lobby; transport and entropy belong to the firmware adapter. */
void race_link_begin(race_link *link,int mode,int map,const uint8_t mac[6],uint32_t nonce,race_send_fn send,void *owner);
/** Validate and apply one fixed-size untrusted datagram. */
bool race_link_receive(race_link *link,const uint8_t mac[6],const uint8_t *bytes,size_t size,uint32_t now);
/** True when the peer reached the finish itself; a peer that conceded did not. */
bool race_link_peer_finished(const race_link *link);
/** Place of the local player, 1 or 2: finishing first wins, equal finish times go to the host; before that, distance decides racing. */
int race_link_rank(const race_link *link,const race_game *game);
/** Send at most one state packet and update connection freshness. */
void race_link_tick(race_link *link,const race_game *game,uint32_t now);
