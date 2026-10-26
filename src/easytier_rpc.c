// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include "easytier_rpc.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static char g_rpc_host[128] = "127.0.0.1";
static unsigned short g_rpc_port = 15888;
static char g_cli_path[4096] = "easytier-cli";
static int g_custom_cli_path;

int easytier_rpc_init(const char *rpc_host, unsigned short rpc_port,
                      const char *cli_path)
{
    const char *executable = cli_path != NULL ? cli_path : "easytier-cli";

    if(rpc_host == NULL || executable[0] == '\0' ||
       strlen(executable) >= sizeof(g_cli_path))
        return -1;
    strcpy(g_cli_path, executable);
    g_custom_cli_path = cli_path != NULL;
    snprintf(g_rpc_host, sizeof(g_rpc_host), "%s", rpc_host);
    g_rpc_port = rpc_port;
    return 0;
}

static int md_field(const char *line, const char *label, char *out, size_t outlen)
{
    const char *p;
    const char *end;
    size_t n;
    p = strstr(line, label);
    if(p == NULL)
        return -1;
    p += strlen(label);
    p = strchr(p, '|');
    if(p == NULL)
        return -1;
    p++;
    while(*p == ' ')
        p++;
    end = strchr(p, '|');
    n = end != NULL ? (size_t)(end - p) : strlen(p);
    while(n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\n' || p[n - 1] == '\r'))
        n--;
    if(n == 0 || n >= outlen)
        return -1;
    memcpy(out, p, n);
    out[n] = '\0';
    return 0;
}

static int starts_with(const char *s, const char *prefix)
{
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static unsigned short listener_port(const char *listener)
{
    const char *p = strrchr(listener, ':');
    if(p == NULL || p[1] == '\0')
        return 0;
    return (unsigned short)strtoul(p + 1, NULL, 10);
}

static int wait_for_cli(pid_t pid);

static FILE *spawn_easytier_cli(pid_t *pid_out)
{
    char rpc_target[160];
    int output_pipe[2];
    pid_t pid;
    FILE *stream;

    snprintf(rpc_target, sizeof(rpc_target), "%s:%u", g_rpc_host, g_rpc_port);
    if(pipe(output_pipe) != 0)
        return NULL;

    pid = fork();
    if(pid < 0) {
        close(output_pipe[0]);
        close(output_pipe[1]);
        return NULL;
    }
    if(pid == 0) {
        char *const argv[] = {
            g_cli_path,
            (char *)"-p",
            rpc_target,
            (char *)"node",
            NULL,
        };
        int devnull;

        close(output_pipe[0]);
        if(dup2(output_pipe[1], STDOUT_FILENO) < 0)
            _exit(127);
        close(output_pipe[1]);

        devnull = open("/dev/null", O_WRONLY);
        if(devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        if(g_custom_cli_path)
            execv(argv[0], argv);
        else
            execvp(argv[0], argv);
        _exit(127);
    }

    close(output_pipe[1]);
    stream = fdopen(output_pipe[0], "r");
    if(stream == NULL) {
        close(output_pipe[0]);
        wait_for_cli(pid);
        return NULL;
    }
    *pid_out = pid;
    return stream;
}

static int wait_for_cli(pid_t pid)
{
    int status;

    while(waitpid(pid, &status, 0) < 0) {
        if(errno != EINTR)
            return -1;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

int easytier_rpc_get_endpoint(char *buf, size_t buflen, int *nat_ok)
{
    char line[512];
    char pubv4[64] = "";
    char pubv6[64] = "";
    char nattr[32] = "";
    char listener[128] = "";
    FILE *f;
    pid_t pid;
    int cli_status;
    unsigned short port;

    if(nat_ok != NULL)
        *nat_ok = 0;

    f = spawn_easytier_cli(&pid);
    if(f == NULL)
        return -1;

    while(fgets(line, sizeof(line), f) != NULL) {
        if(listener[0] == '\0' && md_field(line, "Listener", listener, sizeof(listener)) == 0 &&
           starts_with(listener, "ring"))
            listener[0] = '\0';
        if(pubv4[0] == '\0')
            md_field(line, "Public IPv4", pubv4, sizeof(pubv4));
        if(pubv6[0] == '\0')
            md_field(line, "Public IPv6", pubv6, sizeof(pubv6));
        if(nattr[0] == '\0')
            md_field(line, "UDP Stun Type", nattr, sizeof(nattr));
    }
    fclose(f);
    cli_status = wait_for_cli(pid);
    if(cli_status != 0)
        return -1;

    if(nattr[0] == '\0' ||
       (strcmp(nattr, "Public") != 0 && strcmp(nattr, "FriendlyNAT") != 0))
        return -1;
    if(pubv4[0] == '\0' && pubv6[0] == '\0')
        return -1;
    if(listener[0] == '\0')
        return -1;
    port = listener_port(listener);
    if(port == 0)
        return -1;

    if(pubv4[0] != '\0')
        snprintf(buf, buflen, "tcp://%s:%u", pubv4, port);
    else
        snprintf(buf, buflen, "tcp://[%s]:%u", pubv6, port);

    if(nat_ok != NULL)
        *nat_ok = 1;
    return 0;
}

int easytier_rpc_cleanup(void)
{
    return 0;
}
