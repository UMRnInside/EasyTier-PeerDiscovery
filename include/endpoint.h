// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ENDPOINT_H
#define ENDPOINT_H

#include <stddef.h>

int endpoint_init(void);
int endpoint_add(const char *url);
int endpoint_next(char *buf, size_t buflen);
int endpoint_cleanup(void);

#endif