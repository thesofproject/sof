// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018-2023 Intel Corporation. All rights reserved.
//
// Author: Adrian Warecki <adrian.warecki@intel.com>

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include <rimage/file_utils.h>

int file_error(const char *msg, const char *filename)
{
	int code = errno;
	char sys_msg[256];

	strerror_r(code, sys_msg, sizeof(sys_msg));

	fprintf(stderr, "%s:\terror: %s. %s (errno = %d)\n", filename, msg, sys_msg, code);
	return -code;
}

int create_file_name(char *new_name, const size_t name_size, const char *template_name,
		   const char *new_ext)
{
	int len;

	assert(new_name);

	len = snprintf(new_name, name_size, "%s.%s", template_name, new_ext);
	if (len >= name_size) {
		fprintf(stderr, "error: output file name too long\n");
		return -ENAMETOOLONG;
	}

	unlink(new_name);

	return 0;
}

int get_file_size(FILE *f, const char* filename, size_t *size)
{
	int ret;
	long pos;
	assert(size);

	/* get file size */
	ret = fseek(f, 0, SEEK_END);
	if (ret)
		return file_error("unable to seek eof", filename);

	pos = ftell(f);
	if (pos < 0)
		return file_error("unable to get file size", filename);

	ret = fseek(f, 0, SEEK_SET);
	if (ret)
		return file_error("unable to seek set", filename);

	*size = pos;
	return 0;
}
