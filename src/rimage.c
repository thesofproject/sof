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
#include <rimage/file_utils.h>


static void usage(char *name)
{
	fprintf(stdout, "%s:\t -c adsp_desc -o outfile -k [key] ELF files\n",
		name);
	fprintf(stdout, "%s:\t -c adsp_desc -y infile -k [key]\n",
			name);
	fprintf(stdout, "\t -v enable verbose output\n");
	fprintf(stdout, "\t -r enable relocatable ELF files\n");
	fprintf(stdout, "\t -s MEU signing offset, disables rimage signing\n");
	fprintf(stdout, "\t -i set IMR type\n");
	fprintf(stdout, "\t -f firmware version = major.minor.micro\n");
	fprintf(stdout, "\t -b build version\n");
	fprintf(stdout, "\t -e build extended manifest\n");
	fprintf(stdout, "\t -l build loadable modules image (don't treat the first module as a bootloader)\n");
	fprintf(stdout, "\t -y verify signed file\n");
	fprintf(stdout, "\t -q resign binary\n");
	fprintf(stdout, "\t -p set PV bit\n");
}

int main(int argc, char *argv[])
{
	struct image image;
	struct adsp *heap_adsp;
	const char *adsp_config = NULL;
	int opt, ret, i, first_non_opt;
	int use_ext_man = 0;
	unsigned int pv_bit = 0;
	bool imr_type_override = false;

	memset(&image, 0, sizeof(image));

	image.imr_type = MAN_DEFAULT_IMR_TYPE;

	while ((opt = getopt(argc, argv, "ho:va:s:k:ri:f:b:ec:y:q:pl")) != -1) {
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
			image.imr_type = atoi(optarg);
			imr_type_override = true;
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
			return 0;
		case 'q':
			image.in_file = optarg;
			break;
		case 'p':
			pv_bit = 1;
			break;
		case 'l':
			image.loadable_module = true;
			break;
		default:
		 /* getopt's default error message is good enough */
			return 1;
		}
	}

	first_non_opt = optind;

	/* we must have config */
	if (!adsp_config) {
		usage(argv[0]);
		fprintf(stderr, "error: must have adsp desc\n");
		return -EINVAL;
	}

	/* requires private key */
	if (!image.key_name) {
		fprintf(stderr, "error: requires private key\n");
		return -EINVAL;
	}

	/* make sure we have an outfile if not verifying */
	if ((!image.out_file && !image.verify_file)) {
		usage(argv[0]);
		return -EINVAL;
	}

	/* firmware version: major.minor.micro */
	if (image.fw_ver_string) {
		ret = sscanf(image.fw_ver_string, "%hu.%hu.%hu",
			     &image.fw_ver_major,
			     &image.fw_ver_minor,
			     &image.fw_ver_micro);

		if (ret != 3) {
			fprintf(stderr,
				"error: cannot parse firmware version major.minor.micro\n");
			return -EINVAL;
		}
	}

	/* firmware build id */
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
		fprintf(stderr, "error: memory allocation for adsp struct failed\n");
		return -ENOMEM;
	}
	image.adsp = heap_adsp;
	memset(heap_adsp, 0, sizeof(*heap_adsp));
	ret = adsp_parse_config(adsp_config, &image);
	if (ret < 0)
		goto out;

	/* verify mode ? */
	if (image.verify_file) {
		ret = verify_image(&image);
		goto out;
	}

	if (image.in_file) {
		fprintf(stdout, "going to re-sign\n");
		ret = resign_image(&image);
		goto out;
	}

	/* set IMR Type and the PV bit in found machine definition */
	if (image.adsp->man_v1_8) {
		if (imr_type_override)
			image.adsp->man_v1_8->adsp_file_ext.imr_type = image.imr_type;
		image.adsp->man_v1_8->css.reserved0 = pv_bit;
	}

	if (image.adsp->man_v2_5) {
		if (imr_type_override)
			image.adsp->man_v2_5->adsp_file_ext.imr_type = image.imr_type;
		image.adsp->man_v2_5->css.reserved0 = pv_bit;
	}

	if (image.adsp->man_ace_v1_5) {
		if (imr_type_override)
			image.adsp->man_ace_v1_5->adsp_file_ext.imr_type = image.imr_type;
		image.adsp->man_ace_v1_5->css.reserved0 = pv_bit;
	}

	/* parse input ELF files */
	image.num_modules = argc - first_non_opt;

	if (image.num_modules <= 0) {
		fprintf(stderr,
			"error: requires at least one ELF input module\n");
		ret = -EINVAL;
		goto out;
	}

	/* Some platforms dont have modules configuration in toml file */
	if (image.adsp->modules && image.num_modules > image.adsp->modules->mod_man_count) {
		fprintf(stderr, "error: Each ELF input module requires entry in toml file.\n");
		ret = -EINVAL;
		goto out;
	}

	/* getopt reorders argv[] */
	for (opt = first_non_opt; opt < argc; opt++) {
		i = opt - first_non_opt;
		fprintf(stdout, "\nModule Reading %s\n", argv[opt]);
		ret = module_open(&image.module[i].file, argv[opt], image.verbose);
		if (ret < 0)
			goto out;

		module_parse_sections(&image.module[i].file, &image.adsp->mem, image.verbose);

		/* When there is more than one module, then first one is bootloader.
		 * Does not apply to building a image of a loadable module. */
		image.module[i].is_bootloader = image.num_modules > 1 && i == 0 &&
			!image.loadable_module;
	}

	/* validate all modules */
	ret = modules_validate(&image);
	if (ret < 0)
		goto out;

	/* open outfile for writing */
	unlink(image.out_file);
	image.out_fd = fopen(image.out_file, "wb");
	if (!image.out_fd) {
		ret = file_error("unable to open file for writing", image.out_file);
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
		if (image.adsp->write_firmware_ext_man)
			ret = image.adsp->write_firmware_ext_man(&image);
		else
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

	/* Free loaded modules */
	for (i = 0; i < image.num_modules; i++) {
		module_close(&image.module[i].file);
	}

	return ret;
}
