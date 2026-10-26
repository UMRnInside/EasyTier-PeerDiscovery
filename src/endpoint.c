// SPDX-License-Identifier: GPL-3.0-or-later

#include "endpoint.h"

#include "uthash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ENDPOINT_URL_MAX 256
#define ENDPOINT_TTL 600

typedef struct endpoint_entry {
    char url[ENDPOINT_URL_MAX];
    time_t last_seen;
    UT_hash_handle hh;
} endpoint_entry_t;

static endpoint_entry_t *g_table = NULL;

int endpoint_init(void)
{
    g_table = NULL;
    return 0;
}

int endpoint_add(const char *url)
{
    endpoint_entry_t *e;
    if(url == NULL)
        return -1;
    HASH_FIND_STR(g_table, url, e);
    if(e == NULL) {
        e = (endpoint_entry_t *)malloc(sizeof(*e));
        if(e == NULL)
            return -1;
        snprintf(e->url, sizeof(e->url), "%s", url);
        HASH_ADD_STR(g_table, url, e);
    }
    e->last_seen = time(NULL);
    return 0;
}

int endpoint_next(char *buf, size_t buflen)
{
    endpoint_entry_t *e, *tmp;
    time_t now = time(NULL);
    size_t count = 0;
    endpoint_entry_t *chosen = NULL;

    HASH_ITER(hh, g_table, e, tmp) {
        if(now - e->last_seen > ENDPOINT_TTL)
            continue;
        count++;
        if(chosen == NULL || (rand() % count) == 0)
            chosen = e;
    }
    if(chosen == NULL)
        return -1;
    if(strlen(chosen->url) >= buflen)
        return -1;
    strcpy(buf, chosen->url);
    return 0;
}

int endpoint_cleanup(void)
{
    endpoint_entry_t *e, *tmp;
    HASH_ITER(hh, g_table, e, tmp) {
        HASH_DEL(g_table, e);
        free(e);
    }
    g_table = NULL;
    return 0;
}