// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NET_PLATFORM_H
#define NET_PLATFORM_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET net_socket_t;
typedef int net_socklen_t;
#define NET_INVALID_SOCKET INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
typedef int net_socket_t;
typedef socklen_t net_socklen_t;
#define NET_INVALID_SOCKET (-1)
#endif

int net_init(void);
void net_cleanup(void);
void net_close(net_socket_t socket);
int net_set_nonblocking(net_socket_t socket);
int net_interrupted(void);
int net_select(net_socket_t highest, fd_set *readfds, struct timeval *timeout);
int net_set_option(net_socket_t socket, int level, int option, int value);

#endif
