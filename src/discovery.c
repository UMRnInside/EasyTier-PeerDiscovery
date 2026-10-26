// SPDX-License-Identifier: GPL-3.0-or-later

#include "discovery.h"

#include "dht_runtime.h"
#include "easytier_rpc.h"
#include "endpoint.h"

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define INITIAL_ANNOUNCE_DELAY 30
#define ANNOUNCE_ADDR_LEN 64
#define QUERY_TCP4 (1u << 0)
#define QUERY_UDP4 (1u << 1)
#define QUERY_QUIC4 (1u << 2)
#define QUERY_TCP6 (1u << 3)
#define QUERY_UDP6 (1u << 4)
#define QUERY_QUIC6 (1u << 5)

static char g_network_name[128];
static infohash_t g_tcp_topic;
static infohash_t g_udp_topic;
static infohash_t g_quic_topic;
static unsigned g_search_interval;
static int g_ipv6;
static int g_no_announce;
static int g_quic;

static time_t g_start_time;
static time_t g_last_search;
static int g_last_found;
static unsigned g_query_pending;
static int g_query_failed;
static int g_announce_checked;

static unsigned short url_port(const char *url)
{
    const char *p = strrchr(url, ':');
    if(p == NULL)
        return 0;
    return (unsigned short)strtoul(p + 1, NULL, 10);
}

static int search_topic(const infohash_t *topic, int port, int af)
{
    return dht_runtime_search(topic->hash, port, af, discovery_dht_callback, NULL);
}

static void launch_search(int port, int af)
{
    search_topic(&g_tcp_topic, port, af);
    search_topic(&g_udp_topic, port, af);
    if(g_quic)
        search_topic(&g_quic_topic, port, af);
}

static unsigned query_bit(const unsigned char *info_hash, int af)
{
    unsigned family_offset = af == AF_INET6 ? 3u : 0u;

    if(memcmp(info_hash, g_tcp_topic.hash, INFOHASH_LEN) == 0)
        return 1u << family_offset;
    if(memcmp(info_hash, g_udp_topic.hash, INFOHASH_LEN) == 0)
        return 1u << (family_offset + 1u);
    if(g_quic && memcmp(info_hash, g_quic_topic.hash, INFOHASH_LEN) == 0)
        return 1u << (family_offset + 2u);
    return 0;
}

static void start_query(const infohash_t *topic, int af, unsigned bit)
{
    if(search_topic(topic, 0, af) < 0) {
        g_query_pending &= ~bit;
        g_query_failed = 1;
    }
}

static void start_query_round(time_t now)
{
    g_last_found = 0;
    g_query_failed = 0;
    g_announce_checked = 0;
    g_query_pending = QUERY_TCP4 | QUERY_UDP4;
    if(g_quic)
        g_query_pending |= QUERY_QUIC4;
    if(g_ipv6) {
        g_query_pending |= QUERY_TCP6 | QUERY_UDP6;
        if(g_quic)
            g_query_pending |= QUERY_QUIC6;
    }

    start_query(&g_tcp_topic, AF_INET, QUERY_TCP4);
    start_query(&g_udp_topic, AF_INET, QUERY_UDP4);
    if(g_quic)
        start_query(&g_quic_topic, AF_INET, QUERY_QUIC4);
    if(g_ipv6) {
        start_query(&g_tcp_topic, AF_INET6, QUERY_TCP6);
        start_query(&g_udp_topic, AF_INET6, QUERY_UDP6);
        if(g_quic)
            start_query(&g_quic_topic, AF_INET6, QUERY_QUIC6);
    }
    g_last_search = now;
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
    if(memcmp(topic->hash, g_tcp_topic.hash, INFOHASH_LEN) == 0)
        scheme = "tcp";
    else if(memcmp(topic->hash, g_udp_topic.hash, INFOHASH_LEN) == 0)
        scheme = "udp";
    else
        scheme = "quic";
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

    if(g_no_announce || g_last_found || g_query_failed || g_announce_checked)
        return;
    if(now - g_start_time < INITIAL_ANNOUNCE_DELAY)
        return;
    g_announce_checked = 1;
    if(easytier_rpc_get_endpoint(url, sizeof(url), &nat_ok) != 0 || !nat_ok)
        return;
    aport = url_port(url);
    if(aport == 0)
        return;
    launch_search(aport, AF_INET);
    if(g_ipv6)
        launch_search(aport, AF_INET6);
}

int discovery_init(const char *network_name, const infohash_t *tcp_topic,
                   const infohash_t *udp_topic, const infohash_t *quic_topic,
                   unsigned search_interval, int ipv6, int no_announce,
                   int quic)
{
    if(network_name == NULL || tcp_topic == NULL || udp_topic == NULL ||
       (quic && quic_topic == NULL))
        return -1;
    snprintf(g_network_name, sizeof(g_network_name), "%s", network_name);
    g_tcp_topic = *tcp_topic;
    g_udp_topic = *udp_topic;
    if(quic)
        g_quic_topic = *quic_topic;
    g_search_interval = search_interval;
    g_ipv6 = ipv6;
    g_no_announce = no_announce;
    g_quic = quic;
    g_last_search = 0;
    g_last_found = 0;
    g_query_pending = 0;
    g_query_failed = 0;
    g_announce_checked = 0;
    return 0;
}

int discovery_start(void)
{
    g_start_time = time(NULL);
    start_query_round(g_start_time);
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
    unsigned bit;
    size_t i;
    (void)closure;

    switch(event) {
    case DHT_EVENT_VALUES:
        bit = query_bit(info_hash, AF_INET);
        if((g_query_pending & bit) != 0)
            g_last_found = 1;
        for(i = 0; i + 6 <= data_len; i += 6) {
            p = (const unsigned char *)data + i;
            add_peer(AF_INET, p, (const infohash_t *)info_hash);
        }
        break;
    case DHT_EVENT_VALUES6:
        bit = query_bit(info_hash, AF_INET6);
        if((g_query_pending & bit) != 0)
            g_last_found = 1;
        for(i = 0; i + 18 <= data_len; i += 18) {
            p = (const unsigned char *)data + i;
            add_peer(AF_INET6, p, (const infohash_t *)info_hash);
        }
        break;
    case DHT_EVENT_SEARCH_DONE:
        bit = query_bit(info_hash, AF_INET);
        if((g_query_pending & bit) != 0) {
            g_query_pending &= ~bit;
            if(g_query_pending == 0)
                maybe_announce(time(NULL));
        }
        break;
    case DHT_EVENT_SEARCH_DONE6:
        bit = query_bit(info_hash, AF_INET6);
        if((g_query_pending & bit) != 0) {
            g_query_pending &= ~bit;
            if(g_query_pending == 0)
                maybe_announce(time(NULL));
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
    if(g_query_pending == 0)
        maybe_announce(now);
    if(g_query_pending != 0 || now - g_last_search < (time_t)g_search_interval)
        return;

    start_query_round(now);
}
