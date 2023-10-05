/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2015-2018 Intel Corporation. All rights reserved.
 */

#ifndef __RIMAGE_H__
#define __RIMAGE_H__

#include <stdint.h>
#include <stdio.h>

#include <rimage/manifest.h>
#include <rimage/cavs/cavs_ext_manifest.h>
#include <rimage/sof/kernel/fw.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define MAX_MODULES		32

struct adsp;

/*
 * Firmware image context.
 */
struct image {

	const char *out_file;
	const char *in_file;
	FILE *out_fd;
	void *pos;

	struct adsp *adsp;
	int abi;
	int verbose;
	int reloc;	/* ELF data is relocatable */
	int num_modules;
	struct manifest_module module[MAX_MODULES];
	uint32_t image_end;/* module end, equal to output image size */
	int meu_offset;
	const char *verify_file;

	/* private key file name*/
	const char *key_name;

	/* file IO */
	void *fw_image;
	void *rom_image;
	FILE *out_rom_fd;
	FILE *out_man_fd;
	FILE *out_ext_man_fd;
	FILE *out_unsigned_fd;
	char out_rom_file[256];
	char out_man_file[256];
	char out_ext_man_file[256];
	char out_unsigned_file[256];

	/* fw version and build id */
	char* fw_ver_string;
	char* fw_ver_build_string;
	uint16_t fw_ver_major;
	uint16_t fw_ver_minor;
	uint16_t fw_ver_micro;
	uint16_t fw_ver_build;

	uint32_t imr_type;

	/* Output image is a loadable module */
	bool loadable_module;
};

struct memory_zone {
	uint32_t base;
	uint32_t size;
	uint32_t host_offset;
};

struct memory_alias {
	uint32_t mask;
	uint32_t cached;
	uint32_t uncached;
};

struct memory_config {
	struct memory_zone zones[SOF_FW_BLK_TYPE_NUM];
	struct memory_alias alias;
};

struct fw_image_ext_mod_config {
	struct fw_ext_mod_config_header header;
	struct mod_scheduling_caps sched_caps;
	struct fw_pin_description *pin_desc;
};

struct fw_image_ext_module {
	uint32_t mod_conf_count;
	struct fw_image_ext_mod_config ext_mod_config_array[FW_MAX_EXT_MODULE_NUM];
};

/*
 * module manifest information defined in config file
 */
struct fw_image_manifest_module {
	struct fw_image_ext_module mod_ext;
	uint32_t mod_cfg_count;
	struct sof_man_mod_config *mod_cfg;
	uint32_t mod_man_count;
	struct sof_man_module *mod_man;
};

/*
 * Audio DSP descriptor and operations.
 */
struct adsp {
	const char *name;
	struct memory_config mem;

	uint32_t image_size;

	int (*write_firmware_ext_man)(struct image *image);
	int (*write_firmware)(struct image *image);
	int (*write_firmware_meu)(struct image *image);
	int (*verify_firmware)(struct image *image);
	struct fw_image_manifest_ace_v1_5 *man_ace_v1_5;
	struct fw_image_manifest_v2_5 *man_v2_5;
	struct fw_image_manifest_v1_8 *man_v1_8;
	struct fw_image_manifest_v1_5 *man_v1_5;
	struct fw_image_manifest_v1_5_sue *man_v1_5_sue;
	struct fw_image_manifest_module *modules;
	int exec_boot_ldr;
};

int ri_manifest_sign_v1_5(struct image *image);
int ri_manifest_sign_v1_8(struct image *image);
int ri_manifest_sign_v2_5(struct image *image);
int ri_manifest_sign_ace_v1_5(struct image *image);

int pkcs_v1_5_sign_man_v1_5(struct image *image,
			    struct fw_image_manifest_v1_5 *man,
			    void *ptr1, unsigned int size1);
int pkcs_v1_5_sign_man_v1_8(struct image *image,
			    struct fw_image_manifest_v1_8 *man,
			    void *ptr1, unsigned int size1, void *ptr2,
			    unsigned int size2);
int pkcs_v1_5_sign_man_v2_5(struct image *image,
			    struct fw_image_manifest_v2_5 *man,
			    void *ptr1, unsigned int size1, void *ptr2,
			    unsigned int size2);
int pkcs_v1_5_sign_man_ace_v1_5(struct image *image,
				struct fw_image_manifest_ace_v1_5 *man,
				void *ptr1, unsigned int size1, void *ptr2,
				unsigned int size2);

int verify_image(struct image *image);
int ri_manifest_verify_v1_5(struct image *image);
int ri_manifest_verify_v1_8(struct image *image);
int ri_manifest_verify_v2_5(struct image *image);
int pkcs_v1_5_verify_man_v1_5(struct image *image,
			    struct fw_image_manifest_v1_5 *man,
			    void *ptr1, unsigned int size1);
int pkcs_v1_5_verify_man_v1_8(struct image *image,
			    struct fw_image_manifest_v1_8 *man,
			    void *ptr1, unsigned int size1, void *ptr2,
			    unsigned int size2);
int pkcs_v1_5_verify_man_v2_5(struct image *image,
			    struct fw_image_manifest_v2_5 *man,
			    void *ptr1, unsigned int size1, void *ptr2,
			    unsigned int size2);
int pkcs_v1_5_verify_man_ace_v1_5(struct image *image,
				  struct fw_image_manifest_ace_v1_5 *man,
				  void *ptr1, unsigned int size1, void *ptr2,
				  unsigned int size2);

int resign_image(struct image *image);
int get_key_size(struct image *image);

#endif
