/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Karol Trzcinski	<karolx.trzcinski@linux.intel.com>
 */

#include <stdarg.h>
#include <stdlib.h>

char *vasprintf(const char *format, va_list args);
char *asprintf(const char *format, ...);

void log_err(FILE *out_fd, const char *fmt, ...);
