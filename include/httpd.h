// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef HTTPD_H
#define HTTPD_H

#include "net_platform.h"

int httpd_init(const char *listen_host, unsigned short listen_port);
net_socket_t httpd_fd(void);
int httpd_accept(void);
int httpd_run(void);
int httpd_shutdown(void);

#endif
