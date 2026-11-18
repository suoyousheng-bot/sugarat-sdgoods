/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Native family and crypto behavior using real OpenSSL and the pinned care simulation. */
#include "pet_family.h"
#include "pet_crypto.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static pet_id_t identity(uint8_t n) { pet_id_t id = {{0}}; id.bytes[0] = n; return id; }
static void checksum(uint8_t *data)
{
    uint32_t value = 2166136261u;
    for (size_t i = 0; i < PET_FAMILY_BYTES - 4; i++) value = (value ^ data[i]) * 16777619u;
    for (unsigned i = 0; i < 4; i++) data[PET_FAMILY_BYTES - 4 + i] = (uint8_t)(value >> (8 * i));
}
static void crypto_tests(void)
{
    uint8_t digest[32]; assert(pet_sha256("abc", 3, digest));
    const uint8_t expected[] = {0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
    assert(!memcmp(digest, expected, 32));
    pet_crypto_t a, b; assert(pet_crypto_open(&a) && pet_crypto_open(&b));
    uint8_t ka[32], kb[32], bad[65] = {0};
    assert(pet_crypto_agree(&a, b.public_key, digest, ka) && pet_crypto_agree(&b, a.public_key, digest, kb));
    assert(pet_secure_equal(ka, kb, 32));
    assert(!pet_crypto_agree(&a, bad, digest, kb));
    uint8_t tag[32]; assert(pet_hmac(ka, "message", 7, tag)); tag[3] ^= 1; assert(!pet_secure_equal(tag, ka, 32));
    uint8_t nonce[12]={0}, cipher[4]={1,2,3,4}, original[]={1,2,3,4}, auth[16];
    assert(pet_encrypt(ka,nonce,"aad",3,cipher,4,auth)&&memcmp(cipher,original,4));
    uint8_t encrypted[4];memcpy(encrypted,cipher,4);
    assert(!pet_decrypt(ka,nonce,"bad",3,cipher,4,auth));memcpy(cipher,encrypted,4);
    assert(pet_decrypt(ka,nonce,"aad",3,cipher,4,auth)&&!memcmp(cipher,original,4));
    nonce[0]=1;assert(pet_encrypt(ka,nonce,"aad",3,cipher,0,auth));assert(pet_decrypt(ka,nonce,"aad",3,cipher,0,auth));
    pet_crypto_close(&a); pet_crypto_close(&b);
    puts("PASS: SHA-256 vector, real P-256 agreement, invalid peer-key rejection and authentication");
}
static void family_tests(void)
{
    pet_model_t model; pet_model_init(&model, 0);
    pet_family_t f; pet_family_init(&f, &model, identity(1));
    model.pet.age_ms = 900000; model.pet.energy = 80; pet_family_capture(&f, &model);
    pet_profile_t peer = {.id = {{2}}, .blood = {1,1}, .generation = 1, .age_ms = 900000};
    pet_friend_t *friend = pet_family_friend(&f, &peer, true); assert(friend); friend->interactions = 7;
    assert(pet_family_can_breed(&f, &peer));
    pet_profile_t parents[] = {f.members[0].profile, peer}, reversed[] = {peer, f.members[0].profile}, children[2], same[2];
    uint8_t seed[32] = {4}; assert(pet_genetics(parents, seed, children)&&pet_genetics(reversed, seed, same));
    assert(!memcmp(children, same, sizeof(children)));
    assert(pet_profile_kind(&children[0]) == PET_SPROUT && children[0].generation == 2);
    assert(!pet_id_equal(children[0].id, children[1].id) && memcmp(children[0].traits, children[1].traits, PET_TRAITS_COUNT));
    parents[0].age_ms+=123456;assert(pet_genetics(parents,seed,same)&&!memcmp(children,same,sizeof(children)));
    for(unsigned sample=0;sample<256;sample++) {
        seed[0]=(uint8_t)sample;assert(pet_genetics(parents,seed,same));
        assert(!pet_id_equal(same[0].id,same[1].id));
        if(pet_profile_kind(&same[0])==pet_profile_kind(&same[1]))
            assert(same[0].traits[PET_BODY]%3!=same[1].traits[PET_BODY]%3 || same[0].traits[PET_COLOR]%4!=same[1].traits[PET_COLOR]%4 || same[0].traits[PET_PATTERN]%3!=same[1].traits[PET_PATTERN]%3);
    }
    pet_profile_t relative=peer;relative.parents[0]=f.members[0].profile.id;relative.parents[1]=identity(6);
    assert(!pet_family_can_breed(&f,&relative));
    assert(!pet_family_can_breed(&f,&f.members[0].profile));
    f.pending = (pet_pending_t){.id = {{9}}, .peer = peer.id, .phase = PET_TX_COMMIT, .coordinator = true};
    memcpy(f.pending.children, children, sizeof(children)); assert(pet_family_apply_commit(&f));
    assert(f.has_egg && f.receipt_count == 1 && f.members[0].breed_rest_ms == PET_BREED_REST_MS);
    assert(pet_family_apply_commit(&f) && f.receipt_count == 1);
    uint8_t *record = malloc(PET_FAMILY_BYTES); assert(record);
    assert(pet_family_encode(&f, 1, record)); pet_family_t restored; uint32_t sequence;
    assert(pet_family_decode(&restored, &sequence, record, PET_FAMILY_BYTES) == PET_FAMILY_OK && sequence == 1);
    assert(restored.has_egg && restored.pending.phase == PET_TX_APPLIED);
    record[32] = 255; checksum(record);
    assert(pet_family_decode(&restored, &sequence, record, PET_FAMILY_BYTES) == PET_FAMILY_CORRUPT && restored.count == 1);
    assert(pet_family_encode(&f, 2, record)); record[4] = 3; checksum(record);
    assert(pet_family_decode(&restored, &sequence, record, PET_FAMILY_BYTES) == PET_FAMILY_VERSION);
    memset(&f.pending, 0, sizeof(f.pending));
    assert(!pet_family_hatch(&f, &model)); f.tending_egg = true;
    pet_family_tick(&f, PET_EGG_TIME_MS); assert(pet_family_hatch(&f, &model));
    assert(f.count == 2 && f.active == 1 && !f.has_egg && model.pet.age_ms == 0);
    float hunger = model.pet.hunger;
    assert(pet_family_select(&f, &model, 0) && model.pet.age_ms == 900000);
    assert(pet_family_select(&f, &model, 1) && model.pet.hunger == hunger);
    assert(pet_family_encode(&f,3,record));pet_model_t fresh;pet_model_init(&fresh,0);fresh.pet.hunger=99;
    assert(pet_family_decode(&restored,&sequence,record,PET_FAMILY_BYTES)==PET_FAMILY_OK);pet_family_restore(&restored,&fresh);
    assert(fresh.pet.hunger==hunger&&restored.active==1);
    free(record);
    puts("PASS: parent-order symmetry, hybrid siblings, durable records, replay receipts, egg progress and paused family members");
}
int main(void)
{
    pet_species_register(); crypto_tests(); family_tests(); return 0;
}
