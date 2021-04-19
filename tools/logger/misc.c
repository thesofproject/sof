/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Karol Trzcinski	<karolx.trzcinski@linux.intel.com>
 */

#include "convert.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *vasprintf(const char *format, va_list args)
{
	va_list args_copy;
	int size;
	char localbuf[1];
	char *result;

	va_copy(args_copy, args);
	size = vsnprintf(localbuf, 1, format, args_copy);
	va_end(args_copy);

	result = malloc(size + 1);
	if (result)
		vsnprintf(result, size + 1, format, args);
	return result;
}

char *asprintf(const char *format, ...)
{
	va_list args;
	char *result;

	va_start(args, format);
	result = vasprintf(format, args);
	va_end(args);

	return result;
}

extern struct convert_config *global_config;

/** Prints 1. once to stderr. 2. a second time to the global out_fd if
 * out_fd is neither stderr nor stdout (because the -o option was used).
 */
void log_err(const char *fmt, ...)
{
	FILE *out_fd = global_config ? global_config->out_fd : NULL;
	static const char prefix[] = "error: ";
	ssize_t needed_size;
	va_list args, args_alloc;
	char *buff;

	va_start(args, fmt);

	/* Print into buff first, then outputs buff twice */
	va_copy(args_alloc, args);
	needed_size = vsnprintf(NULL, 0, fmt, args_alloc) + 1;
	buff = malloc(needed_size);
	va_end(args_alloc);

	if (buff) {
		vsprintf(buff, fmt, args);
		fprintf(stderr, "%s%s", prefix, buff);

		/* take care about out_fd validity and duplicated logging */
		if (out_fd && out_fd != stderr && out_fd != stdout) {
			fprintf(out_fd, "%s%s", prefix, buff);
			fflush(out_fd);
		}
		free(buff);
	} else {
		fprintf(stderr, "%s", prefix);
		vfprintf(stderr, fmt, args);
	}

	va_end(args);
}

/* trim whitespaces from string begin */
char *ltrim(char *s)
{
	while (isspace(*s))
		s++;
	return s;
}

/* trim whitespaces from string end */
char *rtrim(char *s)
{
	char *ptr = s + strlen(s) - 1;

	while (ptr >= s && isspace(*ptr)) {
		*ptr = '\0';
		--ptr;
	}
	return s;
}

/* trim whitespaces from string begin and end*/
char *trim(char *s)
{
	return ltrim(rtrim(s));
}
