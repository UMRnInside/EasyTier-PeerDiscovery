// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    if(argc != 6 || strcmp(argv[1], "-p") != 0 ||
       strcmp(argv[3], "--output") != 0 || strcmp(argv[4], "json") != 0 ||
       strcmp(argv[5], "node") != 0)
        return 2;
    if(strcmp(argv[2], "fail:15888") == 0)
        return 7;
    if(strncmp(argv[2], "nat-", 4) == 0) {
        const char *end = strchr(argv[2] + 4, ':');
        if(end == NULL)
            return 4;
        printf("{\"stun_info\":{\"tcp_nat_type\":\"OpenInternet\","
               "\"udp_nat_type\":\"%.*s\",\"public_ip\":[\"1.2.3.4\"]},"
               "\"listeners\":[\"tcp://0.0.0.0:11010\"]}\n",
               (int)(end - (argv[2] + 4)), argv[2] + 4);
        return 0;
    }
    if(strcmp(argv[2], "invalid:15888") == 0) {
        puts("{\"stun_info\":");
        return 0;
    }
    if(strcmp(argv[2], "empty:15888") == 0)
        return 0;
    if(strcmp(argv[2], "oversized:15888") == 0) {
        size_t i;
        for(i = 0; i < 1024 * 1024 + 1; i++)
            putchar(' ');
        return 0;
    }
    if(strcmp(argv[2], "ipv6:15888") == 0) {
        puts("{\"stun_info\":{\"udp_nat_type\":\"FriendlyNAT\","
             "\"public_ip\":[\"2001:db8::1\"]},"
             "\"listeners\":[\"ring://test\",\"tcp://[::]:11010\","
             "\"udp://[::]:11010\"]}");
        return 0;
    }
    if(strcmp(argv[2], "host;$(false) with \\\"quotes\\\":15888") != 0)
        return 3;
    puts("{\"listeners\":[\"ring://test\",\"tcp://0.0.0.0:11010\","
         "\"udp://0.0.0.0:11010\"],"
         "\"stun_info\":{\"public_ip\":[\"1.2.3.4\"],\"udp_nat_type\":\"Public\"}}");
    return 0;
}
