// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include "dht_runtime.h"

#include "dht_state.h"
#include "httpd.h"

#include <limits.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DHT_BUFSIZE 4096
#define DHT_STATE_SAVE_INTERVAL 300
#define DHT_STATE_RESTORE_NODES 32

static net_socket_t g_socket = NET_INVALID_SOCKET;
static net_socket_t g_socket6 = NET_INVALID_SOCKET;
static unsigned char g_node_id[20];
static volatile sig_atomic_t g_stop = 0;
static time_t g_last_state_save;
static int g_verbose;

static void log_state(const char *operation, int status, const dht_state_t *state)
{
    char directory[4096];

    if(!g_verbose)
        return;
    if(dht_state_directory(directory, sizeof(directory)) != 0)
        snprintf(directory, sizeof(directory), "<unavailable>");
    printf("easytier-peerdisc: dht state %s %s: %s/dht-state.bin",
           operation, status < 0 ? "failed" : status == 0 ? "not found" : "succeeded",
           directory);
    if(status > 0)
        printf(" (ipv4 nodes %d, ipv6 nodes %d)",
               state->nodes4_len, state->nodes6_len);
    printf("\n");
}

static int save_dht_state(void)
{
    dht_state_t state;
    int rc;

    memset(&state, 0, sizeof(state));
    memcpy(state.node_id, g_node_id, sizeof(g_node_id));
    state.has_node_id = 1;
    state.nodes4_len = DHT_STATE_MAX_NODES;
    state.nodes6_len = DHT_STATE_MAX_NODES;
    dht_get_nodes(state.nodes4, &state.nodes4_len,
                  state.nodes6, &state.nodes6_len);
    rc = dht_state_save(&state);
    log_state("save", rc == 0 ? 1 : -1, &state);
    return rc;
}

static net_socket_t make_udp_socket(int family, unsigned short port, unsigned short *out_port)
{
    struct sockaddr_storage ss;
    net_socklen_t len;
    net_socket_t fd;

    fd = socket(family, SOCK_DGRAM, 0);
    if(fd == NET_INVALID_SOCKET)
        return NET_INVALID_SOCKET;
    net_set_option(fd, SOL_SOCKET, SO_REUSEADDR, 1);
    if(family == AF_INET6)
        net_set_option(fd, IPPROTO_IPV6, IPV6_V6ONLY, 1);

    memset(&ss, 0, sizeof(ss));
    if(family == AF_INET) {
        struct sockaddr_in *a = (struct sockaddr_in *)&ss;
        a->sin_family = AF_INET;
        a->sin_addr.s_addr = INADDR_ANY;
        a->sin_port = htons(port);
    } else {
        struct sockaddr_in6 *a = (struct sockaddr_in6 *)&ss;
        a->sin6_family = AF_INET6;
        a->sin6_addr = in6addr_any;
        a->sin6_port = htons(port);
    }
    if(bind(fd, (struct sockaddr *)&ss, sizeof(ss)) < 0) {
        net_close(fd);
        return NET_INVALID_SOCKET;
    }
    if(port == 0 && out_port != NULL) {
        len = sizeof(ss);
        if(getsockname(fd, (struct sockaddr *)&ss, &len) == 0) {
            if(ss.ss_family == AF_INET)
                *out_port = ntohs(((struct sockaddr_in *)&ss)->sin_port);
            else
                *out_port = ntohs(((struct sockaddr_in6 *)&ss)->sin6_port);
        }
    }
    if(net_set_nonblocking(fd) != 0) {
        net_close(fd);
        return NET_INVALID_SOCKET;
    }
    return fd;
}

int dht_runtime_init(unsigned short dht_port, int ipv6, int verbose,
                     const char *state_directory)
{
    dht_state_t state;
    unsigned short v4port = 0;
    int state_status;
    int i;
    int rc;

    g_verbose = verbose;
    g_stop = 0;
    if(dht_state_set_directory(state_directory) != 0)
        return -1;
    g_socket = make_udp_socket(AF_INET, dht_port, &v4port);
    if(g_socket == NET_INVALID_SOCKET)
        return -1;
    g_socket6 = NET_INVALID_SOCKET;
    if(ipv6) {
        g_socket6 = make_udp_socket(AF_INET6, v4port, NULL);
    }
    state_status = dht_state_load(&state);
    log_state("load", state_status, &state);
    if(state_status > 0 && state.has_node_id)
        memcpy(g_node_id, state.node_id, sizeof(g_node_id));
    else if(dht_random_bytes(g_node_id, sizeof(g_node_id)) != (int)sizeof(g_node_id))
        goto fail;

    /* The DHT library only passes these integer tokens to dht_sendto. */
    rc = dht_init(0, g_socket6 != NET_INVALID_SOCKET ? 1 : -1,
                  g_node_id, (const unsigned char *)"PD01");
    if(rc < 0)
        goto fail;
    if(state_status > 0) {
        int restore4 = state.nodes4_len < DHT_STATE_RESTORE_NODES ?
                       state.nodes4_len : DHT_STATE_RESTORE_NODES;
        int restore6 = state.nodes6_len < DHT_STATE_RESTORE_NODES ?
                       state.nodes6_len : DHT_STATE_RESTORE_NODES;

        for(i = 0; i < restore4; i++)
            dht_ping_node((struct sockaddr *)&state.nodes4[i], sizeof(state.nodes4[i]));
        if(g_socket6 != NET_INVALID_SOCKET) {
            for(i = 0; i < restore6; i++)
                dht_ping_node((struct sockaddr *)&state.nodes6[i], sizeof(state.nodes6[i]));
        }
    }
    g_last_state_save = time(NULL);
    return 0;

fail:
    net_close(g_socket);
    g_socket = NET_INVALID_SOCKET;
    if(g_socket6 != NET_INVALID_SOCKET) {
        net_close(g_socket6);
        g_socket6 = NET_INVALID_SOCKET;
    }
    return -1;
}

int dht_runtime_add_bootstrap(const char *host, unsigned short port)
{
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *ai;
    char portstr[8];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    snprintf(portstr, sizeof(portstr), "%u", port);
    if(getaddrinfo(host, portstr, &hints, &res) != 0)
        return -1;
    for(ai = res; ai != NULL; ai = ai->ai_next)
        dht_ping_node(ai->ai_addr, (int)ai->ai_addrlen);
    freeaddrinfo(res);
    return 0;
}

int dht_runtime_search(const unsigned char *id, int port, int af,
                       dht_callback_t *cb, void *closure)
{
    return dht_search(id, port, af, cb, closure);
}

static void handle_udp(net_socket_t fd, dht_callback_t *cb, void *closure)
{
    struct sockaddr_storage from;
    net_socklen_t fromlen = sizeof(from);
    char buf[DHT_BUFSIZE + 1];
    int len;
    time_t tosleep = 0;

    len = recvfrom(fd, buf, DHT_BUFSIZE, 0, (struct sockaddr *)&from, &fromlen);
    if(len < 0)
        return;
    buf[len] = '\0';
    dht_periodic(buf, (size_t)len, (struct sockaddr *)&from, (int)fromlen,
                 &tosleep, cb, closure);
}

int dht_runtime_run(net_socket_t http_fd, dht_callback_t *cb, void *cb_closure,
                    dht_runtime_hook_t hook, void *hook_closure)
{
    while(!g_stop) {
        time_t tosleep = 0;
        fd_set rfds;
        net_socket_t highest = 0;
        struct timeval tv;
        int rc;

        dht_periodic(NULL, 0, NULL, 0, &tosleep, cb, cb_closure);
        if(hook != NULL)
            hook(hook_closure);
        if(time(NULL) - g_last_state_save >= DHT_STATE_SAVE_INTERVAL) {
            save_dht_state();
            g_last_state_save = time(NULL);
        }

        FD_ZERO(&rfds);
        if(http_fd != NET_INVALID_SOCKET) {
            FD_SET(http_fd, &rfds);
            if(http_fd > highest)
                highest = http_fd;
        }
        if(g_socket != NET_INVALID_SOCKET) {
            FD_SET(g_socket, &rfds);
            if(g_socket > highest)
                highest = g_socket;
        }
        if(g_socket6 != NET_INVALID_SOCKET) {
            FD_SET(g_socket6, &rfds);
            if(g_socket6 > highest)
                highest = g_socket6;
        }

        if(tosleep < 0)
            tosleep = 0;
        if(tosleep > 3600)
            tosleep = 3600;
        tv.tv_sec = (time_t)tosleep;
        tv.tv_usec = 0;

        rc = net_select(highest, &rfds, &tv);
        if(rc < 0) {
            if(net_interrupted())
                continue;
            break;
        }
        if(http_fd != NET_INVALID_SOCKET && FD_ISSET(http_fd, &rfds))
            httpd_accept();
        if(g_socket != NET_INVALID_SOCKET && FD_ISSET(g_socket, &rfds))
            handle_udp(g_socket, cb, cb_closure);
        if(g_socket6 != NET_INVALID_SOCKET && FD_ISSET(g_socket6, &rfds))
            handle_udp(g_socket6, cb, cb_closure);
    }
    return 0;
}

void dht_runtime_request_stop(void)
{
    g_stop = 1;
}

int dht_runtime_shutdown(void)
{
    g_stop = 1;
    if(g_socket != NET_INVALID_SOCKET) {
        save_dht_state();
        dht_uninit();
        net_close(g_socket);
        g_socket = NET_INVALID_SOCKET;
    }
    if(g_socket6 != NET_INVALID_SOCKET) {
        net_close(g_socket6);
        g_socket6 = NET_INVALID_SOCKET;
    }
    return 0;
}

int dht_sendto(int sockfd, const void *buf, int len, int flags,
               const struct sockaddr *to, int tolen)
{
    net_socket_t socket = sockfd == 0 ? g_socket :
                          sockfd == 1 ? g_socket6 : NET_INVALID_SOCKET;
    return sendto(socket, (const char *)buf, len, flags, to, tolen);
}

int dht_blacklisted(const struct sockaddr *sa, int salen)
{
    (void)sa;
    (void)salen;
    return 0;
}

void dht_hash(void *hash_return, int hash_size,
              const void *v1, int len1,
              const void *v2, int len2,
              const void *v3, int len3)
{
    EVP_MD_CTX *ctx;
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    ctx = EVP_MD_CTX_new();
    if(ctx == NULL)
        return;
    EVP_DigestInit_ex(ctx, EVP_sha1(), NULL);
    EVP_DigestUpdate(ctx, v1, len1);
    EVP_DigestUpdate(ctx, v2, len2);
    EVP_DigestUpdate(ctx, v3, len3);
    EVP_DigestFinal_ex(ctx, digest, &digest_len);
    EVP_MD_CTX_free(ctx);
    if(hash_size > (int)digest_len)
        memset((char *)hash_return, 0, hash_size);
    memcpy(hash_return, digest, hash_size > (int)digest_len ? (int)digest_len : hash_size);
}

int dht_random_bytes(void *buf, size_t size)
{
    if(size > INT_MAX)
        return -1;
    return RAND_bytes(buf, (int)size) == 1 ? (int)size : -1;
}
