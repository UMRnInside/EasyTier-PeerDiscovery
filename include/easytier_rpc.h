// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef EASYTIER_RPC_H
#define EASYTIER_RPC_H

#include <stddef.h>

int easytier_rpc_init(const char *rpc_host, unsigned short rpc_port);
int easytier_rpc_get_endpoint(char *buf, size_t buflen, int *nat_ok);
int easytier_rpc_cleanup(void);

#endif