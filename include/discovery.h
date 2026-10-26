// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DISCOVERY_H
#define DISCOVERY_H

#include "infohash.h"

#include <stddef.h>

int discovery_init(const char *network_name, const infohash_t *tcp_topic,
                   const infohash_t *quic_topic, unsigned search_interval,
                   int ipv6);
int discovery_start(void);
int discovery_stop(void);

void discovery_dht_callback(void *closure, int event,
                            const unsigned char *info_hash, const void *data,
                            size_t data_len);
void discovery_periodic(void *closure);

#endif