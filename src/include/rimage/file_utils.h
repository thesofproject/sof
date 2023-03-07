/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 */

#ifndef __FILE_UTILS_H__
#define __FILE_UTILS_H__

#include <stddef.h>

/**
 * Print file operation error message
 * @param [in]msg error message
 * @param [in]filename File name used to display the error message
 * @param error code
 */
int file_error(const char *msg, const char *filename);

/**
 * Create new file name using output file name as template.
 * @param [out] new_name char[] destination of new file name
 * @param [in] name_size new file name buffer capacity
 * @param [in] template_name File name used as a template for the new name
 * @param [in] new_ext extension of the new file name
 * @param error code, 0 when success
 */
int create_file_name(char *new_name, const size_t name_size, const char *template_name,
		     const char *new_ext);

/**
 * Get file size
 * @param [in] f file handle
 * @param [in] filename File name used to display the error message
 * @param [out] size output for file size
 * @param error code, 0 when success
 */
int get_file_size(FILE *f, const char *filename, size_t *size);

#endif /* __FILE_UTILS_H__ */
