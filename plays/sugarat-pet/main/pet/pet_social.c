/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "pet_social.h"
#include <string.h>

enum { ANNOUNCE = 1, KEY, CONFIRM, STATUS, PREPARE, READY, COMMIT, DONE, FINAL, IDENTITY, PROOF };
static const uint8_t broadcast[6] = {255,255,255,255,255,255};
static void put32(uint8_t *p, uint32_t n) { for (unsigned i = 0; i < 4; i++) p[i] = (uint8_t)(n >> (8 * i)); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static bool persist(pet_social_t *s)
{
    if (s->save(s->context)) return true;
    s->state = PET_SOCIAL_FAULT; s->notice = "Save unconfirmed: reconnect after restart"; return false;
}
static bool receipt(const pet_family_t *f, pet_id_t id)
{
    for (unsigned i = 0; i < f->receipt_count; i++) if (pet_id_equal(f->receipts[i], id)) return true;
    return false;
}
static bool send_packet(pet_social_t *s, unsigned type, const uint8_t *payload, size_t size)
{
    uint8_t packet[PET_PACKET_MAX] = {0}, nonce[12];
    bool encrypted = type >= CONFIRM;
    size_t header = encrypted ? 28 : 8;
    if (header + size + (encrypted ? 16 : 0) > sizeof(packet)) return false;
    memcpy(packet, "SGP2", 4); packet[4] = 2; packet[5] = (uint8_t)type; packet[6] = (uint8_t)size;
    if (encrypted) {
        if (!s->agreed || s->tx_sequence == UINT32_MAX) { s->state = PET_SOCIAL_FAULT; s->notice = "Reopen nearby session"; return false; }
        memcpy(packet + 8, s->sid, 16); put32(packet + 24, ++s->tx_sequence);
        memcpy(nonce, s->sid, 8); memcpy(nonce + 8, packet + 24, 4);
    }
    if (size) memcpy(packet + header, payload, size);
    if (encrypted && !pet_encrypt(s->tx_key, nonce, packet, header, packet + header, size, packet + header + size)) return false;
    return s->send(s->context, type == ANNOUNCE ? broadcast : s->peer_mac, packet, header + size + (encrypted ? 16 : 0));
}
static void send_key(pet_social_t *s)
{
    uint8_t bytes[97]; memcpy(bytes, s->nonce, 16); memcpy(bytes + 16, s->peer_nonce, 16); memcpy(bytes + 32, s->crypto.public_key, 65);
    send_packet(s, KEY, bytes, sizeof(bytes));
}
bool pet_social_open(pet_social_t *s, pet_family_t *family, pet_social_send_fn send, pet_social_save_fn save, void *context, uint64_t now_ms)
{
    memset(s, 0, sizeof(*s)); s->family = family; s->send = send; s->save = save; s->context = context; s->now_ms = now_ms;
    s->self = family->members[family->active].profile;
    if (!pet_random_bytes(s->nonce, 16) || !pet_crypto_open(&s->crypto)) {
        pet_social_close(s); s->state = PET_SOCIAL_FAULT; s->notice = "Secure session unavailable"; return false;
    }
    s->state = PET_SOCIAL_SEARCH; s->notice = "Choose each other on both devices"; return true;
}
void pet_social_close(pet_social_t *s)
{
    pet_crypto_close(&s->crypto);
    memset(s->tx_key, 0, 32); memset(s->rx_key, 0, 32); memset(s->bond_key, 0, 32);
    s->state = PET_SOCIAL_OFF;
}
bool pet_social_select(pet_social_t *s, unsigned candidate)
{
    if (s->state != PET_SOCIAL_SEARCH || candidate >= s->nearby_count || s->now_ms - s->nearby[candidate].seen_ms > 5000) return false;
    memcpy(s->peer_mac, s->nearby[candidate].mac, 6); memcpy(s->peer_nonce, s->nearby[candidate].nonce, 16);
    s->selected = true; s->state = PET_SOCIAL_KEY; s->notice = "Waiting for the other device"; send_key(s); return true;
}
bool pet_social_confirm(pet_social_t *s)
{
    if (s->state != PET_SOCIAL_VERIFY) return false;
    s->confirmed = true; s->transmit_ms = 0; return true;
}
static void connect(pet_social_t *s)
{
    if (!s->confirmed || !s->peer_confirmed || s->state == PET_SOCIAL_LINKED) return;
    if (pet_id_equal(s->self.id, s->peer.id)) { s->state = PET_SOCIAL_FAULT; s->notice = "Cannot pair with the same pet"; return; }
    if (s->family->pending.phase != PET_TX_NONE && !pet_id_equal(s->family->pending.peer, s->peer.id)) {
        s->state = PET_SOCIAL_FAULT; s->notice = "Pending: choose the original friend"; return;
    }
    pet_friend_t *f = pet_family_friend(s->family, &s->peer, true);
    if (!f) { s->state = PET_SOCIAL_FAULT; s->notice = "Friend album is full"; return; }
    if (!f->bonded) memcpy(f->key, s->bond_key, 32);
    f->bonded = true; f->profile = s->peer;
    if (persist(s)) { s->state = PET_SOCIAL_LINKED; s->notice = s->known_friend ? "Reconnected with your friend" : "Connected: greet, gift and play"; }
}
bool pet_social_interact(pet_social_t *s, unsigned bit)
{
    if (s->state != PET_SOCIAL_LINKED || bit > 2) return false;
    pet_friend_t *f = pet_family_friend(s->family, &s->peer, false);
    if (!f) return false;
    uint8_t offered = (uint8_t)(1u << bit);
    if (!(s->offered_interactions & offered)) { s->offered_interactions |= offered; s->transmit_ms = 0; }
    return true;
}
bool pet_social_breed(pet_social_t *s)
{
    if (s->state != PET_SOCIAL_LINKED || !pet_family_can_breed(s->family, &s->peer)) {
        s->notice = "Need adults, 3 activities, energy and room"; return false;
    }
    s->wants_egg = true; s->transmit_ms = 0; s->notice = "Waiting for both breeding requests"; return true;
}
static bool derive(pet_social_t *s, const uint8_t public_key[65])
{
    uint8_t transcript[162], hash[32], base[32], first[32], second[32];
    int order = memcmp(s->nonce, s->peer_nonce, 16); if (!order) return false;
    const uint8_t *n0 = order < 0 ? s->nonce : s->peer_nonce, *n1 = order < 0 ? s->peer_nonce : s->nonce;
    const uint8_t *p0 = order < 0 ? s->crypto.public_key : public_key, *p1 = order < 0 ? public_key : s->crypto.public_key;
    memcpy(transcript, n0, 16); memcpy(transcript + 16, p0, 65); memcpy(transcript + 81, n1, 16); memcpy(transcript + 97, p1, 65);
    bool ok = pet_sha256(transcript, sizeof(transcript), hash) && pet_crypto_agree(&s->crypto, public_key, hash, base) &&
        pet_hmac(base, "low-to-high", 11, first) && pet_hmac(base, "high-to-low", 11, second) && pet_hmac(base, "friend", 6, s->bond_key);
    if (ok) {
        memcpy(s->sid, hash, 16); memcpy(s->tx_key, order < 0 ? first : second, 32); memcpy(s->rx_key, order < 0 ? second : first, 32);
        s->code = get32(base) % 1000000u; memcpy(s->peer_public, public_key, 65); s->agreed = true;
        s->state = PET_SOCIAL_VERIFY; s->notice = "Compare code, then confirm on both";
    }
    memset(base, 0, sizeof(base)); memset(first, 0, sizeof(first)); memset(second, 0, sizeof(second)); return ok;
}
static bool child_plan_valid(pet_social_t *s, pet_profile_t children[2])
{
    const pet_profile_t *a = &s->self, *b = &s->peer;
    if (pet_id_compare(a->id, b->id) > 0) { a = &s->peer; b = &s->self; }
    for (unsigned i = 0; i < 2; i++) {
        pet_profile_t *c = &children[i];
        if (!pet_id_equal(c->parents[0], a->id) || !pet_id_equal(c->parents[1], b->id) || c->age_ms ||
            c->generation != (unsigned)(a->generation > b->generation ? a->generation : b->generation) + 1u ||
            (c->blood[0] != a->blood[0] && c->blood[0] != a->blood[1]) || (c->blood[1] != b->blood[0] && c->blood[1] != b->blood[1])) return false;
        if (pet_id_equal(c->id, a->id) || pet_id_equal(c->id, b->id)) return false;
    }
    return !pet_id_equal(children[0].id, children[1].id);
}
static bool plan_hash(const pet_profile_t children[2], uint8_t hash[32])
{
    uint8_t encoded[136]; pet_profile_encode(&children[0], encoded); pet_profile_encode(&children[1], encoded + 68);
    return pet_sha256(encoded, sizeof(encoded), hash);
}
static bool apply(pet_social_t *s)
{
    pet_pending_t *p = &s->family->pending;
    p->phase = PET_TX_COMMIT;
    if (!persist(s)) return false;
    if (!pet_family_apply_commit(s->family)) { s->state = PET_SOCIAL_FAULT; s->notice = "Family conflict: keep save for recovery"; return false; }
    if (!persist(s)) return false;
    s->notice = "Egg saved: finishing with your friend"; return true;
}
static void transaction_message(pet_social_t *s, unsigned type, const uint8_t *bytes, size_t size)
{
    if (s->state != PET_SOCIAL_LINKED || size < 16) return;
    pet_family_t *f = s->family; pet_pending_t *p = &f->pending; pet_id_t id; memcpy(id.bytes, bytes, 16);
    if (pet_id_empty(id)) return;
    if (type == PREPARE && size == 80) {
        if (receipt(f, id)) { send_packet(s, DONE, id.bytes, 16); return; }
        if (p->phase != PET_TX_NONE) {
            if (pet_id_equal(p->id, id) && pet_id_equal(p->peer, s->peer.id) && !p->coordinator) send_packet(s, READY, id.bytes, 16);
            return;
        }
        if (!s->wants_egg || !pet_family_can_breed(f, &s->peer) || pet_id_compare(s->self.id, s->peer.id) < 0) return;
        pet_profile_t children[2], parents[] = {s->self, s->peer}; uint8_t hash[32];
        if (!pet_genetics(parents, bytes + 16, children) || !child_plan_valid(s, children) ||
            !plan_hash(children, hash) || !pet_secure_equal(hash, bytes + 48, 32)) return;
        *p = (pet_pending_t){.id = id, .peer = s->peer.id, .phase = PET_TX_PREPARED, .own_child = 1};
        memcpy(p->children, children, sizeof(children)); memcpy(p->seed, bytes + 16, 32);
        if (persist(s)) send_packet(s, READY, id.bytes, 16);
        return;
    }
    if (type == DONE && receipt(f, id) && p->phase == PET_TX_NONE) { send_packet(s, FINAL, id.bytes, 16); return; }
    if (p->phase == PET_TX_NONE || !pet_id_equal(p->id, id) || !pet_id_equal(p->peer, s->peer.id)) return;
    if (type == READY && p->coordinator && p->phase == PET_TX_PREPARED) { if (apply(s)) send_packet(s, COMMIT, id.bytes, 16); }
    else if (type == COMMIT && !p->coordinator) {
        if (p->phase == PET_TX_PREPARED || p->phase == PET_TX_COMMIT) { if (!apply(s)) return; }
        if (p->phase == PET_TX_APPLIED) send_packet(s, DONE, id.bytes, 16);
    } else if (type == DONE && p->coordinator && p->phase == PET_TX_APPLIED) {
        memset(p, 0, sizeof(*p));
        if (persist(s)) { send_packet(s, FINAL, id.bytes, 16); s->wants_egg = false; s->notice = "Eggs saved: open Family"; }
    } else if (type == FINAL && !p->coordinator && p->phase == PET_TX_APPLIED) {
        memset(p, 0, sizeof(*p));
        if (persist(s)) { s->wants_egg = false; s->notice = "Eggs saved: open Family"; }
    }
}
void pet_social_receive(pet_social_t *s, const uint8_t mac[6], const uint8_t *data, size_t size)
{
    if (s->state == PET_SOCIAL_OFF || s->state == PET_SOCIAL_FAULT || size < 8 || size > PET_PACKET_MAX ||
        memcmp(data, "SGP2", 4) || data[4] != 2 || data[7]) return;
    unsigned type = data[5]; size_t payload_size = data[6];
    if (type == ANNOUNCE && payload_size == 17 && size == 25 && data[24] < PET_KIND_COUNT) {
        if (s->state != PET_SOCIAL_SEARCH || !memcmp(data + 8, s->nonce, 16)) return;
        unsigned i = 0; for (; i < s->nearby_count; i++) if (!memcmp(s->nearby[i].mac, mac, 6)) break;
        if (i == PET_NEARBY_MAX) return;
        if (i == s->nearby_count) s->nearby_count++;
        memcpy(s->nearby[i].mac, mac, 6); memcpy(s->nearby[i].nonce, data + 8, 16);
        s->nearby[i].kind = data[24]; s->nearby[i].seen_ms = s->now_ms; return;
    }
    if (!s->selected || memcmp(mac, s->peer_mac, 6)) return;
    if (type == KEY && payload_size == 97 && size == 105) {
        if (memcmp(data + 8, s->peer_nonce, 16) || memcmp(data + 24, s->nonce, 16)) return;
        if (!s->agreed) { if (!derive(s, data + 40)) { s->state = PET_SOCIAL_FAULT; s->notice = "Invalid secure handshake"; } }
        else if (memcmp(s->peer_public, data + 40, 65)) { s->state = PET_SOCIAL_FAULT; s->notice = "Peer changed: reopen session"; }
        return;
    }
    if (type < CONFIRM || type > PROOF || !s->agreed || size != 28 + payload_size + 16 || memcmp(data + 8, s->sid, 16)) return;
    uint32_t sequence = get32(data + 24); if (!sequence) return;
    if (sequence <= s->rx_sequence && (s->rx_sequence - sequence >= 32 || (s->rx_window & (1u << (s->rx_sequence - sequence))))) return;
    uint8_t bytes[206], nonce[12]; if (payload_size > sizeof(bytes)) return;
    memcpy(bytes, data + 28, payload_size); memcpy(nonce, s->sid, 8); memcpy(nonce + 8, data + 24, 4);
    if (!pet_decrypt(s->rx_key, nonce, data, 28, bytes, payload_size, data + 28 + payload_size)) return;
    if (sequence > s->rx_sequence) { unsigned shift = sequence - s->rx_sequence; s->rx_window = shift >= 32 ? 1 : (s->rx_window << shift) | 1u; s->rx_sequence = sequence; }
    else s->rx_window |= 1u << (s->rx_sequence - sequence);
    bool was_quiet = s->received_ms && s->now_ms - s->received_ms > 5000;
    s->received_ms = s->now_ms;
    if (was_quiet && s->state == PET_SOCIAL_LINKED) s->notice = s->family->pending.phase ? "Resuming saved egg exchange" : "Connected: greet, gift and play";
    if ((type == CONFIRM || type == IDENTITY) && payload_size == 68) {
        pet_profile_t peer; if (!pet_profile_decode(&peer, bytes)) return;
        if (s->peer_identified && !pet_id_equal(peer.id, s->peer.id)) return;
        s->peer = peer; s->peer_identified = true;
        if (type == CONFIRM) s->peer_confirmed = true;
        connect(s); return;
    }
    if (type == PROOF && payload_size == 32 && s->peer_identified) {
        pet_friend_t *friend = pet_family_friend(s->family, &s->peer, false); uint8_t expected[32];
        if (friend && friend->bonded && pet_hmac(friend->key, s->sid, 16, expected) && pet_secure_equal(expected, bytes, 32)) {
            s->known_friend = true; s->confirmed = s->peer_confirmed = true; connect(s);
        }
        return;
    }
    if (s->state != PET_SOCIAL_LINKED) return;
    if (type == STATUS && payload_size == 2 && bytes[0] <= 7 && bytes[1] <= 1) {
        pet_friend_t *friend = pet_family_friend(s->family, &s->peer, false);
        if (!friend) return;
        uint8_t merged = friend->interactions | bytes[0];
        if (merged != friend->interactions) { friend->interactions = merged; if (!persist(s)) return; }
        s->peer_wants_egg = bytes[1] != 0;
    } else transaction_message(s, type, bytes, payload_size);
}
void pet_social_tick(pet_social_t *s, uint64_t now_ms)
{
    s->now_ms = now_ms;
    if (s->state == PET_SOCIAL_OFF || s->state == PET_SOCIAL_FAULT) return;
    if (now_ms - s->broadcast_ms >= 500) {
        s->broadcast_ms = now_ms; uint8_t bytes[17]; memcpy(bytes, s->nonce, 16); bytes[16] = pet_profile_kind(&s->self);
        send_packet(s, ANNOUNCE, bytes, sizeof(bytes));
    }
    if (!s->selected || (s->transmit_ms && now_ms < s->transmit_ms)) return;
    s->transmit_ms = now_ms + 425u + (s->nonce[s->tx_sequence % 16] + s->tx_sequence * 37u) % 151u;
    if (!s->peer_confirmed) send_key(s);
    if (s->agreed) {
        uint8_t identity[68]; pet_profile_encode(&s->self, identity); send_packet(s, IDENTITY, identity, sizeof(identity));
        pet_friend_t *friend = s->peer_identified ? pet_family_friend(s->family, &s->peer, false) : NULL;
        uint8_t proof[32];
        if (friend && friend->bonded && pet_hmac(friend->key, s->sid, 16, proof)) send_packet(s, PROOF, proof, sizeof(proof));
    }
    if (s->agreed && s->confirmed) { uint8_t bytes[68]; pet_profile_encode(&s->self, bytes); send_packet(s, CONFIRM, bytes, sizeof(bytes)); connect(s); }
    if (s->state != PET_SOCIAL_LINKED) return;
    if (s->received_ms && now_ms - s->received_ms > 5000) s->notice = s->family->pending.phase ? "Pending: meet the same friend again" : "Connection quiet: waiting to reconnect";
    pet_friend_t *friend = pet_family_friend(s->family, &s->peer, false);
    if (!friend) return;
    uint8_t status[] = {friend->interactions | s->offered_interactions, s->wants_egg}; send_packet(s, STATUS, status, sizeof(status));
    pet_pending_t *p = &s->family->pending;
    if (p->phase == PET_TX_NONE && s->wants_egg && s->peer_wants_egg && pet_id_compare(s->self.id, s->peer.id) < 0 && pet_family_can_breed(s->family, &s->peer)) {
        uint8_t seed[32]; pet_id_t id; pet_profile_t parents[] = {s->self, s->peer}, children[2];
        if (!pet_random_bytes(id.bytes, 16) || pet_id_empty(id) || !pet_hmac(s->bond_key, id.bytes, 16, seed)) { s->state = PET_SOCIAL_FAULT; s->notice = "Random source unavailable"; return; }
        if (!pet_genetics(parents, seed, children) || !child_plan_valid(s, children)) { s->state = PET_SOCIAL_FAULT; s->notice = "Offspring plan failed"; return; }
        *p = (pet_pending_t){.id = id, .peer = s->peer.id, .phase = PET_TX_PREPARED, .coordinator = true}; memcpy(p->children, children, sizeof(children));
        memcpy(p->seed, seed, 32);
        if (!persist(s)) return;
    }
    if (p->phase == PET_TX_NONE || !pet_id_equal(p->peer, s->peer.id)) return;
    s->notice = p->phase == PET_TX_PREPARED ? "Saved: waiting for your friend" : "Egg saved: finishing sync";
    if (p->phase == PET_TX_COMMIT) { if (!apply(s)) return; }
    if (p->phase == PET_TX_PREPARED && p->coordinator) {
        uint8_t plan[80]; memcpy(plan, p->id.bytes, 16); memcpy(plan + 16, p->seed, 32);
        if (!plan_hash(p->children, plan + 48)) { s->state = PET_SOCIAL_FAULT; s->notice = "Offspring digest unavailable"; return; }
        send_packet(s, PREPARE, plan, sizeof(plan));
    } else send_packet(s, p->coordinator ? COMMIT : p->phase == PET_TX_PREPARED ? READY : DONE, p->id.bytes, 16);
}
