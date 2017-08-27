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

#ifndef __FILE_FORMAT_H__
#define __FILE_FORMAT_H__

#define REEF_FW_SIGNATURE_SIZE	4
#define REEF_FW_SIGN			"$SST"
#define REEF_FW_LIB_SIGN		"$LIB"

#define REEF_IRAM	1
#define REEF_DRAM	2
#define REEF_REGS	3
#define REEF_CACHE	3

enum reef_module_id {
	REEF_MODULE_BASE_FW = 0x0,
	REEF_MODULE_MP3     = 0x1,
	REEF_MODULE_AAC_5_1 = 0x2,
	REEF_MODULE_AAC_2_0 = 0x3,
	REEF_MODULE_SRC     = 0x4,
	REEF_MODULE_WAVES   = 0x5,
	REEF_MODULE_DOLBY   = 0x6,
	REEF_MODULE_BOOST   = 0x7,
	REEF_MODULE_LPAL    = 0x8,
	REEF_MODULE_DTS     = 0x9,
	REEF_MODULE_PCM_CAPTURE = 0xA,
	REEF_MODULE_PCM_SYSTEM = 0xB,
	REEF_MODULE_PCM_REFERENCE = 0xC,
	REEF_MODULE_PCM = 0xD,
	REEF_MODULE_BLUETOOTH_RENDER_MODULE = 0xE,
	REEF_MODULE_BLUETOOTH_CAPTURE_MODULE = 0xF,
	REEF_MAX_MODULE_ID,
};

struct dma_block_info {
	uint32_t type;		/* IRAM/DRAM */
	uint32_t size;		/* Bytes */
	uint32_t ram_offset;	/* Offset in I/DRAM */
	uint32_t rsvd;		/* Reserved field */
} __attribute__((packed));

struct fw_module_info {
	uint32_t persistent_size;
	uint32_t scratch_size;
} __attribute__((packed));

struct fw_header {
	unsigned char signature[REEF_FW_SIGNATURE_SIZE]; /* FW signature */
	uint32_t file_size;		/* size of fw minus this header */
	uint32_t modules;		/*  # of modules */
	uint32_t file_format;	/* version of header format */
	uint32_t reserved[4];
} __attribute__((packed));

/* ABI 0 - used by reef driver and CoE BDW/HSW firmware*/
struct hsw_module_header {
	unsigned char signature[REEF_FW_SIGNATURE_SIZE]; /* module signature */
	uint32_t mod_size;	/* size of module */
	uint32_t blocks;	/* # of blocks */
	uint16_t padding;
	uint16_t type;	/* codec type, pp lib */
	uint32_t entry_point;
	struct fw_module_info info;
} __attribute__((packed));

/* ABI 1 - used by CoE/MCG BYT/CHT/BSW driver */
struct byt_module_header {
	unsigned char signature[REEF_FW_SIGNATURE_SIZE];
	uint32_t mod_size; /* size of module */
	uint32_t blocks; /* # of blocks */
	uint32_t type; /* codec type, pp lib */
	uint32_t entry_point;
}__attribute__((packed));

#endif
