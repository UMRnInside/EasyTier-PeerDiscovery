// SPDX-License-Identifier: GPL-3.0-or-later

#define _XOPEN_SOURCE 700

#include "args.h"
#include "dht_state.h"
#include "easytier_rpc.h"
#include "unity.h"

#include <arpa/inet.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char g_directory[PATH_MAX];
static char g_executable[PATH_MAX];

void setUp(void) {}
void tearDown(void) {}

static void test_path_arguments(void)
{
    peerdisc_args_t args;
    char *argv[] = {"peerdisc", "-n", "test", "-k", "test",
                    "--easytier-cli-path", "/opt/Easy Tier/cli;literal",
                    "--dht-state-path", "relative state", NULL};
    char *empty[] = {"peerdisc", "--dht-state-path", "", NULL};

    optind = 1;
    TEST_ASSERT_EQUAL_INT(0, args_parse(9, argv, &args));
    TEST_ASSERT_EQUAL_STRING(argv[6], args.easytier_cli_path);
    TEST_ASSERT_EQUAL_STRING(argv[8], args.dht_state_path);
    args_free(&args);
    optind = 1;
    TEST_ASSERT_EQUAL_INT(1, args_parse(3, empty, &args));
}

static void test_custom_state_directory(void)
{
    dht_state_t saved = {0}, loaded;
    char path[PATH_MAX];
    char actual[PATH_MAX];
    char tiny[1];

    snprintf(path, sizeof(path), "%s/custom state", g_directory);
    TEST_ASSERT_EQUAL_INT(0, dht_state_set_directory(path));
    TEST_ASSERT_EQUAL_INT(0, dht_state_directory(actual, sizeof(actual)));
    TEST_ASSERT_EQUAL_STRING(path, actual);
    TEST_ASSERT_EQUAL_INT(-1, dht_state_directory(tiny, sizeof(tiny)));
    TEST_ASSERT_EQUAL_INT(0, dht_state_load(&loaded));
    memset(saved.node_id, 0x5a, sizeof(saved.node_id));
    saved.has_node_id = 1;
    saved.nodes4_len = 1;
    saved.nodes4[0].sin_port = htons(6881);
    inet_pton(AF_INET, "1.2.3.4", &saved.nodes4[0].sin_addr);
    saved.nodes6_len = 1;
    saved.nodes6[0].sin6_port = htons(6882);
    inet_pton(AF_INET6, "2001:db8::1", &saved.nodes6[0].sin6_addr);
    TEST_ASSERT_EQUAL_INT(0, dht_state_save(&saved));
    TEST_ASSERT_EQUAL_INT(0, dht_state_save(&saved));
    TEST_ASSERT_EQUAL_INT(1, dht_state_load(&loaded));
    TEST_ASSERT_EQUAL_MEMORY(saved.node_id, loaded.node_id, sizeof(saved.node_id));
    TEST_ASSERT_EQUAL_INT(1, loaded.nodes4_len);
    TEST_ASSERT_EQUAL_INT(1, loaded.nodes6_len);
    TEST_ASSERT_EQUAL_MEMORY(&saved.nodes4[0].sin_addr, &loaded.nodes4[0].sin_addr, 4);
    TEST_ASSERT_EQUAL_MEMORY(&saved.nodes6[0].sin6_addr, &loaded.nodes6[0].sin6_addr, 16);
    TEST_ASSERT_EQUAL_INT(saved.nodes4[0].sin_port, loaded.nodes4[0].sin_port);
    TEST_ASSERT_EQUAL_INT(saved.nodes6[0].sin6_port, loaded.nodes6[0].sin6_port);
    snprintf(actual, sizeof(actual), "%s/dht-state.bin", path);
    unlink(actual);
    rmdir(path);
    TEST_ASSERT_EQUAL_INT(0, dht_state_set_directory(NULL));
    TEST_ASSERT_EQUAL_INT(0, dht_state_directory(actual, sizeof(actual)));
    TEST_ASSERT_EQUAL_INT(0, dht_state_default_directory(path, sizeof(path)));
    TEST_ASSERT_EQUAL_STRING(path, actual);
}

static void test_cli_path_and_literal_arguments(void)
{
    char path[PATH_MAX];
    char endpoint[256];
    int nat_ok = 0;

    snprintf(path, sizeof(path), "%s/Easy Tier;literal", g_directory);
    TEST_ASSERT_EQUAL_INT(0, symlink(g_executable, path));
    TEST_ASSERT_EQUAL_INT(0, easytier_rpc_init("host;$(false) with space", 15888, path));
    TEST_ASSERT_EQUAL_INT(0, easytier_rpc_get_endpoint(endpoint, sizeof(endpoint), &nat_ok));
    TEST_ASSERT_EQUAL_STRING("tcp://1.2.3.4:11010", endpoint);
    TEST_ASSERT_EQUAL_INT(1, nat_ok);
    unlink(path);
}

int main(int argc, char **argv)
{
    int result;

    if(argc == 4 && strcmp(argv[1], "-p") == 0) {
        if(strcmp(argv[2], "host;$(false) with space:15888") != 0 ||
           strcmp(argv[3], "node") != 0)
            return 1;
        puts("| Listener 1 | tcp://0.0.0.0:11010 |\n"
             "| Public IPv4 | 1.2.3.4 |\n"
             "| UDP Stun Type | Public |");
        return 0;
    }
    if(realpath(argv[0], g_executable) == NULL ||
       getcwd(g_directory, sizeof(g_directory)) == NULL)
        return 1;
    if(strlen(g_directory) + sizeof("/path-tests-XXXXXX") > sizeof(g_directory))
        return 1;
    strcat(g_directory, "/path-tests-XXXXXX");
    if(mkdtemp(g_directory) == NULL)
        return 1;
    UNITY_BEGIN();
    RUN_TEST(test_path_arguments);
    RUN_TEST(test_custom_state_directory);
    RUN_TEST(test_cli_path_and_literal_arguments);
    result = UNITY_END();
    rmdir(g_directory);
    return result;
}
