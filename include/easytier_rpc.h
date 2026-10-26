// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef EASYTIER_RPC_H
#define EASYTIER_RPC_H

#include <stddef.h>

typedef struct {
    char tcp_nat[32];
    char udp_nat[32];
    char public_ipv4[64];
    char public_ipv6[64];
    unsigned short tcp4_port;
    unsigned short tcp6_port;
    unsigned short udp4_port;
    unsigned short udp6_port;
    unsigned short quic4_port;
    unsigned short quic6_port;
} easytier_network_status_t;

int easytier_rpc_init(const char *rpc_host, unsigned short rpc_port,
                      const char *cli_path);
int easytier_rpc_get_network_status(easytier_network_status_t *status);
int easytier_ipv4_nat_eligible(const char *type, int assume_good);
void easytier_rpc_set_verbose(int verbose);
int easytier_rpc_cleanup(void);

#endif
