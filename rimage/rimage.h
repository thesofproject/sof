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

#ifndef __RIMAGE_H__
#define __RIMAGE_H__

#include <stdint.h>
#include <elf.h>
#include <stdio.h>

#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

struct adsp;
struct fw_image_manifest;

struct image {
	const char *in_file;
	const char *out_file;
	FILE *in_fd;
	FILE *out_fd;
	void *pos;
	Elf32_Ehdr hdr;
	Elf32_Shdr *section;
	Elf32_Phdr *prg;
	const struct adsp *adsp;
	int abi;
	int verbose;

	int num_sections;
	int num_bss;
	int fw_size;
	int bss_size;
	int text_size;
	int data_size;
	int file_size;
	int num_modules;
	uint32_t text_start;
	uint32_t text_end;
	uint32_t data_start;
	uint32_t data_end;
	uint32_t bss_start;
	uint32_t bss_end;


	/* disa */
	void *in_buffer;
	void *out_buffer;
	int dump_sections;
};

struct section {
	const char *name;
	uint32_t addr;
	uint32_t size;
};

struct adsp_ops {
	/* write header or manifest */
	int (*write_header)(struct image *image);
	/* write data modules */
	int (*write_modules)(struct image *image);
};

enum machine_id {
	MACHINE_BAYTRAIL	= 0,
	MACHINE_CHERRYTRAIL,
	MACHINE_MAX
};

struct adsp {
	const char *name;
	uint32_t iram_base;
	uint32_t iram_size;
	uint32_t dram_base;
	uint32_t dram_size;
	uint32_t host_iram_offset;
	uint32_t host_dram_offset;

	uint32_t image_size;
	uint32_t dram_offset;

	enum machine_id machine_id;
	struct adsp_ops ops;
	const struct section *sections;
};

/* headers used by multiple platforms */
int byt_write_header(struct image *image);

/* modules used by multiple platforms */
int byt_write_modules(struct image *image);

/* for disassembly */
int write_byt_binary_image(struct image *image);

/* supported machines */
extern const struct adsp byt_machine;
extern const struct adsp cht_machine;

#endif
