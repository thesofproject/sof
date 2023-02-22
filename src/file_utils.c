// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018-2023 Intel Corporation. All rights reserved.
//
// Author: Adrian Warecki <adrian.warecki@intel.com>

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <rimage/file_utils.h>

/**
 * Create new file name using output file name as template.
 * @param [out] new_name char[] destination of new file name
 * @param [in] name_size new file name buffer capacity
 * @param [in] template_name File name used as a template for the new name
 * @param [in] new_ext extension of the new file name
 * @param error code, 0 when success
 */
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

/**
 * Get file size
 * @param [in] f file handle
 * @param [in] filename File name used to display the error message
 * @param [out] size output for file size
 * @param error code, 0 when success
 */
int get_file_size(FILE *f, const char* filename, size_t *size)
{
	int ret;

	assert(size);

	/* get file size */
	ret = fseek(f, 0, SEEK_END);
	if (ret < 0) {
		fprintf(stderr, "error: unable to seek eof %s %d\n", filename, errno);
		return -errno;
	}

	*size = ftell(f);
	if (*size < 0) {
		fprintf(stderr, "error: unable to get file size for %s %d\n", filename, errno);
		return -errno;
	}

	ret = fseek(f, 0, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "error: unable to seek set %s %d\n", filename, errno);
		return -errno;
	}

	return 0;
}
