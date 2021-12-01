/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Karol Trzcinski	<karolx.trzcinski@linux.intel.com>
 */

#include <stdarg.h>
#include <stdlib.h>

char *log_vasprintf(const char *format, va_list args);

#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
char *log_asprintf(const char *format, ...);

#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
void log_err(const char *fmt, ...);

/* trim whitespaces from string begin */
char *ltrim(char *s);

/* trim whitespaces from string end */
char *rtrim(char *s);

/* trim whitespaces from string begin and end*/
char *trim(char *s);
