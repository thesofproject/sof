// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Adrian Bonislawski <adrian.bonislawski@intel.com>

/*
 * Probes will extract data for several probe points in one stream
 * with extra headers. This app will read the resulting file,
 * strip the headers and create wave files for each extracted buffer.
 *
 * Usage to parse data and create wave files: ./sof-probes -p data.bin
 *
 */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "probes_demux.h"

#define APP_NAME "sof-probes"

static void usage(void)
{
	fprintf(stdout, "Usage %s <option(s)> <buffer_id/file>\n\n", APP_NAME);
	fprintf(stdout, "%s:\t -p file\tParse extracted file\n\n", APP_NAME);
	fprintf(stdout, "%s:\t -h \t\tHelp, usage info\n", APP_NAME);
	exit(0);
}

void parse_data(char *file_in)
{
	struct dma_frame_parser *p = parser_init();
	FILE *fd_in;
	uint8_t *data;
	size_t len;
	int ret;

	if (!p) {
		fprintf(stderr, "parser_init() failed\n");
		exit(1);
	}

	fd_in = fopen(file_in, "rb");
	if (!fd_in) {
		fprintf(stderr, "error: unable to open file %s, error %d\n",
			file_in, errno);
		exit(0);
	}

	do {
		parser_fetch_free_buffer(p, &data, &len);
		len = fread(data, 1, len, fd_in);
		ret = parser_parse_data(p, len);
	} while (!ret && !feof(fd_in));
}

int main(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "hp:")) != -1) {
		switch (opt) {
		case 'p':
			parse_data(optarg);
			break;
		case 'h':
		default:
			usage();
		}
	}

	return 0;
}
