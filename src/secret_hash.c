// SPDX-License-Identifier: GPL-3.0-or-later

#include "infohash.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

static void print_usage(FILE *out)
{
    fprintf(out,
            "Usage: et-secret-hash --secret <secret> [--network-name <name>]\n"
            "\n"
            "Options:\n"
            "      --network-name <name>  Optional network context\n"
            "      --secret <secret>      Network secret (required)\n"
            "  -h, --help                 Show this help and exit\n");
}

int main(int argc, char **argv)
{
    static const struct option options[] = {
        {"network-name", required_argument, NULL, 'n'},
        {"secret", required_argument, NULL, 'k'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0},
    };
    const char *network_name = NULL;
    const char *secret = NULL;
    uint8_t hash[SECRET_HASH_LEN];
    int opt;
    size_t i;

    while((opt = getopt_long(argc, argv, "n:k:h", options, NULL)) != -1) {
        switch(opt) {
        case 'n':
            network_name = optarg;
            break;
        case 'k':
            secret = optarg;
            break;
        case 'h':
            print_usage(stdout);
            return EXIT_SUCCESS;
        default:
            print_usage(stderr);
            return EXIT_FAILURE;
        }
    }

    if(secret == NULL || optind < argc) {
        print_usage(stderr);
        return EXIT_FAILURE;
    }
    if(network_name != NULL && network_name[0] == '\0') {
        fprintf(stderr, "et-secret-hash: network name cannot be empty\n");
        return EXIT_FAILURE;
    }
    if(secret_hash_derive(hash, secret) != 0) {
        fprintf(stderr, "et-secret-hash: failed to derive secret hash\n");
        return EXIT_FAILURE;
    }

    for(i = 0; i < SECRET_HASH_LEN; i++)
        printf("%02x", hash[i]);
    putchar('\n');
    return EXIT_SUCCESS;
}
