// SPDX-License-Identifier: GPL-3.0-or-later

#include "net_platform.h"

#ifndef _WIN32
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#endif

int net_init(void)
{
#ifdef _WIN32
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0 ? 0 : -1;
#else
    return 0;
#endif
}

void net_cleanup(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

void net_close(net_socket_t socket)
{
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

int net_set_nonblocking(net_socket_t socket)
{
#ifdef _WIN32
    u_long enabled = 1;
    return ioctlsocket(socket, FIONBIO, &enabled);
#else
    int flags = fcntl(socket, F_GETFL, 0);
    return flags < 0 ? -1 : fcntl(socket, F_SETFL, flags | O_NONBLOCK);
#endif
}

int net_interrupted(void)
{
#ifdef _WIN32
    return WSAGetLastError() == WSAEINTR;
#else
    return errno == EINTR;
#endif
}

int net_select(net_socket_t highest, fd_set *readfds, struct timeval *timeout)
{
#ifdef _WIN32
    (void)highest;
    return select(0, readfds, NULL, NULL, timeout);
#else
    return select(highest + 1, readfds, NULL, NULL, timeout);
#endif
}

int net_set_option(net_socket_t socket, int level, int option, int value)
{
    return setsockopt(socket, level, option, (const char *)&value, sizeof(value));
}
