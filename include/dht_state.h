// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DHT_STATE_H
#define DHT_STATE_H

#include <stddef.h>

#include "net_platform.h"

#define DHT_STATE_NODE_ID_LEN 20
#define DHT_STATE_MAX_NODES 256

typedef struct {
    unsigned char node_id[DHT_STATE_NODE_ID_LEN];
    int has_node_id;
    struct sockaddr_in nodes4[DHT_STATE_MAX_NODES];
    int nodes4_len;
    struct sockaddr_in6 nodes6[DHT_STATE_MAX_NODES];
    int nodes6_len;
} dht_state_t;

int dht_state_default_directory(char *buf, size_t buflen);
int dht_state_set_directory(const char *directory);
int dht_state_directory(char *buf, size_t buflen);
int dht_state_load(dht_state_t *state);
int dht_state_save(const dht_state_t *state);

#endif
