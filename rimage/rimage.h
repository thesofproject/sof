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
#include <kernel/fw.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define MAX_MODULES		32

struct adsp;
struct manifest;
struct man_module;

/* list of supported targets */
enum machine_id {
	MACHINE_BAYTRAIL	= 0,
	MACHINE_CHERRYTRAIL,
	MACHINE_BRASWELL,
	MACHINE_HASWELL,
	MACHINE_BROADWELL,
	MACHINE_APOLLOLAKE,
	MACHINE_KABYLAKE,
	MACHINE_SKYLAKE,
	MACHINE_CANNONLAKE,
	MACHINE_ICELAKE,
	MACHINE_TIGERLAKE,
	MACHINE_SUECREEK,
	MACHINE_IMX8,
	MACHINE_IMX8X,
	MACHINE_IMX8M,
	MACHINE_MAX
};

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
	int logs_index;
	int uids_index;
	int fw_ready_index;

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
	const char *ldc_out_file;
	FILE *out_fd;
	FILE *ldc_out_fd;
	void *pos;

	const struct adsp *adsp;
	int abi;
	int verbose;
	int reloc;	/* ELF data is relocatable */
	int num_modules;
	struct module module[MAX_MODULES];
	uint32_t image_end;/* module end, equal to output image size */
	int meu_offset;
	int xcc_mod_offset;

	/* SHA 256 & 384 */
	const char *key_name;
	EVP_MD_CTX *mdctx;
	const EVP_MD *md;

	/* file IO */
	void *fw_image;
	void *rom_image;
	FILE *out_rom_fd;
	FILE *out_man_fd;
	FILE *out_unsigned_fd;
	char out_rom_file[256];
	char out_man_file[256];
	char out_unsigned_file[256];
};

struct mem_zone {
	uint32_t base;
	uint32_t size;
	uint32_t host_offset;
};

/*
 * Audio DSP descriptor and operations.
 */
struct adsp {
	const char *name;
	struct mem_zone mem_zones[SOF_FW_BLK_TYPE_NUM];

	uint32_t image_size;
	uint32_t dram_offset;

	enum machine_id machine_id;
	int (*write_firmware)(struct image *image);
	int (*write_firmware_meu)(struct image *image);
	struct fw_image_manifest_v2_5 *man_v2_5;
	struct fw_image_manifest_v1_8 *man_v1_8;
	struct fw_image_manifest_v1_5 *man_v1_5;
	struct fw_image_manifest_v1_5_sue *man_v1_5_sue;
	int exec_boot_ldr;
};

int write_dictionaries(struct image *image);

void module_sha256_create(struct image *image);
void module_sha_update(struct image *image, uint8_t *data, size_t bytes);
void module_sha_complete(struct image *image, uint8_t *hash);
int ri_manifest_sign_v1_5(struct image *image);
int ri_manifest_sign_v1_8(struct image *image);
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

int elf_parse_module(struct image *image, int module_index, const char *name);
void elf_free_module(struct image *image, int module_index);
int elf_is_rom(struct image *image, Elf32_Shdr *section);
int elf_validate_modules(struct image *image);
int elf_find_section(struct image *image, struct module *module,
		const char *name);
int elf_validate_section(struct image *image, struct module *module,
	Elf32_Shdr *section, int index);

/* supported machines */
extern const struct adsp machine_byt;
extern const struct adsp machine_cht;
extern const struct adsp machine_bsw;
extern const struct adsp machine_hsw;
extern const struct adsp machine_bdw;
extern const struct adsp machine_apl;
extern const struct adsp machine_cnl;
extern const struct adsp machine_icl;
extern const struct adsp machine_jsl;
extern const struct adsp machine_tgl;
extern const struct adsp machine_sue;
extern const struct adsp machine_skl;
extern const struct adsp machine_kbl;
extern const struct adsp machine_imx8;
extern const struct adsp machine_imx8x;
extern const struct adsp machine_imx8m;

#endif
