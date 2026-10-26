// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ARGS_H
#define ARGS_H

#include <stddef.h>
#include <stdio.h>

#define PEERDISC_VERSION "0.1.0"

#define DEFAULT_ANNOUNCE_INTERVAL 900u
#define MIN_ANNOUNCE_INTERVAL 300u
#define DEFAULT_HTTP_PORT 8080u
#define DEFAULT_RPC_PORT 15888u

typedef struct {
    char *network_name;
    char *secret;
    char *secret_hash;
    char *http_host;
    unsigned short http_port;
    unsigned short dht_port;
    char **bootstrap;
    size_t bootstrap_len;
    unsigned announce_interval;
    int no_announce;
    int ipv6;
    int quic;
    char *rpc_host;
    unsigned short rpc_port;
    int verbose;
} peerdisc_args_t;

int args_parse(int argc, char **argv, peerdisc_args_t *out);
void args_print_usage(FILE *out);
void args_free(peerdisc_args_t *a);

#endif
