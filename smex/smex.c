// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "elf.h"
#include "ldc.h"
#include "smex.h"

static void usage(char *name)
{
	fprintf(stdout, "%s:\t in_file\n", name);
	fprintf(stdout, "\t -l log dictionary outfile\n");
	fprintf(stdout, "\t -v enable verbose output\n");
	fprintf(stdout, "\t -h this help message\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	struct image image;
	int opt, ret;

	memset(&image, 0, sizeof(image));

	while ((opt = getopt(argc, argv, "hl:v")) != -1) {
		switch (opt) {
		case 'l':
			image.ldc_out_file = optarg;
			break;
		case 'v':
			image.verbose = true;
			break;
		case 'h':
			usage(argv[0]);
			break;
		default:
			break;
		}
	}

	/* make sure we have an outfile */
	if (!image.ldc_out_file)
		image.ldc_out_file = "out.ldc";

	/* make sure we have an input ELF file */
	if (argc - optind != 1) {
		usage(argv[0]);
		return -EINVAL;
	}

	/* read source elf file */
	ret = elf_read_module(&image.module, argv[optind], image.verbose);
	if (ret < 0)
		goto out;

	/* open outfile for writing */
	unlink(image.ldc_out_file);
	image.ldc_out_fd = fopen(image.ldc_out_file, "wb");
	if (!image.ldc_out_fd) {
		fprintf(stderr, "error: unable to open %s for writing %d\n",
			image.ldc_out_file, errno);
		ret = -EINVAL;
		goto out;
	}

	/* write dictionaries */
	ret = write_dictionaries(&image, &image.module);
	if (ret) {
		fprintf(stderr, "error: unable to write dictionaries, %d\n",
			ret);
		/* Don't corrupt the build with an empty or incomplete output */
		unlink(image.ldc_out_file);
		goto out;
	}

out:
	/* close files */
	if (image.ldc_out_fd)
		fclose(image.ldc_out_fd);

	elf_free_module(&image.module);

	return ret;
}
