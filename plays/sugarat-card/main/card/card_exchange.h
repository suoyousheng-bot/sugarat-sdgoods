/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Explicit peer selection and bilateral, code-confirmed encrypted card transfer. */
#pragma once
#include "card_language.h"
#include "card_data.h"
#include "card_crypto.h"
#define CARD_PEERS_MAX 6
#define CARD_PACKET_MAX 250
#define CARD_CHUNK 160
typedef enum {CARD_EX_OFF,CARD_EX_DISCOVER,CARD_EX_VERIFY,CARD_EX_TRANSFER,CARD_EX_DONE,CARD_EX_ERROR} card_exchange_phase_t;
typedef struct {
    uint8_t mac[6],id[16],public_key[65],nonce[16];
    char name[49];uint64_t seen;
} card_peer_t;
typedef bool (*card_send_fn)(void *context,const uint8_t mac[6],const uint8_t *bytes,size_t size);
typedef card_result_t (*card_receive_fn)(void *context,const uint8_t id[16],const card_profile_t *profile);
typedef struct {
    card_exchange_phase_t phase;
    card_crypto_t crypto;
    card_peer_t self,peers[CARD_PEERS_MAX],peer;
    unsigned count,code,generation;
    uint8_t tx_key[32],rx_key[32],tx_digest[32],rx_digest[32];
    uint64_t tx_sequence,rx_sequence,rx_window,deadline,next_hello,next_send;
    char outgoing[CARD_JSON_MAX],incoming[CARD_JSON_MAX];
    size_t outgoing_size,incoming_size;
    uint8_t received[(CARD_JSON_MAX+CARD_CHUNK-1)/CARD_CHUNK];
    unsigned chunk;
    bool local_confirmed,peer_confirmed,saved,acknowledged;
    card_message_t notice;
    card_send_fn send;card_receive_fn receive;void *context;
} card_exchange_t;
/** Open one exchange window using a saved identity and validated nonempty profile. */
bool card_exchange_open(card_exchange_t *exchange,const card_state_t *state,card_send_fn send,card_receive_fn receive,void *context,uint64_t now);
/** Forget transient keys and pending transfers. Saved contacts belong to the store owner. */
void card_exchange_close(card_exchange_t *exchange);
/** Select a discovered peer and derive a verification code, without sending profile fields. */
bool card_exchange_select(card_exchange_t *exchange,unsigned index,uint64_t now);
/** Authorize only the displayed session. Transfer also requires the peer's authenticated confirmation. */
void card_exchange_confirm(card_exchange_t *exchange);
/** Parse one untrusted datagram on the application owner thread. */
void card_exchange_receive(card_exchange_t *exchange,const uint8_t mac[6],const uint8_t *bytes,size_t size,uint64_t now);
/** Perform bounded retries, discovery expiry and a finite exchange deadline. */
void card_exchange_tick(card_exchange_t *exchange,uint64_t now);
