// SPDX-License-Identifier: GPL-3.0-or-later

#include "infohash.h"

#include <argon2.h>
#include <openssl/evp.h>
#include <string.h>

#define KDF_SALT "easytier:dht:v1"
#define KDF_T_COST 3
#define KDF_M_COST 65536
#define KDF_P 1

int secret_hash_derive(uint8_t out[SECRET_HASH_LEN], const char *secret)
{
    if(out == NULL || secret == NULL)
        return -1;
    return argon2id_hash_raw(KDF_T_COST, KDF_M_COST, KDF_P,
                             secret, strlen(secret),
                             KDF_SALT, (size_t)strlen(KDF_SALT),
                             out, SECRET_HASH_LEN) == ARGON2_OK ? 0 : -1;
}

static int hex_value(char c)
{
    if(c >= '0' && c <= '9')
        return c - '0';
    if(c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if(c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

int secret_hash_parse(uint8_t out[SECRET_HASH_LEN], const char *hex)
{
    size_t i;

    if(out == NULL || hex == NULL || strlen(hex) != SECRET_HASH_LEN * 2)
        return -1;
    for(i = 0; i < SECRET_HASH_LEN; i++) {
        int high = hex_value(hex[i * 2]);
        int low = hex_value(hex[i * 2 + 1]);
        if(high < 0 || low < 0)
            return -1;
        out[i] = (uint8_t)((high << 4) | low);
    }
    return 0;
}

int infohash_derive_from_secret_hash(infohash_t *out,
                                     const uint8_t secret_hash[SECRET_HASH_LEN],
                                     const char *network_name,
                                     const char *scheme)
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    EVP_MD_CTX *ctx;

    if(out == NULL || secret_hash == NULL || network_name == NULL || scheme == NULL)
        return -1;

    ctx = EVP_MD_CTX_new();
    if(ctx == NULL)
        return -1;
    if(EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 ||
       EVP_DigestUpdate(ctx, secret_hash, SECRET_HASH_LEN) != 1 ||
       EVP_DigestUpdate(ctx, network_name, strlen(network_name)) != 1 ||
       EVP_DigestUpdate(ctx, scheme, strlen(scheme)) != 1 ||
       EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return -1;
    }
    EVP_MD_CTX_free(ctx);

    if(digest_len < INFOHASH_LEN)
        return -1;
    memcpy(out->hash, digest, INFOHASH_LEN);
    return 0;
}

int infohash_derive(infohash_t *out, const char *secret, const char *network_name,
                    const char *scheme)
{
    uint8_t secret_hash[SECRET_HASH_LEN];

    if(secret_hash_derive(secret_hash, secret) != 0)
        return -1;
    return infohash_derive_from_secret_hash(out, secret_hash, network_name, scheme);
}
