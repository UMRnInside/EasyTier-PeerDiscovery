// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "args.h"

#include <getopt.h>
#include <stdlib.h>
#include <string.h>

enum {
    OPT_HTTP = 256,
    OPT_DHT_PORT,
    OPT_BOOTSTRAP,
    OPT_ANNOUNCE_INTERVAL,
    OPT_NO_ANNOUNCE,
    OPT_IPV6,
    OPT_NO_IPV6,
    OPT_QUIC,
    OPT_RPC,
    OPT_SECRET_HASH,
    OPT_EASYTIER_CLI_PATH,
    OPT_DHT_STATE_PATH,
    OPT_VERSION,
};

static const struct option long_options[] = {
    {"network-name", required_argument, NULL, 'n'},
    {"secret", required_argument, NULL, 'k'},
    {"secret-hash", required_argument, NULL, OPT_SECRET_HASH},
    {"http", required_argument, NULL, OPT_HTTP},
    {"dht-port", required_argument, NULL, OPT_DHT_PORT},
    {"bootstrap", required_argument, NULL, OPT_BOOTSTRAP},
    {"announce-interval", required_argument, NULL, OPT_ANNOUNCE_INTERVAL},
    {"no-announce", no_argument, NULL, OPT_NO_ANNOUNCE},
    {"ipv6", no_argument, NULL, OPT_IPV6},
    {"no-ipv6", no_argument, NULL, OPT_NO_IPV6},
    {"quic", no_argument, NULL, OPT_QUIC},
    {"rpc", required_argument, NULL, OPT_RPC},
    {"easytier-cli-path", required_argument, NULL, OPT_EASYTIER_CLI_PATH},
    {"dht-state-path", required_argument, NULL, OPT_DHT_STATE_PATH},
    {"verbose", no_argument, NULL, 'v'},
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, OPT_VERSION},
    {0, 0, 0, 0},
};

static int parse_uint(const char *s, unsigned long max, unsigned long *out)
{
    char *end = NULL;
    long v;
    if(s[0] == '\0')
        return -1;
    v = strtol(s, &end, 10);
    if(*end != '\0' || v < 0 || (unsigned long)v > max)
        return -1;
    *out = (unsigned long)v;
    return 0;
}

static int parse_host_port(const char *s, char **host_out, unsigned short *port_out)
{
    const char *colon;
    char *host;
    unsigned long port;
    if((colon = strrchr(s, ':')) == NULL || colon == s || colon[1] == '\0')
        return -1;
    if(parse_uint(colon + 1, 65535, &port) != 0)
        return -1;
    host = strndup(s, (size_t)(colon - s));
    if(host == NULL)
        return -1;
    *host_out = host;
    *port_out = (unsigned short)port;
    return 0;
}

static int add_bootstrap(peerdisc_args_t *a, const char *s)
{
    char **tmp;
    char *copy;
    tmp = realloc(a->bootstrap, (a->bootstrap_len + 1) * sizeof(char *));
    if(tmp == NULL)
        return -1;
    a->bootstrap = tmp;
    copy = strdup(s);
    if(copy == NULL)
        return -1;
    a->bootstrap[a->bootstrap_len++] = copy;
    return 0;
}

void args_print_usage(FILE *out)
{
    fprintf(out,
            "Usage: easytier-peerdisc --network-name <name> --secret <secret> [options]\n"
            "\n"
            "Core (required):\n"
            "  -n, --network-name <name>    Network name (required)\n"
            "  -k, --secret <secret>        Network secret\n"
            "      --secret-hash <hex>      Precomputed 32-byte secret hash\n"
            "\n"
            "HTTP /endpoint service:\n"
            "      --http <host:port>       Listen address (default 127.0.0.1:11008)\n"
            "\n"
            "DHT runtime:\n"
            "      --dht-port <port>        DHT UDP listen port (default 0 = random)\n"
            "      --dht-state-path <directory>  Override the platform state directory\n"
            "      --bootstrap <host:port>  Custom bootstrap node (repeatable)\n"
            "      --announce-interval <s>  Announce interval, >= %u (default %u)\n"
            "      --no-announce            Query only, never announce\n"
            "      --ipv6 / --no-ipv6       Dual-stack toggle (default on)\n"
            "      --quic                   Enable QUIC discovery (default off)\n"
            "\n"
            "External endpoint source (easytier-cli RPC):\n"
            "      --rpc <host:port>        easytier-cli rpc portal (default 127.0.0.1:%u)\n"
            "      --easytier-cli-path <path-to-easytier-cli>  CLI executable (default PATH lookup)\n"
            "\n"
            "Generic:\n"
            "  -v, --verbose                Verbose logging\n"
            "  -h, --help                   Show this help and exit\n"
            "      --version                Show version and exit\n",
            MIN_ANNOUNCE_INTERVAL, DEFAULT_ANNOUNCE_INTERVAL, DEFAULT_RPC_PORT);
}

int args_parse(int argc, char **argv, peerdisc_args_t *out)
{
    const char *env;
    int opt;

    memset(out, 0, sizeof(*out));
    out->http_host = strdup("127.0.0.1");
    out->http_port = DEFAULT_HTTP_PORT;
    out->dht_port = 0;
    out->announce_interval = DEFAULT_ANNOUNCE_INTERVAL;
    out->ipv6 = 1;
    out->rpc_host = strdup("127.0.0.1");
    out->rpc_port = DEFAULT_RPC_PORT;
    if(out->http_host == NULL || out->rpc_host == NULL) {
        args_free(out);
        fprintf(stderr, "easytier-peerdisc: out of memory\n");
        return 1;
    }

    while((opt = getopt_long(argc, argv, "n:k:vh", long_options, NULL)) != -1) {
        switch(opt) {
        case 'n':
            if(out->network_name == NULL)
                out->network_name = strdup(optarg);
            break;
        case 'k':
            if(out->secret == NULL)
                out->secret = strdup(optarg);
            break;
        case OPT_SECRET_HASH:
            if(out->secret_hash == NULL)
                out->secret_hash = strdup(optarg);
            break;
        case 'v':
            out->verbose = 1;
            break;
        case OPT_EASYTIER_CLI_PATH:
        case OPT_DHT_STATE_PATH: {
            char **path = opt == OPT_EASYTIER_CLI_PATH ?
                          &out->easytier_cli_path : &out->dht_state_path;
            char *copy;

            if(optarg[0] == '\0') {
                fprintf(stderr, "easytier-peerdisc: path must not be empty\n");
                goto fail;
            }
            copy = strdup(optarg);
            if(copy == NULL) {
                fprintf(stderr, "easytier-peerdisc: out of memory\n");
                goto fail;
            }
            free(*path);
            *path = copy;
            break;
        }
        case 'h':
            args_print_usage(stdout);
            return 2;
        case OPT_VERSION:
            printf("easytier-peerdisc %s\n", PEERDISC_VERSION);
            return 2;
        case OPT_HTTP: {
            char *host = NULL;
            unsigned short port = 0;
            if(parse_host_port(optarg, &host, &port) != 0) {
                fprintf(stderr, "easytier-peerdisc: invalid --http value '%s'\n", optarg);
                goto fail;
            }
            free(out->http_host);
            out->http_host = host;
            out->http_port = port;
            break;
        }
        case OPT_DHT_PORT: {
            unsigned long v;
            if(parse_uint(optarg, 65535, &v) != 0) {
                fprintf(stderr, "easytier-peerdisc: invalid --dht-port '%s'\n", optarg);
                goto fail;
            }
            out->dht_port = (unsigned short)v;
            break;
        }
        case OPT_BOOTSTRAP:
            if(add_bootstrap(out, optarg) != 0) {
                fprintf(stderr, "easytier-peerdisc: out of memory\n");
                goto fail;
            }
            break;
        case OPT_ANNOUNCE_INTERVAL: {
            unsigned long v;
            if(parse_uint(optarg, 2147483647, &v) != 0 || v < MIN_ANNOUNCE_INTERVAL) {
                fprintf(stderr,
                        "easytier-peerdisc: --announce-interval must be an integer >= %u\n",
                        MIN_ANNOUNCE_INTERVAL);
                goto fail;
            }
            out->announce_interval = (unsigned)v;
            break;
        }
        case OPT_NO_ANNOUNCE:
            out->no_announce = 1;
            break;
        case OPT_IPV6:
            out->ipv6 = 1;
            break;
        case OPT_NO_IPV6:
            out->ipv6 = 0;
            break;
        case OPT_QUIC:
            out->quic = 1;
            break;
        case OPT_RPC: {
            char *host = NULL;
            unsigned short port = 0;
            if(parse_host_port(optarg, &host, &port) != 0) {
                fprintf(stderr, "easytier-peerdisc: invalid --rpc value '%s'\n", optarg);
                goto fail;
            }
            free(out->rpc_host);
            out->rpc_host = host;
            out->rpc_port = port;
            break;
        }
        case '?':
        default:
            args_print_usage(stderr);
            goto fail;
        }
    }

    if(out->network_name == NULL) {
        env = getenv("ET_NETWORK_NAME");
        if(env != NULL && env[0] != '\0')
            out->network_name = strdup(env);
    }
    if(out->secret == NULL && out->secret_hash == NULL) {
        env = getenv("ET_NETWORK_SECRET");
        if(env != NULL && env[0] != '\0')
            out->secret = strdup(env);
    }

    if(out->secret != NULL && out->secret_hash != NULL) {
        fprintf(stderr, "easytier-peerdisc: --secret and --secret-hash are mutually exclusive\n");
        goto fail;
    }
    if(out->network_name == NULL || (out->secret == NULL && out->secret_hash == NULL)) {
        fprintf(stderr,
                "easytier-peerdisc: network name and a secret credential are required\n");
        args_print_usage(stderr);
        goto fail;
    }

    if(optind < argc) {
        fprintf(stderr, "easytier-peerdisc: unexpected argument '%s'\n", argv[optind]);
        goto fail;
    }

    return 0;

fail:
    args_free(out);
    return 1;
}

void args_free(peerdisc_args_t *a)
{
    size_t i;
    free(a->network_name);
    free(a->secret);
    free(a->secret_hash);
    free(a->http_host);
    free(a->rpc_host);
    free(a->easytier_cli_path);
    free(a->dht_state_path);
    for(i = 0; i < a->bootstrap_len; i++)
        free(a->bootstrap[i]);
    free(a->bootstrap);
    memset(a, 0, sizeof(*a));
}
