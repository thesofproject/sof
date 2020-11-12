// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2015 Intel Corporation. All rights reserved.

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include <rimage/adsp_config.h>
#include <rimage/ext_manifest_gen.h>
#include <rimage/rimage.h>
#include <rimage/manifest.h>


static void usage(char *name)
{
	fprintf(stdout, "%s:\t -c adsp_desc -o outfile -k [key] ELF files\n",
		name);
	fprintf(stdout, "%s:\t -c adsp_desc -y infile -k [key]\n",
			name);
	fprintf(stdout, "\t -v enable verbose output\n");
	fprintf(stdout, "\t -r enable relocatable ELF files\n");
	fprintf(stdout, "\t -s MEU signing offset\n");
	fprintf(stdout, "\t -i set IMR type\n");
	fprintf(stdout, "\t -x set xcc module offset\n");
	fprintf(stdout, "\t -f firmware version = x.y\n");
	fprintf(stdout, "\t -b build version\n");
	fprintf(stdout, "\t -e build extended manifest\n");
	fprintf(stdout, "\t -y verify signed file\n");
	exit(0);
}

int main(int argc, char *argv[])
{
	struct image image;
	struct adsp *heap_adsp;
	const char *adsp_config = NULL;
	int opt, ret, i, first_non_opt;
	int imr_type = MAN_DEFAULT_IMR_TYPE;
	int use_ext_man = 0;

	memset(&image, 0, sizeof(image));

	image.xcc_mod_offset = DEFAULT_XCC_MOD_OFFSET;

	while ((opt = getopt(argc, argv, "ho:va:s:k:ri:x:f:b:ec:y:")) != -1) {
		switch (opt) {
		case 'o':
			image.out_file = optarg;
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
		case 'i':
			imr_type = atoi(optarg);
			break;
		case 'x':
			image.xcc_mod_offset = atoi(optarg);
			break;
		case 'f':
			image.fw_ver_string = optarg;
			break;
		case 'b':
			image.fw_ver_build_string = optarg;
			break;
		case 'e':
			use_ext_man = 1;
			break;
		case 'c':
			adsp_config = optarg;
			break;
		case 'y':
			image.verify_file = optarg;
			break;
		case 'h':
			usage(argv[0]);
			break;
		default:
		 /* getopt's default error message is good enough */
			break;
		}
	}

	first_non_opt = optind;

	/* we must have config */
	if (!adsp_config) {
		fprintf(stderr, "error: must have adsp desc");
		usage(argv[0]);
	}

	/* requires private key */
	if (!image.key_name) {
		fprintf(stderr, "error: requires private key\n");
		return -EINVAL;
	}

	/* make sure we have an outfile if nt verifying */
	if ((!image.out_file && !image.verify_file))
		usage(argv[0]);


	/* firmware version and build id */
	if (image.fw_ver_string) {
		ret = sscanf(image.fw_ver_string, "%hu.%hu",
			     &image.fw_ver_major,
			     &image.fw_ver_minor);

		if (ret != 2) {
			fprintf(stderr,
				"error: cannot parse firmware version\n");
			return -EINVAL;
		}
	}

	if (image.fw_ver_build_string) {
		ret = sscanf(image.fw_ver_build_string, "%hu",
			     &image.fw_ver_build);

		if (ret != 1) {
			fprintf(stderr,
				"error: cannot parse build version\n");
			return -EINVAL;
		}
	}
	/* find machine */
	heap_adsp = malloc(sizeof(struct adsp));
	if (!heap_adsp) {
		fprintf(stderr, "error: cannot parse build version\n");
		return -ENOMEM;
	}
	image.adsp = heap_adsp;
	memset(heap_adsp, 0, sizeof(*heap_adsp));
	ret = adsp_parse_config(adsp_config, heap_adsp, image.verbose);
	if (ret < 0)
		goto out;

	/* verify mode ? */
	if (image.verify_file) {
		ret = verify_image(&image);
		goto out;
	}

	/* set IMR Type in found machine definition */
	if (image.adsp->man_v1_8)
		image.adsp->man_v1_8->adsp_file_ext.imr_type = imr_type;

	if (image.adsp->man_v2_5)
		image.adsp->man_v2_5->adsp_file_ext.imr_type = imr_type;

	/* parse input ELF files */
	image.num_modules = argc - first_non_opt;

	if (image.num_modules <= 0) {
		fprintf(stderr,
			"error: requires at least one ELF input module\n");
		return -EINVAL;
	}

	/* getopt reorders argv[] */
	for (i = first_non_opt; i < argc; i++) {
		fprintf(stdout, "\nModule Reading %s\n", argv[i]);
		ret = elf_parse_module(&image, i - first_non_opt, argv[i]);
		if (ret < 0)
			goto out;
	}

	/* validate all modules */
	ret = elf_validate_modules(&image);
	if (ret < 0)
		goto out;

	/* open outfile for writing */
	unlink(image.out_file);
	image.out_fd = fopen(image.out_file, "wb");
	if (!image.out_fd) {
		fprintf(stderr, "error: unable to open %s for writing %d\n",
			image.out_file, errno);
		ret = -EINVAL;
		goto out;
	}

	/* process and write output */
	if (image.meu_offset) {
		assert(image.adsp->write_firmware_meu);
		ret = image.adsp->write_firmware_meu(&image);
	} else {
		assert(image.adsp->write_firmware);
		ret = image.adsp->write_firmware(&image);
	}
	if (ret)
		goto out;

	/* build extended manifest */
	if (use_ext_man) {
		ret = ext_man_write(&image);
		if (ret < 0) {
			fprintf(stderr, "error: unable to write extended manifest, %d\n",
				ret);
			goto out;
		}
	}

out:
	/* free memory */
	adsp_free(heap_adsp);

	/* close files */
	if (image.out_fd)
		fclose(image.out_fd);

	return ret;
}
