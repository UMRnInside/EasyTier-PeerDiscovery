// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef INFOHASH_H
#define INFOHASH_H

#include <stdint.h>
#include <stddef.h>

#define INFOHASH_LEN 20

typedef struct {
    uint8_t hash[INFOHASH_LEN];
} infohash_t;

int infohash_derive(infohash_t *out, const char *secret, const char *network_name,
                    const char *scheme);

#endif