// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef HTTPD_H
#define HTTPD_H

int httpd_init(const char *listen_host, unsigned short listen_port);
int httpd_fd(void);
int httpd_accept(void);
int httpd_run(void);
int httpd_shutdown(void);

#endif