/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "pet_crypto.h"
#include <stdlib.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_random.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/md.h"
#include "mbedtls/sha256.h"
#include "mbedtls/gcm.h"
#include "mbedtls/hkdf.h"
static int entropy(void *context, unsigned char *out, size_t size)
{
    (void)context; esp_fill_random(out, size); return 0;
}
bool pet_random_bytes(void *out, size_t size) { esp_fill_random(out, size); return true; }
bool pet_sha256(const void *data, size_t size, uint8_t out[32]) { return mbedtls_sha256(data, size, out, 0) == 0; }
bool pet_hmac(const uint8_t key[32], const void *data, size_t size, uint8_t out[32])
{
    return mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), key, 32, data, size, out) == 0;
}
bool pet_crypto_open(pet_crypto_t *c)
{
    memset(c, 0, sizeof(*c));
    mbedtls_ecp_keypair *key = calloc(1, sizeof(*key));
    if (!key) return false;
    mbedtls_ecp_keypair_init(key); c->implementation = key;
    size_t size = 0;
    if (mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, key, entropy, NULL) != 0 ||
        mbedtls_ecp_write_public_key(key, MBEDTLS_ECP_PF_UNCOMPRESSED, &size, c->public_key, 65) != 0 || size != 65) {
        pet_crypto_close(c); return false;
    }
    return true;
}
bool pet_crypto_agree(pet_crypto_t *c, const uint8_t peer[65], const uint8_t transcript[32], uint8_t out[32])
{
    mbedtls_ecp_keypair *key = c->implementation;
    mbedtls_ecp_point point; mbedtls_ecp_point_init(&point);
    mbedtls_ecp_group group; mbedtls_ecp_group_init(&group);
    mbedtls_mpi private; mbedtls_mpi_init(&private);
    mbedtls_mpi secret; mbedtls_mpi_init(&secret);
    uint8_t bytes[32] = {0};
    const uint8_t info[] = "sugarat-pet-v2";
    bool ok = key && mbedtls_ecp_export(key, &group, &private, NULL) == 0 && mbedtls_ecp_point_read_binary(&group, &point, peer, 65) == 0 &&
        mbedtls_ecp_check_pubkey(&group, &point) == 0 &&
        mbedtls_ecdh_compute_shared(&group, &secret, &point, &private, entropy, NULL) == 0 &&
        mbedtls_mpi_write_binary(&secret, bytes, 32) == 0 &&
        mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), transcript, 32, bytes, 32, info, sizeof(info) - 1, out, 32) == 0;
    mbedtls_mpi_free(&private); mbedtls_ecp_group_free(&group);
    memset(bytes, 0, sizeof(bytes)); mbedtls_mpi_free(&secret); mbedtls_ecp_point_free(&point);
    return ok;
}
void pet_crypto_close(pet_crypto_t *c)
{
    if (c->implementation) { mbedtls_ecp_keypair_free(c->implementation); free(c->implementation); }
    memset(c, 0, sizeof(*c));
}
bool pet_encrypt(const uint8_t key[32], const uint8_t nonce[12], const void *header, size_t header_size, uint8_t *data, size_t size, uint8_t tag[16])
{
    mbedtls_gcm_context ctx; mbedtls_gcm_init(&ctx);
    bool ok = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 256) == 0 &&
        mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT, size, nonce, 12, header, header_size, data, data, 16, tag) == 0;
    mbedtls_gcm_free(&ctx); return ok;
}
bool pet_decrypt(const uint8_t key[32], const uint8_t nonce[12], const void *header, size_t header_size, uint8_t *data, size_t size, const uint8_t tag[16])
{
    uint8_t plain[250];
    if (size > sizeof(plain)) return false;
    mbedtls_gcm_context ctx; mbedtls_gcm_init(&ctx);
    bool ok = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 256) == 0 &&
        mbedtls_gcm_auth_decrypt(&ctx, size, nonce, 12, header, header_size, tag, 16, data, plain) == 0;
    if (ok) memcpy(data, plain, size);
    memset(plain, 0, sizeof(plain));
    mbedtls_gcm_free(&ctx); return ok;
}
#else
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/rand.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
bool pet_random_bytes(void *out, size_t size) { return size <= INT32_MAX && RAND_bytes(out, (int)size) == 1; }
bool pet_sha256(const void *data, size_t size, uint8_t out[32])
{
    unsigned length = 0; return EVP_Digest(data, size, out, &length, EVP_sha256(), NULL) == 1 && length == 32;
}
bool pet_hmac(const uint8_t key[32], const void *data, size_t size, uint8_t out[32])
{
    unsigned length = 0; return HMAC(EVP_sha256(), key, 32, data, size, out, &length) != NULL && length == 32;
}
bool pet_crypto_open(pet_crypto_t *c)
{
    memset(c, 0, sizeof(*c));
    EVP_PKEY *key = EVP_EC_gen("prime256v1");
    size_t size = 0;
    c->implementation = key;
    if (!key || EVP_PKEY_get_octet_string_param(key, OSSL_PKEY_PARAM_PUB_KEY, c->public_key, 65, &size) != 1 || size != 65) {
        pet_crypto_close(c); return false;
    }
    return true;
}
bool pet_crypto_agree(pet_crypto_t *c, const uint8_t peer[65], const uint8_t transcript[32], uint8_t out[32])
{
    char group[] = "prime256v1";
    OSSL_PARAM params[] = {OSSL_PARAM_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, group, 0),
        OSSL_PARAM_octet_string(OSSL_PKEY_PARAM_PUB_KEY, (void *)peer, 65), OSSL_PARAM_END};
    EVP_PKEY_CTX *decode = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
    EVP_PKEY *remote = NULL; EVP_PKEY_CTX *derive = NULL;
    uint8_t bytes[32] = {0}; size_t size = sizeof(bytes);
    bool ok = decode && EVP_PKEY_fromdata_init(decode) == 1 && EVP_PKEY_fromdata(decode, &remote, EVP_PKEY_PUBLIC_KEY, params) == 1;
    if (ok) { derive = EVP_PKEY_CTX_new(c->implementation, NULL); ok = derive && EVP_PKEY_derive_init(derive) == 1 &&
        EVP_PKEY_derive_set_peer(derive, remote) == 1 && EVP_PKEY_derive(derive, bytes, &size) == 1 && size == 32; }
    EVP_PKEY_CTX *kdf = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL); size_t output_size = 32;
    const uint8_t info[] = "sugarat-pet-v2";
    ok = ok && kdf && EVP_PKEY_derive_init(kdf) == 1 && EVP_PKEY_CTX_set_hkdf_md(kdf, EVP_sha256()) == 1 &&
        EVP_PKEY_CTX_set1_hkdf_salt(kdf, transcript, 32) == 1 && EVP_PKEY_CTX_set1_hkdf_key(kdf, bytes, 32) == 1 &&
        EVP_PKEY_CTX_add1_hkdf_info(kdf, info, sizeof(info) - 1) == 1 && EVP_PKEY_derive(kdf, out, &output_size) == 1 && output_size == 32;
    EVP_PKEY_CTX_free(kdf);
    memset(bytes, 0, sizeof(bytes)); EVP_PKEY_CTX_free(derive); EVP_PKEY_free(remote); EVP_PKEY_CTX_free(decode);
    return ok;
}
void pet_crypto_close(pet_crypto_t *c) { EVP_PKEY_free(c->implementation); memset(c, 0, sizeof(*c)); }
static bool cipher(bool encrypt, const uint8_t key[32], const uint8_t nonce[12], const void *header, size_t header_size, uint8_t *data, size_t size, uint8_t tag[16])
{
    if (header_size > INT32_MAX || size > INT32_MAX) return false;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new(); int count = 0, tail = 0;
    bool ok = ctx && EVP_CipherInit_ex(ctx, EVP_aes_256_gcm(), NULL, key, nonce, encrypt) == 1 &&
        EVP_CipherUpdate(ctx, NULL, &count, header, (int)header_size) == 1 &&
        EVP_CipherUpdate(ctx, data, &count, data, (int)size) == 1;
    if (ok && !encrypt) ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag) == 1;
    if (ok) ok = EVP_CipherFinal_ex(ctx, data + count, &tail) == 1;
    if (ok && encrypt) ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag) == 1;
    EVP_CIPHER_CTX_free(ctx); return ok;
}
bool pet_encrypt(const uint8_t key[32], const uint8_t nonce[12], const void *header, size_t header_size, uint8_t *data, size_t size, uint8_t tag[16])
{ return cipher(true, key, nonce, header, header_size, data, size, tag); }
bool pet_decrypt(const uint8_t key[32], const uint8_t nonce[12], const void *header, size_t header_size, uint8_t *data, size_t size, const uint8_t tag[16])
{ uint8_t copy[16]; memcpy(copy, tag, 16); return cipher(false, key, nonce, header, header_size, data, size, copy); }
#endif
bool pet_secure_equal(const uint8_t *a, const uint8_t *b, size_t size)
{
    volatile uint8_t difference = 0;
    for (size_t i = 0; i < size; i++) difference |= a[i] ^ b[i];
    return difference == 0;
}
