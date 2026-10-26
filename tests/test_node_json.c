// SPDX-License-Identifier: GPL-3.0-or-later

#include "node_json.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_dual_stack_node(void)
{
    const char *json =
        "{\"peer_id\":4180703577,\"stun_info\":{"
        "\"udp_nat_type\":\"PortRestricted\",\"tcp_nat_type\":\"OpenInternet\","
        "\"last_update_time\":\"1793020992\","
        "\"public_ip\":[\"192.0.2.123\",\"2001:db8:1234::1\"],"
        "\"min_port\":32798,\"max_port\":32798},"
        "\"listeners\":[\"ring://test\",\"tcp://0.0.0.0:11010\","
        "\"udp://0.0.0.0:11010\",\"quic://0.0.0.0:11012\",\"tcp://[::]:11010\","
        "\"udp://[::]:11010\"],"
        "\"config\":\"network_secret = \\\"not-for-logging\\\"\\n\","
        "\"ip_list\":{\"public_ipv4\":{\"addr\":3221226107},"
        "\"public_ipv6\":{\"part1\":536939960,\"part2\":305397760,\"part4\":1}}}";
    node_json_t node;

    TEST_ASSERT_EQUAL_INT(0, node_json_parse(json, strlen(json), &node));
    TEST_ASSERT_EQUAL_STRING("PortRestricted", node.udp_nat);
    TEST_ASSERT_EQUAL_STRING("OpenInternet", node.tcp_nat);
    TEST_ASSERT_EQUAL_STRING("192.0.2.123", node.public_ipv4);
    TEST_ASSERT_EQUAL_STRING("2001:db8:1234::1", node.public_ipv6);
    TEST_ASSERT_EQUAL_STRING("tcp://0.0.0.0:11010", node.tcp4_listener);
    TEST_ASSERT_EQUAL_STRING("tcp://[::]:11010", node.tcp6_listener);
    TEST_ASSERT_EQUAL_STRING("udp://0.0.0.0:11010", node.udp4_listener);
    TEST_ASSERT_EQUAL_STRING("udp://[::]:11010", node.udp6_listener);
    TEST_ASSERT_EQUAL_STRING("quic://0.0.0.0:11012", node.quic4_listener);
}

static void test_ipv4_node(void)
{
    const char *json =
        "{\"stun_info\":{\"udp_nat_type\":\"NoPAT\",\"tcp_nat_type\":\"NoPAT\","
        "\"public_ip\":[\"198.51.100.42\"],\"min_port\":52280,\"max_port\":52280},"
        "\"listeners\":[\"ring://test\",\"tcp://0.0.0.0:11010\",\"udp://[::]:11010\"]}";
    node_json_t node;

    TEST_ASSERT_EQUAL_INT(0, node_json_parse(json, strlen(json), &node));
    TEST_ASSERT_EQUAL_STRING("NoPAT", node.udp_nat);
    TEST_ASSERT_EQUAL_STRING("NoPAT", node.tcp_nat);
    TEST_ASSERT_EQUAL_STRING("198.51.100.42", node.public_ipv4);
    TEST_ASSERT_EQUAL_STRING("", node.public_ipv6);
    TEST_ASSERT_EQUAL_STRING("", node.udp4_listener);
    TEST_ASSERT_EQUAL_STRING("udp://[::]:11010", node.udp6_listener);
}

static void test_reordered_compact_and_escaped_json(void)
{
    const char *json =
        " {\"listeners\":[\"ring:\\/\\/test\",\"tcp:\\/\\/[::]:11010\"],"
        "\"ignored\":[null,true,false,-1.25e+3,{\"public_ip\":[\"1.2.3.4\"]}],"
        "\"stun_info\":{\"public_ip\":[\"invalid\",\"2001:db8::1\"],"
        "\"udp_nat_type\":\"\\u0050ublic\"}}\r\n";
    node_json_t node;

    TEST_ASSERT_EQUAL_INT(0, node_json_parse(json, strlen(json), &node));
    TEST_ASSERT_EQUAL_STRING("Public", node.udp_nat);
    TEST_ASSERT_EQUAL_STRING("", node.tcp_nat);
    TEST_ASSERT_EQUAL_STRING("", node.public_ipv4);
    TEST_ASSERT_EQUAL_STRING("2001:db8::1", node.public_ipv6);
    TEST_ASSERT_EQUAL_STRING("tcp://[::]:11010", node.tcp6_listener);
}

static void test_invalid_json(void)
{
    const char *invalid[] = {
        "", "{} trailing", "[]", "{", "{\"stun_info\":{},\"listeners\":[,]}",
        "{\"stun_info\":{},\"listeners\":[\"tcp://0.0.0.0:11010\",]}",
        "{\"stun_info\":{},\"listeners\":[],}",
        "{\"stun_info\":null,\"listeners\":[]}",
        "{\"stun_info\":{},\"listeners\":{}}",
        "{\"stun_info\":{\"udp_nat_type\":1},\"listeners\":[]}",
        "{\"stun_info\":{\"public_ip\":\"1.2.3.4\"},\"listeners\":[]}",
        "{\"stun_info\":{},\"listeners\":[true]}",
        "{\"stun_info\":{},\"listeners\":[],\"x\":01}",
        "{\"stun_info\":{},\"listeners\":[],\"x\":1e}",
        "{\"stun_info\":{},\"listeners\":[],\"x\":\"\\uD800\"}",
        "{\"stun_info\":{},\"listeners\":[],\"x\":\"\\q\"}"
    };
    size_t i;
    node_json_t node;

    for(i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        TEST_ASSERT_EQUAL_INT(-1, node_json_parse(invalid[i], strlen(invalid[i]), &node));
        TEST_ASSERT_EQUAL_STRING("", node.udp_nat);
        TEST_ASSERT_EQUAL_STRING("", node.public_ipv4);
    }
}

static void test_missing_fields(void)
{
    const char *json = "{\"stun_info\":{},\"listeners\":[\"ring://test\"]}";
    node_json_t node;
    TEST_ASSERT_EQUAL_INT(0, node_json_parse(json, strlen(json), &node));
    TEST_ASSERT_EQUAL_STRING("", node.udp_nat);
    TEST_ASSERT_EQUAL_STRING("", node.public_ipv4);
    TEST_ASSERT_EQUAL_STRING("", node.tcp4_listener);
    TEST_ASSERT_EQUAL_STRING("", node.tcp6_listener);
    TEST_ASSERT_EQUAL_STRING("", node.udp4_listener);
}

static void test_parser_limits(void)
{
    char deep[200];
    char *many = malloc(20032);
    const char embedded_nul[] = "{\"stun_info\":{},\"listeners\":[]}\0{}";
    const char *long_nat =
        "{\"stun_info\":{\"udp_nat_type\":\"12345678901234567890123456789012345\"},"
        "\"listeners\":[]}";
    node_json_t node;
    size_t i;
    int rc;

    TEST_ASSERT_NOT_NULL(many);
    memset(deep, '[', 64);
    memset(deep + 64, ']', 64);
    TEST_ASSERT_EQUAL_INT(-1, node_json_parse(deep, 128, &node));
    strcpy(many, "{\"stun_info\":{},\"listeners\":[],\"x\":[");
    for(i = 0; i < 9000; i++)
        strcat(many, i == 0 ? "0" : ",0");
    strcat(many, "]}");
    rc = node_json_parse(many, strlen(many), &node);
    free(many);
    TEST_ASSERT_EQUAL_INT(-1, rc);
    TEST_ASSERT_EQUAL_INT(-1, node_json_parse(embedded_nul, sizeof(embedded_nul) - 1, &node));
    TEST_ASSERT_EQUAL_INT(-1, node_json_parse(long_nat, strlen(long_nat), &node));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_dual_stack_node);
    RUN_TEST(test_ipv4_node);
    RUN_TEST(test_reordered_compact_and_escaped_json);
    RUN_TEST(test_invalid_json);
    RUN_TEST(test_missing_fields);
    RUN_TEST(test_parser_limits);
    return UNITY_END();
}
