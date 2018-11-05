/*
 * ELF to firmware image creator.
 *
 * Copyright (c) 2015, Intel Corporation.
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "rimage.h"
#include "file_format.h"
#include "manifest.h"

static const struct adsp *machine[] = {
	&machine_byt,
	&machine_cht,
	&machine_bsw,
	&machine_hsw,
	&machine_bdw,
	&machine_apl,
	&machine_cnl,
	&machine_icl,
	&machine_sue,
	&machine_kbl,
	&machine_skl,
};

static void usage(char *name)
{
	fprintf(stdout, "%s:\t -m machine -o outfile -k [key] ELF files\n",
		name);
	fprintf(stdout, "\t -v enable verbose output\n");
	fprintf(stdout, "\t -r enable relocatable ELF files\n");
	fprintf(stdout, "\t -s MEU signing offset\n");
	fprintf(stdout, "\t -p log dictionary outfile\n");
	exit(0);
}

int main(int argc, char *argv[])
{
	struct image image;
	const char *mach = NULL;
	int opt, ret, i, elf_argc = 0;

	memset(&image, 0, sizeof(image));

	while ((opt = getopt(argc, argv, "ho:p:m:vba:s:k:l:r")) != -1) {
		switch (opt) {
		case 'o':
			image.out_file = optarg;
			break;
		case 'p':
			image.ldc_out_file = optarg;
			break;
		case 'm':
			mach = optarg;
			break;
		case 'v':
			image.verbose = 1;
			break;
		case 's':
			image.meu_offset = atoi(optarg);
			break;
		case 'a':
			image.abi = atoi(optarg);
			break;
		case 'k':
			image.key_name = optarg;
			break;
		case 'r':
			image.reloc = 1;
			break;
		case 'h':
			usage(argv[0]);
			break;
		default:
			break;
		}
	}

	elf_argc = optind;

	/* make sure we have an outfile and machine */
	if (image.out_file == NULL || mach == NULL)
		usage(argv[0]);

	if (image.ldc_out_file == NULL)
		image.ldc_out_file = "out.ldc";

	/* find machine */
	for (i = 0; i < ARRAY_SIZE(machine); i++) {
		if (!strcmp(mach, machine[i]->name)) {
			image.adsp = machine[i];
			goto found;
		}
	}
	fprintf(stderr, "error: machine %s not found\n", mach);
	fprintf(stderr, "error: available machines ");
	for (i = 0; i < ARRAY_SIZE(machine); i++)
		fprintf(stderr, "%s, ", machine[i]->name);
	fprintf(stderr, "\n");

	return -EINVAL;

found:

	/* parse input ELF files */
	image.num_modules = argc - elf_argc;
	for (i = elf_argc; i < argc; i++) {
		fprintf(stdout, "\nModule Reading %s\n", argv[i]);
		ret = elf_parse_module(&image, i - elf_argc, argv[i]);
		if (ret < 0)
			goto out;
	}

	/* validate all modules */
	ret = elf_validate_modules(&image);
	if (ret < 0)
		goto out;

	/* open outfile for writing */
	unlink(image.out_file);
	image.out_fd = fopen(image.out_file, "w");
	if (image.out_fd == NULL) {
		fprintf(stderr, "error: unable to open %s for writing %d\n",
			image.out_file, errno);
		ret = -EINVAL;
		goto out;
	}

	/* process and write output */
	if (image.meu_offset)
		ret = image.adsp->write_firmware_meu(&image);
	else
		ret = image.adsp->write_firmware(&image);

	unlink(image.ldc_out_file);
	image.ldc_out_fd = fopen(image.ldc_out_file, "w");
	if (image.ldc_out_fd == NULL) {
		fprintf(stderr, "error: unable to open %s for writing %d\n",
			image.ldc_out_file, errno);
		ret = -EINVAL;
		goto out;
	}
	ret = write_logs_dictionary(&image);
out:
	/* close files */
	if (image.out_fd)
		fclose(image.out_fd);

	if (image.ldc_out_fd)
		fclose(image.ldc_out_fd);

	return ret;
}
