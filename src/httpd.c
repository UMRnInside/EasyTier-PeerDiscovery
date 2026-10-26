// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include "httpd.h"

#include "endpoint.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

static int g_listen_fd = -1;

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags < 0)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int httpd_init(const char *listen_host, unsigned short listen_port)
{
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *ai;
    char portstr[8];
    int fd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    snprintf(portstr, sizeof(portstr), "%u", listen_port);

    if(getaddrinfo(listen_host, portstr, &hints, &res) != 0)
        return -1;

    for(ai = res; ai != NULL; ai = ai->ai_next) {
        int one = 1;
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if(fd < 0)
            continue;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        if(bind(fd, ai->ai_addr, ai->ai_addrlen) == 0 && listen(fd, 16) == 0)
            break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if(fd < 0)
        return -1;
    set_nonblocking(fd);
    g_listen_fd = fd;
    return 0;
}

int httpd_fd(void)
{
    return g_listen_fd;
}

static void http_send(int c, const char *body, size_t bodylen)
{
    char head[256];
    int n = snprintf(head, sizeof(head),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: text/plain\r\n"
                     "Content-Length: %zu\r\n"
                     "Connection: close\r\n\r\n",
                     bodylen);
    send(c, head, n, 0);
    if(bodylen > 0)
        send(c, body, bodylen, 0);
}

static void handle_connection(int c)
{
    char req[4096];
    char body[512];
    ssize_t n;

    n = recv(c, req, sizeof(req) - 1, 0);
    if(n <= 0)
        return;
    req[n] = '\0';

    if(strncmp(req, "GET ", 4) != 0)
        return;

    if(endpoint_next(body, sizeof(body)) == 0) {
        size_t blen = strlen(body);
        body[blen++] = '\n';
        http_send(c, body, blen);
    } else {
        http_send(c, NULL, 0);
    }
}

int httpd_accept(void)
{
    for(;;) {
        int c = accept(g_listen_fd, NULL, NULL);
        if(c < 0)
            break;
        handle_connection(c);
        close(c);
    }
    return 0;
}

int httpd_run(void)
{
    for(;;) {
        fd_set rfds;
        int rc;
        FD_ZERO(&rfds);
        FD_SET(g_listen_fd, &rfds);
        rc = select(g_listen_fd + 1, &rfds, NULL, NULL, NULL);
        if(rc < 0) {
            if(errno == EINTR)
                continue;
            break;
        }
        if(FD_ISSET(g_listen_fd, &rfds))
            httpd_accept();
    }
    return 0;
}

int httpd_shutdown(void)
{
    if(g_listen_fd >= 0) {
        close(g_listen_fd);
        g_listen_fd = -1;
    }
    return 0;
}