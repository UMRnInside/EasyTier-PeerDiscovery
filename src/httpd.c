// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include "httpd.h"

#include "endpoint.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static net_socket_t g_listen_fd = NET_INVALID_SOCKET;

int httpd_init(const char *listen_host, unsigned short listen_port)
{
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *ai;
    char portstr[8];
    net_socket_t fd = NET_INVALID_SOCKET;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    snprintf(portstr, sizeof(portstr), "%u", listen_port);

    if(getaddrinfo(listen_host, portstr, &hints, &res) != 0)
        return -1;

    for(ai = res; ai != NULL; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if(fd == NET_INVALID_SOCKET)
            continue;
        net_set_option(fd, SOL_SOCKET, SO_REUSEADDR, 1);
        if(bind(fd, ai->ai_addr, ai->ai_addrlen) == 0 && listen(fd, 16) == 0)
            break;
        net_close(fd);
        fd = NET_INVALID_SOCKET;
    }
    freeaddrinfo(res);
    if(fd == NET_INVALID_SOCKET)
        return -1;
    if(net_set_nonblocking(fd) != 0) {
        net_close(fd);
        return -1;
    }
    g_listen_fd = fd;
    return 0;
}

net_socket_t httpd_fd(void)
{
    return g_listen_fd;
}

static void http_send(net_socket_t c, const char *body, size_t bodylen)
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
        send(c, body, (int)bodylen, 0);
}

static void handle_connection(net_socket_t c)
{
    char req[4096];
    char body[512];
    int n;

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
        net_socket_t c = accept(g_listen_fd, NULL, NULL);
        if(c == NET_INVALID_SOCKET)
            break;
        handle_connection(c);
        net_close(c);
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
        rc = net_select(g_listen_fd, &rfds, NULL);
        if(rc < 0) {
            if(net_interrupted())
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
    if(g_listen_fd != NET_INVALID_SOCKET) {
        net_close(g_listen_fd);
        g_listen_fd = NET_INVALID_SOCKET;
    }
    return 0;
}
