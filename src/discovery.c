// SPDX-License-Identifier: GPL-3.0-or-later

#include "discovery.h"

#include "dht_runtime.h"
#include "easytier_rpc.h"
#include "endpoint.h"

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MIN_UPTIME_BEFORE_ANNOUNCE 300
#define ANNOUNCE_ADDR_LEN 64

static char g_network_name[128];
static infohash_t g_tcp_topic;
static infohash_t g_quic_topic;
static unsigned g_search_interval;
static int g_ipv6;

static time_t g_start_time;
static time_t g_last_search;
static int g_last_found;

static unsigned short url_port(const char *url)
{
    const char *p = strrchr(url, ':');
    if(p == NULL)
        return 0;
    return (unsigned short)strtoul(p + 1, NULL, 10);
}

static void search_topic(const infohash_t *topic, int port, int af)
{
    dht_runtime_search(topic->hash, port, af, discovery_dht_callback, NULL);
}

static void launch_search(int port, int af)
{
    search_topic(&g_tcp_topic, port, af);
    search_topic(&g_quic_topic, port, af);
}

static void add_peer(int af, const unsigned char *p, const infohash_t *topic)
{
    char addr[ANNOUNCE_ADDR_LEN];
    const char *scheme;
    char url[256];
    unsigned short port;

    if(af == AF_INET) {
        inet_ntop(AF_INET, p, addr, sizeof(addr));
        port = (unsigned short)((p[4] << 8) | p[5]);
    } else {
        inet_ntop(AF_INET6, p, addr, sizeof(addr));
        port = (unsigned short)((p[16] << 8) | p[17]);
    }
    scheme = (memcmp(topic->hash, g_tcp_topic.hash, INFOHASH_LEN) == 0) ? "tcp" : "quic";
    if(af == AF_INET)
        snprintf(url, sizeof(url), "%s://%s:%u", scheme, addr, port);
    else
        snprintf(url, sizeof(url), "%s://[%s]:%u", scheme, addr, port);
    endpoint_add(url);
}

static void maybe_announce(time_t now)
{
    char url[256];
    int nat_ok = 0;
    unsigned short aport;

    if(g_last_found)
        return;
    if(now - g_start_time < MIN_UPTIME_BEFORE_ANNOUNCE)
        return;
    if(easytier_rpc_get_endpoint(url, sizeof(url), &nat_ok) != 0 || !nat_ok)
        return;
    aport = url_port(url);
    if(aport == 0)
        return;
    launch_search(aport, AF_INET);
}

int discovery_init(const char *network_name, const infohash_t *tcp_topic,
                   const infohash_t *quic_topic, unsigned search_interval,
                   int ipv6)
{
    if(network_name == NULL || tcp_topic == NULL || quic_topic == NULL)
        return -1;
    snprintf(g_network_name, sizeof(g_network_name), "%s", network_name);
    g_tcp_topic = *tcp_topic;
    g_quic_topic = *quic_topic;
    g_search_interval = search_interval;
    g_ipv6 = ipv6;
    g_last_search = 0;
    g_last_found = 0;
    return 0;
}

int discovery_start(void)
{
    g_start_time = time(NULL);
    g_last_found = 0;
    launch_search(0, AF_INET);
    if(g_ipv6)
        launch_search(0, AF_INET6);
    g_last_search = time(NULL);
    return 0;
}

int discovery_stop(void)
{
    return 0;
}

void discovery_dht_callback(void *closure, int event,
                            const unsigned char *info_hash, const void *data,
                            size_t data_len)
{
    const unsigned char *p;
    size_t i;
    (void)closure;

    switch(event) {
    case DHT_EVENT_VALUES:
        g_last_found = 1;
        for(i = 0; i + 6 <= data_len; i += 6) {
            p = (const unsigned char *)data + i;
            add_peer(AF_INET, p, (const infohash_t *)info_hash);
        }
        break;
    case DHT_EVENT_VALUES6:
        g_last_found = 1;
        for(i = 0; i + 18 <= data_len; i += 18) {
            p = (const unsigned char *)data + i;
            add_peer(AF_INET6, p, (const infohash_t *)info_hash);
        }
        break;
    default:
        break;
    }
}

void discovery_periodic(void *closure)
{
    time_t now;
    (void)closure;

    now = time(NULL);
    if(now - g_last_search < (time_t)g_search_interval)
        return;

    g_last_found = 0;
    launch_search(0, AF_INET);
    if(g_ipv6)
        launch_search(0, AF_INET6);
    g_last_search = now;
    maybe_announce(now);
}