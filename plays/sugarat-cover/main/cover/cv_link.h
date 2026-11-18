/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Routerless, user-confirmed photo exchange; caller serializes all calls and owns its transport. */
#pragma once
#include "cv_types.h"
#include "cv_crypto.h"
#define CV_PACKET 1200
#define CV_CHUNK 1000
typedef enum { L_OFF, L_DISCOVER, L_ADVERTISE, L_PAIRING, L_CONFIRM, L_WAIT,
               L_SEND, L_RECEIVE, L_DONE, L_FAILED } cv_phase_t;
typedef struct { uint8_t mac[6]; uint64_t epoch, seen; } cv_peer_t;
typedef bool (*cv_send_t)(void *user,const uint8_t mac[6],const uint8_t *data,size_t size);
typedef cv_error_t (*cv_save_t)(void *user,const uint8_t *data,size_t size,char id[CV_ID_LENGTH+1]);
typedef struct {
    cv_phase_t phase; cv_error_t error; cv_peer_t peers[CV_PEERS]; unsigned peer_count;
    uint8_t own[6],peer[6],session[8],epoch[8],local_nonce[16],remote_nonce[16];
    cv_crypto_t crypto; uint8_t remote_public[65],key[32],digest[32];
    bool receiver,key_ready,confirmed,remote_confirmed,finishing;
    uint32_t code,sequence,offset,total; uint64_t now,started,last_send,last_hello;
    uint8_t cached[CV_PACKET];size_t cached_size;unsigned attempts;
    const uint8_t *outgoing;uint8_t *incoming;char saved[CV_ID_LENGTH+1];
    cv_send_t send;cv_save_t save;void *user;
} cv_link_t;
/** Initialize discovery or receive advertising on an already-running, common-channel radio. */
bool cv_link_open(cv_link_t *link,bool receiver,const uint8_t own[6],cv_send_t send,cv_save_t save,void *user,uint64_t now);
/** Revoke the session and wipe ephemeral keys; caller may then release outgoing bytes and radio resources. */
void cv_link_close(cv_link_t *link);
/** Offer immutable JPEG bytes to a recently discovered receiver; bytes live until close. */
bool cv_link_offer(cv_link_t *link,unsigned peer,const uint8_t *data,size_t size);
/** Confirm the displayed six-digit code after comparing both screens. */
void cv_link_confirm(cv_link_t *link);
/** Cancel the selected session; a saved image is never deleted by cancellation. */
void cv_link_cancel(cv_link_t *link);
/** Validate and process one bounded datagram from the transport's source MAC. */
void cv_link_receive(cv_link_t *link,const uint8_t source[6],const uint8_t *packet,size_t size);
/** Advance bounded retry budgets and discovery expiry using a monotonic millisecond clock. */
void cv_link_tick(cv_link_t *link,uint64_t now);
