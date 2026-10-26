// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    if(argc != 4 || strcmp(argv[1], "-p") != 0 || strcmp(argv[3], "node") != 0)
        return 2;
    if(strcmp(argv[2], "fail:15888") == 0)
        return 7;
    if(strcmp(argv[2], "host;$(false) with \\\"quotes\\\":15888") != 0)
        return 3;
    puts("| Listener | tcp://0.0.0.0:11010 |");
    puts("| Public IPv4 | 1.2.3.4 |");
    puts("| UDP Stun Type | Public |");
    return 0;
}
