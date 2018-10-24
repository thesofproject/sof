/*
* debug log converter interface.
*
* Copyright (c) 2018, Intel Corporation.
*
* This program is free software; you can redistribute it and/or modify it
* under the terms and conditions of the GNU General Public License,
* version 2, as published by the Free Software Foundation.
*
* This program is distributed in the hope it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*/
#include <stdio.h>
#include <sof/uapi/logging.h>

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"

static double to_usecs(uint64_t time, double clk)
{
	/* trace timestamp uses CPU system clock at default 25MHz ticks */
	// TODO: support variable clock rates
	return (double)time / clk;
}

struct convert_config {
	const char *out_file;
	const char *in_file;
	FILE *out_fd;
	FILE *in_fd;
	double clock;
	int trace;
#ifdef LOGGER_FORMAT
	const char *ldc_file;
	FILE* ldc_fd;
	int input_std;
	int version_fw;
	char *version_file;
	FILE *version_fd;
#endif
};

int convert(struct convert_config *config);
