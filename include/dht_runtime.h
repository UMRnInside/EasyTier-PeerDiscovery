// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DHT_RUNTIME_H
#define DHT_RUNTIME_H

#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <sys/socket.h>

#include "dht.h"

typedef void (*dht_runtime_hook_t)(void *closure);

int dht_runtime_init(unsigned short dht_port, int ipv6, int verbose);
int dht_runtime_add_bootstrap(const char *host, unsigned short port);
int dht_runtime_search(const unsigned char *id, int port, int af,
                       dht_callback_t *cb, void *closure);
int dht_runtime_run(int http_fd, dht_callback_t *cb, void *cb_closure,
                    dht_runtime_hook_t hook, void *hook_closure);
int dht_runtime_shutdown(void);
void dht_runtime_request_stop(void);

int dht_sendto(int sockfd, const void *buf, int len, int flags,
               const struct sockaddr *to, int tolen);
int dht_blacklisted(const struct sockaddr *sa, int salen);
void dht_hash(void *hash_return, int hash_size,
              const void *v1, int len1,
              const void *v2, int len2,
              const void *v3, int len3);
int dht_random_bytes(void *buf, size_t size);

#endif
