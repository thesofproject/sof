/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Bartosz Kokoszko	<bartoszx.kokoszko@linux.intel.com>
 *	   Artur Kloniecki	<arturx.kloniecki@linux.intel.com>
 */

/*
 * debug log converter interface.
 */

#include <stdio.h>
#include <ipc/info.h>
#include <rimage/file_format.h>

#define KNRM	"\x1B[0m"
#define KRED	"\x1B[31m"
#define KGRN	"\x1B[32m"
#define KYEL	"\x1B[33m"
#define KBLU	"\x1B[34m"

struct convert_config {
	const char *out_file;
	const char *in_file;
	FILE *out_fd;
	FILE *in_fd;
	double clock;
	int trace;
	const char *ldc_file;
	FILE* ldc_fd;
	int input_std;
	int version_fw;
	char *version_file;
	FILE *version_fd;
	int use_colors;
	int serial_fd;
	int raw_output;
	struct snd_sof_uids_header *uids_dict;
};

int convert(struct convert_config *config);
