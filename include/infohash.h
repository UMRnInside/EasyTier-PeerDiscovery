// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef INFOHASH_H
#define INFOHASH_H

#include <stdint.h>
#include <stddef.h>

#define INFOHASH_LEN 20
#define SECRET_HASH_LEN 32

typedef struct {
    uint8_t hash[INFOHASH_LEN];
} infohash_t;

int infohash_derive(infohash_t *out, const char *secret, const char *network_name,
                     const char *scheme);
int secret_hash_derive(uint8_t out[SECRET_HASH_LEN], const char *secret);
int secret_hash_parse(uint8_t out[SECRET_HASH_LEN], const char *hex);
int infohash_derive_from_secret_hash(infohash_t *out,
                                     const uint8_t secret_hash[SECRET_HASH_LEN],
                                     const char *network_name,
                                     const char *scheme);

#endif
