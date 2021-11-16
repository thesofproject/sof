/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2015-2018 Intel Corporation. All rights reserved.
 */

#ifndef __RIMAGE_H__
#define __RIMAGE_H__

#include "elf.h"

#include <stdint.h>
#include <stdio.h>

#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <rimage/cavs/cavs_ext_manifest.h>
#include <rimage/sof/kernel/fw.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define MAX_MODULES		32

struct adsp;
struct manifest;
struct man_module;

/*
 * ELF module data
 */
struct module {
	const char *elf_file;
	FILE *fd;

	Elf32_Ehdr hdr;
	Elf32_Shdr *section;
	Elf32_Phdr *prg;
	char *strings;

	uint32_t text_start;
	uint32_t text_end;
	uint32_t data_start;
	uint32_t data_end;
	uint32_t bss_start;
	uint32_t bss_end;
	uint32_t foffset;

	int num_sections;
	int num_bss;
	int fw_size;
	int bss_index;

	/* sizes do not include any gaps */
	int bss_size;
	int text_size;
	int data_size;

	/* sizes do include gaps to nearest page */
	int bss_file_size;
	int text_file_size;
	int text_fixup_size;
	int data_file_size;

	/* total file size */
	int file_size;

	/* executable header module */
	int exec_header;
};

/*
 * Firmware image context.
 */
struct image {

	const char *out_file;
	const char *in_file;
	FILE *out_fd;
	void *pos;

	const struct adsp *adsp;
	int abi;
	int verbose;
	int reloc;	/* ELF data is relocatable */
	int num_modules;
	struct module module[MAX_MODULES];
	uint32_t image_end;/* module end, equal to output image size */
	int meu_offset;
	const char *verify_file;

	/* SHA 256 & 384 */
	const char *key_name;
	EVP_MD_CTX *mdctx;
	const EVP_MD *md;

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
	uint16_t fw_ver_build;
};

struct mem_zone {
	uint32_t base;
	uint32_t size;
	uint32_t host_offset;
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
	struct mem_zone mem_zones[SOF_FW_BLK_TYPE_NUM];

	uint32_t image_size;
	uint32_t dram_offset;

	int (*write_firmware_ext_man)(struct image *image);
	int (*write_firmware)(struct image *image);
	int (*write_firmware_meu)(struct image *image);
	int (*verify_firmware)(struct image *image);
	struct fw_image_manifest_v2_5 *man_v2_5;
	struct fw_image_manifest_v1_8 *man_v1_8;
	struct fw_image_manifest_v1_5 *man_v1_5;
	struct fw_image_manifest_v1_5_sue *man_v1_5_sue;
	struct fw_image_manifest_module *modules;
	int exec_boot_ldr;
};

void module_sha256_create(struct image *image);
void module_sha384_create(struct image *image);
void module_sha_update(struct image *image, uint8_t *data, size_t bytes);
void module_sha_complete(struct image *image, uint8_t *hash);
int ri_manifest_sign_v1_5(struct image *image);
int ri_manifest_sign_v1_8(struct image *image);
int ri_manifest_sign_v2_5(struct image *image);
void ri_sha256(struct image *image, unsigned int offset, unsigned int size,
	       uint8_t *hash);
void ri_sha384(struct image *image, unsigned int offset, unsigned int size,
	       uint8_t *hash);

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

int resign_image(struct image *image);
int get_key_size(struct image *image);

int elf_parse_module(struct image *image, int module_index, const char *name);
void elf_free_module(struct image *image, int module_index);
int elf_is_rom(struct image *image, Elf32_Shdr *section);
int elf_validate_modules(struct image *image);
int elf_find_section(const struct module *module, const char *name);
int elf_read_section(const struct module *module, const char *name,
		     const Elf32_Shdr **dst_section, void **dst_buff);
int elf_validate_section(struct image *image, struct module *module,
			 Elf32_Shdr *section, int index);

#endif
