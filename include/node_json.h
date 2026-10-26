// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NODE_JSON_H
#define NODE_JSON_H

#include <stddef.h>

typedef struct {
    char tcp_nat[32];
    char udp_nat[32];
    char public_ipv4[64];
    char public_ipv6[64];
    char listener[128];
} node_json_t;

int node_json_parse(const char *json, size_t length, node_json_t *node);

#endif
