// SPDX-License-Identifier: GPL-3.0-or-later

#include "endpoint.h"
#include "infohash.h"
#include "unity.h"

#include <string.h>

void setUp(void)
{
}

void tearDown(void)
{
}

void test_infohash_deterministic(void)
{
    infohash_t a;
    infohash_t b;
    TEST_ASSERT_EQUAL_INT(0, infohash_derive(&a, "mysecret", "mynet", "tcp"));
    TEST_ASSERT_EQUAL_INT(0, infohash_derive(&b, "mysecret", "mynet", "tcp"));
    TEST_ASSERT_EQUAL_MEMORY(a.hash, b.hash, INFOHASH_LEN);
}

void test_infohash_scheme_separated(void)
{
    infohash_t a;
    infohash_t b;
    infohash_t c;
    TEST_ASSERT_EQUAL_INT(0, infohash_derive(&a, "mysecret", "mynet", "tcp"));
    TEST_ASSERT_EQUAL_INT(0, infohash_derive(&b, "mysecret", "mynet", "udp"));
    TEST_ASSERT_EQUAL_INT(0, infohash_derive(&c, "mysecret", "mynet", "quic"));
    TEST_ASSERT_TRUE(memcmp(a.hash, b.hash, INFOHASH_LEN) != 0);
    TEST_ASSERT_TRUE(memcmp(a.hash, c.hash, INFOHASH_LEN) != 0);
    TEST_ASSERT_TRUE(memcmp(b.hash, c.hash, INFOHASH_LEN) != 0);
}

void test_endpoint_random_selection(void)
{
    static const char *url0 = "tcp://1.2.3.4:11010";
    static const char *url1 = "quic://5.6.7.8:9999";
    char buf[256];
    int seen0 = 0;
    int seen1 = 0;
    int i;

    TEST_ASSERT_EQUAL_INT(0, endpoint_init());
    TEST_ASSERT_EQUAL_INT(0, endpoint_add(url0));
    TEST_ASSERT_EQUAL_INT(0, endpoint_add(url1));

    for(i = 0; i < 20; i++) {
        TEST_ASSERT_EQUAL_INT(0, endpoint_next(buf, sizeof(buf)));
        if(strcmp(buf, url0) == 0)
            seen0 = 1;
        else if(strcmp(buf, url1) == 0)
            seen1 = 1;
        else
            TEST_FAIL();
    }
    TEST_ASSERT_TRUE(seen0);
    TEST_ASSERT_TRUE(seen1);
    endpoint_cleanup();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_infohash_deterministic);
    RUN_TEST(test_infohash_scheme_separated);
    RUN_TEST(test_endpoint_random_selection);
    return UNITY_END();
}
