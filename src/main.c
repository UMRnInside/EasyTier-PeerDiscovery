// SPDX-License-Identifier: GPL-3.0-or-later

#include "args.h"
#include "dht_runtime.h"
#include "discovery.h"
#include "easytier_rpc.h"
#include "endpoint.h"
#include "httpd.h"
#include "infohash.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct {
    const char *host;
    unsigned short port;
} g_bootstrap[] = {
    {"dht.transmissionbt.com", 6881},
    {"router.bittorrent.com", 6881},
    {"router.utorrent.com", 6881},
    {"dht.libtorrent.org", 25401},
};

static void on_signal(int sig)
{
    (void)sig;
    dht_runtime_shutdown();
}

static int run(const peerdisc_args_t *args)
{
    infohash_t tcp_topic;
    infohash_t quic_topic;
    int http_fd;
    size_t i;
    int rc;

    if(infohash_derive(&tcp_topic, args->secret, args->network_name, "tcp") != 0 ||
       infohash_derive(&quic_topic, args->secret, args->network_name, "quic") != 0) {
        fprintf(stderr, "easytier-peerdisc: failed to derive infohash\n");
        return -1;
    }

    if(endpoint_init() != 0)
        return -1;
    if(httpd_init(args->http_host, args->http_port) != 0) {
        fprintf(stderr, "easytier-peerdisc: failed to bind http on %s:%u\n",
                args->http_host, args->http_port);
        return -1;
    }
    http_fd = httpd_fd();

    if(easytier_rpc_init(args->rpc_host, args->rpc_port) != 0) {
        fprintf(stderr, "easytier-peerdisc: invalid rpc target\n");
        return -1;
    }
    if(dht_runtime_init(args->dht_port, args->ipv6) != 0) {
        fprintf(stderr, "easytier-peerdisc: failed to init dht\n");
        return -1;
    }
    if(dht_runtime_add_bootstrap(g_bootstrap[0].host, g_bootstrap[0].port) != 0)
        fprintf(stderr, "easytier-peerdisc: warning: failed to resolve bootstrap\n");
    for(i = 0; i < args->bootstrap_len; i++) {
        const char *colon;
        char host[256];
        unsigned short port = 6881;
        colon = strrchr(args->bootstrap[i], ':');
        if(colon == NULL)
            continue;
        snprintf(host, sizeof(host), "%.*s", (int)(colon - args->bootstrap[i]),
                 args->bootstrap[i]);
        port = (unsigned short)strtoul(colon + 1, NULL, 10);
        dht_runtime_add_bootstrap(host, port);
    }

    if(discovery_init(args->network_name, &tcp_topic, &quic_topic,
                      args->announce_interval, args->ipv6) != 0) {
        fprintf(stderr, "easytier-peerdisc: failed to init discovery\n");
        return -1;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    printf("easytier-peerdisc %s: http http://%s:%u/endpoint, dht port %u\n",
           PEERDISC_VERSION, args->http_host, args->http_port, args->dht_port);
    discovery_start();
    rc = dht_runtime_run(http_fd, discovery_dht_callback, NULL, discovery_periodic, NULL);

    discovery_stop();
    dht_runtime_shutdown();
    httpd_shutdown();
    endpoint_cleanup();
    return rc;
}

int main(int argc, char **argv)
{
    peerdisc_args_t args;
    int ret;

    ret = args_parse(argc, argv, &args);
    if(ret == 1)
        return EXIT_FAILURE;
    if(ret == 2)
        return EXIT_SUCCESS;

    ret = run(&args);
    args_free(&args);
    return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}