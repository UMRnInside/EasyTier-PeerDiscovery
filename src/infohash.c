// SPDX-License-Identifier: GPL-3.0-or-later

#include "infohash.h"

#include <argon2.h>
#include <openssl/evp.h>
#include <string.h>

#define KDF_OUTLEN 32
#define KDF_SALT "easytier:dht:v1"
#define KDF_T_COST 3
#define KDF_M_COST 65536
#define KDF_P 1

int infohash_derive(infohash_t *out, const char *secret, const char *network_name,
                    const char *scheme)
{
    unsigned char key[KDF_OUTLEN];
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    EVP_MD_CTX *ctx;
    int rc;

    if(out == NULL || secret == NULL || network_name == NULL || scheme == NULL)
        return -1;

    rc = argon2id_hash_raw(KDF_T_COST, KDF_M_COST, KDF_P,
                           secret, strlen(secret),
                           KDF_SALT, (size_t)strlen(KDF_SALT),
                           key, KDF_OUTLEN);
    if(rc != ARGON2_OK)
        return -1;

    ctx = EVP_MD_CTX_new();
    if(ctx == NULL)
        return -1;
    if(EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 ||
       EVP_DigestUpdate(ctx, key, KDF_OUTLEN) != 1 ||
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