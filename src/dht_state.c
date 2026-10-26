// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "dht_state.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <shlobj.h>
#include <windows.h>
#define PATH_SEPARATOR '\\'
#define mkdir_one(path) _mkdir(path)
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define PATH_SEPARATOR '/'
#define mkdir_one(path) mkdir(path, 0700)
#endif

#define STATE_FILENAME "dht-state.bin"
#define STATE_MAGIC "ETDHTS1"
#define STATE_MAGIC_LEN 7
#define STATE_HEADER_LEN 12

static char g_directory[4096];

int dht_state_set_directory(const char *directory)
{
    if(directory == NULL) {
        g_directory[0] = '\0';
        return 0;
    }
    if(directory[0] == '\0' || strlen(directory) >= sizeof(g_directory))
        return -1;
    strcpy(g_directory, directory);
    return 0;
}

int dht_state_directory(char *buf, size_t buflen)
{
    size_t len;

    if(g_directory[0] == '\0')
        return dht_state_default_directory(buf, buflen);
    len = strlen(g_directory);
    if(len >= buflen)
        return -1;
    memcpy(buf, g_directory, len + 1);
    return 0;
}

static int path_join(char *buf, size_t buflen, const char *dir, const char *name)
{
    size_t dirlen = strlen(dir);
    int needs_separator = dirlen > 0 && dir[dirlen - 1] != PATH_SEPARATOR;
    int n = snprintf(buf, buflen, "%s%s%s", dir,
                     needs_separator ? (PATH_SEPARATOR == '/' ? "/" : "\\") : "",
                     name);
    return n < 0 || (size_t)n >= buflen ? -1 : 0;
}

int dht_state_default_directory(char *buf, size_t buflen)
{
#ifdef _WIN32
    PWSTR wide_path = NULL;
    char base[4096];
    int n;

    if(SHGetKnownFolderPath(&FOLDERID_LocalAppDataLow, KF_FLAG_CREATE,
                            NULL, &wide_path) != S_OK)
        return -1;
    n = WideCharToMultiByte(CP_ACP, 0, wide_path, -1, base, (int)sizeof(base),
                            NULL, NULL);
    CoTaskMemFree(wide_path);
    if(n <= 0)
        return -1;
    return path_join(buf, buflen, base, "easytier-peerdiscovery");
#else
    const char *base = getenv("XDG_STATE_HOME");
    const char *home;
    int n;

    if(base != NULL && base[0] == '/')
        return path_join(buf, buflen, base, "easytier-peerdiscovery");
    home = getenv("HOME");
    if(home == NULL || home[0] == '\0')
        return -1;
    n = snprintf(buf, buflen, "%s/.local/state/easytier-peerdiscovery", home);
    if(n < 0 || (size_t)n >= buflen)
        return -1;
    return 0;
#endif
}

static int create_directory_tree(const char *path)
{
#ifdef _WIN32
    if(mkdir_one(path) != 0 && errno != EEXIST)
        return -1;
    return 0;
#else
    char copy[4096];
    char *p;

    if(strlen(path) >= sizeof(copy))
        return -1;
    strcpy(copy, path);
    for(p = copy + 1; *p != '\0'; p++) {
        if(*p != '/')
            continue;
        *p = '\0';
        if(mkdir_one(copy) != 0 && errno != EEXIST)
            return -1;
        *p = PATH_SEPARATOR;
    }
    if(mkdir_one(copy) != 0 && errno != EEXIST)
        return -1;
    return 0;
#endif
}

static int read_exact(FILE *f, void *buf, size_t len)
{
    return fread(buf, 1, len, f) == len ? 0 : -1;
}

static int write_exact(FILE *f, const void *buf, size_t len)
{
    return fwrite(buf, 1, len, f) == len ? 0 : -1;
}

static int replace_file(const char *source, const char *destination)
{
#ifdef _WIN32
    return MoveFileExA(source, destination,
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : -1;
#else
    return rename(source, destination);
#endif
}

static unsigned long process_id(void)
{
#ifdef _WIN32
    return (unsigned long)GetCurrentProcessId();
#else
    return (unsigned long)getpid();
#endif
}

static FILE *open_temporary_file(const char *path)
{
#ifdef _WIN32
    return fopen(path, "wb");
#else
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    FILE *f;

    if(fd < 0 && errno == EEXIST) {
        if(remove(path) != 0)
            return NULL;
        fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    }
    if(fd < 0)
        return NULL;
    f = fdopen(fd, "wb");
    if(f == NULL) {
        close(fd);
        remove(path);
    }
    return f;
#endif
}

static uint16_t read_u16(const unsigned char value[2])
{
    return (uint16_t)((value[0] << 8) | value[1]);
}

static void write_u16(unsigned char value[2], uint16_t number)
{
    value[0] = (unsigned char)(number >> 8);
    value[1] = (unsigned char)number;
}

int dht_state_load(dht_state_t *state)
{
    char directory[4096];
    char path[4096];
    unsigned char header[STATE_HEADER_LEN];
    uint16_t count4;
    uint16_t count6;
    FILE *f;
    int i;

    if(state == NULL)
        return -1;
    memset(state, 0, sizeof(*state));
    if(dht_state_directory(directory, sizeof(directory)) != 0 ||
       path_join(path, sizeof(path), directory, STATE_FILENAME) != 0)
        return -1;
    f = fopen(path, "rb");
    if(f == NULL)
        return errno == ENOENT ? 0 : -1;
    if(read_exact(f, header, sizeof(header)) != 0 ||
       memcmp(header, STATE_MAGIC, STATE_MAGIC_LEN) != 0 || header[7] != 1) {
        fclose(f);
        return -1;
    }
    count4 = read_u16(header + 8);
    count6 = read_u16(header + 10);
    if(count4 > DHT_STATE_MAX_NODES || count6 > DHT_STATE_MAX_NODES ||
       read_exact(f, state->node_id, sizeof(state->node_id)) != 0) {
        fclose(f);
        return -1;
    }
    state->has_node_id = 1;
    for(i = 0; i < count4; i++) {
        struct sockaddr_in *addr = &state->nodes4[i];
        memset(addr, 0, sizeof(*addr));
        addr->sin_family = AF_INET;
        if(read_exact(f, &addr->sin_addr, 4) != 0 ||
           read_exact(f, &addr->sin_port, 2) != 0) {
            fclose(f);
            return -1;
        }
    }
    for(i = 0; i < count6; i++) {
        struct sockaddr_in6 *addr = &state->nodes6[i];
        memset(addr, 0, sizeof(*addr));
        addr->sin6_family = AF_INET6;
        if(read_exact(f, &addr->sin6_addr, 16) != 0 ||
           read_exact(f, &addr->sin6_port, 2) != 0) {
            fclose(f);
            return -1;
        }
    }
    state->nodes4_len = count4;
    state->nodes6_len = count6;
    fclose(f);
    return 1;
}

int dht_state_save(const dht_state_t *state)
{
    char directory[4096];
    char path[4096];
    char temporary[4096];
    unsigned char header[STATE_HEADER_LEN];
    FILE *f;
    int i;
    int ok = 0;
    int n;

    if(state == NULL || !state->has_node_id || state->nodes4_len < 0 ||
       state->nodes4_len > DHT_STATE_MAX_NODES || state->nodes6_len < 0 ||
       state->nodes6_len > DHT_STATE_MAX_NODES)
        return -1;
    if(dht_state_directory(directory, sizeof(directory)) != 0 ||
       create_directory_tree(directory) != 0 ||
       path_join(path, sizeof(path), directory, STATE_FILENAME) != 0)
        return -1;
    n = snprintf(temporary, sizeof(temporary), "%s.tmp.%lu", path, process_id());
    if(n < 0 || (size_t)n >= sizeof(temporary))
        return -1;

    memset(header, 0, sizeof(header));
    memcpy(header, STATE_MAGIC, STATE_MAGIC_LEN);
    header[7] = 1;
    write_u16(header + 8, (uint16_t)state->nodes4_len);
    write_u16(header + 10, (uint16_t)state->nodes6_len);

    f = open_temporary_file(temporary);
    if(f == NULL)
        return -1;
    if(write_exact(f, header, sizeof(header)) != 0 ||
       write_exact(f, state->node_id, sizeof(state->node_id)) != 0)
        goto done;
    for(i = 0; i < state->nodes4_len; i++) {
        if(write_exact(f, &state->nodes4[i].sin_addr, 4) != 0 ||
           write_exact(f, &state->nodes4[i].sin_port, 2) != 0)
            goto done;
    }
    for(i = 0; i < state->nodes6_len; i++) {
        if(write_exact(f, &state->nodes6[i].sin6_addr, 16) != 0 ||
           write_exact(f, &state->nodes6[i].sin6_port, 2) != 0)
            goto done;
    }
    if(fflush(f) != 0)
        goto done;
#ifndef _WIN32
    if(fsync(fileno(f)) != 0)
        goto done;
#endif
    ok = 1;

done:
    if(fclose(f) != 0)
        ok = 0;
    if(!ok || replace_file(temporary, path) != 0) {
        remove(temporary);
        return -1;
    }
#ifndef _WIN32
    chmod(path, 0600);
#endif
    return 0;
}
