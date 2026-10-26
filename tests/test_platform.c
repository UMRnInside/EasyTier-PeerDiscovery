// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "dht_runtime.h"
#include "easytier_rpc.h"
#include "endpoint.h"
#include "httpd.h"
#include "unity.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static const char *g_cli_path;
static char g_state_directory[4096];
static net_socket_t g_peer = NET_INVALID_SOCKET;
static net_socket_t g_client = NET_INVALID_SOCKET;
static int g_dht_initialized;

void setUp(void) {}

void tearDown(void)
{
    if(g_dht_initialized) {
        dht_runtime_shutdown();
        g_dht_initialized = 0;
    }
    if(g_peer != NET_INVALID_SOCKET) {
        net_close(g_peer);
        g_peer = NET_INVALID_SOCKET;
    }
    if(g_client != NET_INVALID_SOCKET) {
        net_close(g_client);
        g_client = NET_INVALID_SOCKET;
    }
    httpd_shutdown();
    endpoint_cleanup();
}

static void wait_readable(net_socket_t socket)
{
    fd_set readfds;
    struct timeval timeout = {3, 0};
    FD_ZERO(&readfds);
    FD_SET(socket, &readfds);
    TEST_ASSERT_EQUAL_INT(1, net_select(socket, &readfds, &timeout));
    TEST_ASSERT_TRUE(FD_ISSET(socket, &readfds));
}

static void test_udp_and_dht_socket_tokens(void)
{
    struct sockaddr_in address;
    net_socklen_t length = sizeof(address);
    char packet[4096];
    int received;

    g_peer = socket(AF_INET, SOCK_DGRAM, 0);
    TEST_ASSERT_TRUE(g_peer != NET_INVALID_SOCKET);
    TEST_ASSERT_EQUAL_INT(0, net_set_nonblocking(g_peer));
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    TEST_ASSERT_EQUAL_INT(0, bind(g_peer, (struct sockaddr *)&address, sizeof(address)));
    TEST_ASSERT_EQUAL_INT(0, getsockname(g_peer, (struct sockaddr *)&address, &length));
    TEST_ASSERT_EQUAL_INT(-1, recv(g_peer, packet, sizeof(packet), 0));
    TEST_ASSERT_EQUAL_INT(0, dht_runtime_init(0, 1, 0, g_state_directory));
    g_dht_initialized = 1;
    TEST_ASSERT_TRUE(dht_ping_node((struct sockaddr *)&address, sizeof(address)) > 0);
    wait_readable(g_peer);
    received = recv(g_peer, packet, sizeof(packet), 0);
    TEST_ASSERT_TRUE(received > 0);
    TEST_ASSERT_EQUAL_CHAR('d', packet[0]);
}

static void test_ipv6_dht_socket_token(void)
{
    struct sockaddr_in6 address;
    net_socklen_t length = sizeof(address);
    char packet[4096];

    g_peer = socket(AF_INET6, SOCK_DGRAM, 0);
    if(g_peer == NET_INVALID_SOCKET)
        TEST_IGNORE_MESSAGE("IPv6 is unavailable");
    memset(&address, 0, sizeof(address));
    address.sin6_family = AF_INET6;
    address.sin6_addr = in6addr_loopback;
    if(bind(g_peer, (struct sockaddr *)&address, sizeof(address)) != 0)
        TEST_IGNORE_MESSAGE("IPv6 loopback is unavailable");
    TEST_ASSERT_EQUAL_INT(0, getsockname(g_peer, (struct sockaddr *)&address, &length));
    TEST_ASSERT_EQUAL_INT(0, net_set_nonblocking(g_peer));
    TEST_ASSERT_EQUAL_INT(0, dht_runtime_init(0, 1, 0, g_state_directory));
    g_dht_initialized = 1;
    TEST_ASSERT_TRUE(dht_ping_node((struct sockaddr *)&address, sizeof(address)) > 0);
    wait_readable(g_peer);
    TEST_ASSERT_TRUE(recv(g_peer, packet, sizeof(packet), 0) > 0);
    TEST_ASSERT_EQUAL_CHAR('d', packet[0]);
}

static void stop_runtime(void *closure)
{
    (void)closure;
    dht_runtime_request_stop();
}

static void test_http_socket_interface(void)
{
    struct sockaddr_in address;
    net_socklen_t length = sizeof(address);
    const char request[] = "GET /endpoint HTTP/1.1\r\nHost: localhost\r\n\r\n";
    char response[512];
    int received;

    TEST_ASSERT_EQUAL_INT(0, endpoint_init());
    TEST_ASSERT_EQUAL_INT(0, httpd_init("127.0.0.1", 0));
    TEST_ASSERT_EQUAL_INT(0, dht_runtime_init(0, 1, 0, g_state_directory));
    g_dht_initialized = 1;
    TEST_ASSERT_EQUAL_INT(0, getsockname(httpd_fd(), (struct sockaddr *)&address, &length));
    g_client = socket(AF_INET, SOCK_STREAM, 0);
    TEST_ASSERT_TRUE(g_client != NET_INVALID_SOCKET);
    TEST_ASSERT_EQUAL_INT(0, connect(g_client, (struct sockaddr *)&address, length));
    TEST_ASSERT_EQUAL_INT(sizeof(request) - 1, send(g_client, request, sizeof(request) - 1, 0));
    wait_readable(httpd_fd());
    TEST_ASSERT_EQUAL_INT(0, dht_runtime_run(httpd_fd(), NULL, NULL, stop_runtime, NULL));
    wait_readable(g_client);
    received = recv(g_client, response, sizeof(response) - 1, 0);
    TEST_ASSERT_TRUE(received > 0);
    response[received] = '\0';
    TEST_ASSERT_NOT_NULL(strstr(response, "HTTP/1.1 200 OK\r\n"));
}

static void test_random_bytes(void)
{
    unsigned char bytes[32];
    TEST_ASSERT_EQUAL_INT(sizeof(bytes), dht_random_bytes(bytes, sizeof(bytes)));
    TEST_ASSERT_EQUAL_INT(-1, dht_random_bytes(bytes, (size_t)INT_MAX + 1));
}

static void test_cli_literal_arguments_and_exit_status(void)
{
    char endpoint[256];
    int nat_ok = 0;
    TEST_ASSERT_EQUAL_INT(0, easytier_rpc_init("host;$(false) with \\\"quotes\\\"", 15888, g_cli_path));
    TEST_ASSERT_EQUAL_INT(0, easytier_rpc_get_endpoint(endpoint, sizeof(endpoint), &nat_ok));
    TEST_ASSERT_EQUAL_STRING("tcp://1.2.3.4:11010", endpoint);
    TEST_ASSERT_EQUAL_INT(1, nat_ok);
    TEST_ASSERT_EQUAL_INT(0, easytier_rpc_init("fail", 15888, g_cli_path));
    TEST_ASSERT_EQUAL_INT(-1, easytier_rpc_get_endpoint(endpoint, sizeof(endpoint), &nat_ok));
    TEST_ASSERT_EQUAL_INT(0, nat_ok);
    TEST_ASSERT_EQUAL_INT(0, easytier_rpc_init("localhost", 15888,
                                             "missing-directory/missing-cli.exe"));
    TEST_ASSERT_EQUAL_INT(-1, easytier_rpc_get_endpoint(endpoint, sizeof(endpoint), &nat_ok));
}

static void test_cli_json_output(void)
{
    const char *invalid_hosts[] = {"invalid", "empty", "oversized"};
    char endpoint[256];
    int nat_ok = 0;
    size_t i;

    TEST_ASSERT_EQUAL_INT(0, easytier_rpc_init("ipv6", 15888, g_cli_path));
    TEST_ASSERT_EQUAL_INT(0, easytier_rpc_get_endpoint(endpoint, sizeof(endpoint), &nat_ok));
    TEST_ASSERT_EQUAL_STRING("tcp://[2001:db8::1]:11010", endpoint);
    TEST_ASSERT_EQUAL_INT(1, nat_ok);
    for(i = 0; i < sizeof(invalid_hosts) / sizeof(invalid_hosts[0]); i++) {
        TEST_ASSERT_EQUAL_INT(0, easytier_rpc_init(invalid_hosts[i], 15888, g_cli_path));
        nat_ok = 1;
        TEST_ASSERT_EQUAL_INT(-1, easytier_rpc_get_endpoint(endpoint, sizeof(endpoint), &nat_ok));
        TEST_ASSERT_EQUAL_INT(0, nat_ok);
    }
}

static void test_nat_announce_eligibility(void)
{
    const char *types[] = {"Public", "FriendlyNAT", "NoPAT", "OpenInternet", "FullCone",
                           "PortRestricted", "Symmetric", "Unknown", "", "fullcone"};
    char host[64];
    char endpoint[256];
    int nat_ok;
    size_t i;

    for(i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        int allowed = i < 5;
        snprintf(host, sizeof(host), "nat-%s", types[i]);
        TEST_ASSERT_EQUAL_INT(0, easytier_rpc_init(host, 15888, g_cli_path));
        nat_ok = !allowed;
        TEST_ASSERT_EQUAL_INT(allowed ? 0 : -1,
                              easytier_rpc_get_endpoint(endpoint, sizeof(endpoint), &nat_ok));
        TEST_ASSERT_EQUAL_INT(allowed, nat_ok);
        if(allowed)
            TEST_ASSERT_EQUAL_STRING("tcp://1.2.3.4:11010", endpoint);
    }
}

int main(int argc, char **argv)
{
    char state_file[4200];
    int result;
    if(argc != 2 || net_init() != 0)
        return 1;
    g_cli_path = argv[1];
#ifdef _WIN32
    {
        char temporary[MAX_PATH];
        if(GetTempPathA(sizeof(temporary), temporary) == 0 ||
           GetTempFileNameA(temporary, "etp", 0, g_state_directory) == 0)
            return 1;
        DeleteFileA(g_state_directory);
        if(!CreateDirectoryA(g_state_directory, NULL))
            return 1;
    }
#else
    strcpy(g_state_directory, "/tmp/peerdisc-platform-XXXXXX");
    if(mkdtemp(g_state_directory) == NULL)
        return 1;
#endif
    UNITY_BEGIN();
    RUN_TEST(test_udp_and_dht_socket_tokens);
    RUN_TEST(test_ipv6_dht_socket_token);
    RUN_TEST(test_http_socket_interface);
    RUN_TEST(test_random_bytes);
    RUN_TEST(test_cli_literal_arguments_and_exit_status);
    RUN_TEST(test_cli_json_output);
    RUN_TEST(test_nat_announce_eligibility);
    result = UNITY_END();
    snprintf(state_file, sizeof(state_file), "%s/dht-state.bin", g_state_directory);
    remove(state_file);
#ifdef _WIN32
    RemoveDirectoryA(g_state_directory);
#else
    rmdir(g_state_directory);
#endif
    net_cleanup();
    return result;
}
