// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NODE_JSON_H
#define NODE_JSON_H

#include <stddef.h>

typedef struct {
    char tcp_nat[32];
    char udp_nat[32];
    char public_ipv4[64];
    char public_ipv6[64];
    char tcp4_listener[128];
    char tcp6_listener[128];
    char udp4_listener[128];
    char udp6_listener[128];
    char quic4_listener[128];
    char quic6_listener[128];
} node_json_t;

int node_json_parse(const char *json, size_t length, node_json_t *node);

#endif
