/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef SOF_UAPI_MANIFEST_H
#define SOF_UAPI_MANIFEST_H

#include <stdint.h>

#define MAN_PAGE_SIZE	4096
#define ELF_TEXT_OFFSET	0x2000

/*
 * FW modules.
 *
 */

/* module type load type */
#define MAN_MOD_TYPE_LOAD_BUILTIN	0
#define MAN_MOD_TYPE_LOAD_MODULE 	1

struct module_type {
	uint32_t load_type:4;	/* MAN_MOD_TYPE_LOAD_ */
	uint32_t auto_start:1;
	uint32_t domain_ll:1;
	uint32_t domain_dp:1;
	uint32_t rsvd_:25;
};

/* segment flags.type */
#define MAN_SEGMENT_TEXT		0
#define MAN_SEGMENT_DATA		1
#define MAN_SEGMENT_BSS 		2

union segment_flags
{
	uint32_t ul;
	struct {
		uint32_t contents:1;
		uint32_t alloc:1;
		uint32_t load:1;
		uint32_t readonly:1;
		uint32_t code:1;
		uint32_t data:1;
		uint32_t _rsvd0:2;
		uint32_t type:4;	/* MAN_SEGMENT_ */
		uint32_t _rsvd1:4;
		uint32_t length:16;	/* of segment in pages */
	} r;
} __attribute__((packed));

/*
 * Module segment descriptor.
 */
struct segment_desc {
	union segment_flags flags;
	uint32_t v_base_addr;
	uint32_t file_offset;
} __attribute__((packed));

/*
 * The firmware binary can be split into several modules. We will only support
 * one module (the base firmware) at the moment.
 */

#define MAN_MODULE_NAME_LEN		 8
#define MAN_MODULE_SHA256_LEN		32
#define MAN_MODULE_ID			{'$','A','M','E'}

#define MAN_MODULE_BASE_NAME		"BASEFW"
#define MAN_MODULE_BASE_UUID		\
	{0xb9, 0x0c, 0xeb, 0x61, 0xd8, 0x34, 0x59, 0x4f, 0xa2, 0x1d, 0x04, 0xc5, 0x4c, 0x21, 0xd3, 0xa4}
#define MAN_MODULE_BASE_TYPE		0x21


#define MAN_MODULE_BASE_CFG_OFFSET	0x0
#define MAN_MODULE_BASE_CFG_COUNT	0x0
#define MAN_MODULE_BASE_AFFINITY	0x3	/* all 4 cores */
#define MAN_MODULE_BASE_INST_COUNT	0x1
#define MAN_MODULE_BASE_INST_BSS	0x11

/*
 * Each module has an entry in the FW header.
 */
struct module {
	uint8_t struct_id[4];			/* MAN_MODULE_ID */
	uint8_t name[MAN_MODULE_NAME_LEN];
	uint8_t uuid[16];
	struct module_type type;
	uint8_t hash[MAN_MODULE_SHA256_LEN];
	uint32_t entry_point;
	uint16_t cfg_offset;
	uint16_t cfg_count;
	uint32_t affinity_mask;
	uint16_t instance_max_count;	/* max number of instances */
	uint16_t instance_bss_size;	/* instance (pages) */
	struct segment_desc segment[3];
} __attribute__((packed));

/*
 * Each module has a configuration in the FW header.
 */
struct mod_config {
	uint32_t par[4];	/* module parameters */
	uint32_t is_pages;	/* actual size of instance .bss (pages) */
	uint32_t cps;		/* cycles per second */
	uint32_t ibs;		/* input buffer size (bytes) */
	uint32_t obs;		/* output buffer size (bytes) */
	uint32_t module_flags;	/* flags, reserved for future use */
	uint32_t cpc;		/* cycles per single run */
	uint32_t obls;		/* output block size, reserved for future use */
} __attribute__((packed));


/*
 * FW Manifest Header
 */

#define MAN_FW_HDR_FW_NAME_LEN		8

#define MAN_FW_HDR_ID			{'$','A','M','1'}
#define MAN_FW_HDR_NAME			"ADSPFW"
#define MAN_FW_HDR_FLAGS		0x0
#define MAN_FW_HDR_FEATURES		0x1f

/* hard coded atm - will pass this in from cmd line and git */
#define MAN_FW_HDR_VERSION_MAJOR	9
#define MAN_FW_HDR_VERSION_MINOR	22
#define MAN_FW_HDR_VERSION_HOTFIX	1
#define MAN_FW_HDR_VERSION_BUILD	0x7da


/*
 * The firmware has a standard header that is checked by the ROM on firmware
 * loading.  Members from header_id to num_module entries must be unchanged
 * to be backward compatible with SPT-LP ROM.
 *
 * preload_page_count is used by DMA code loader and is entire image size on
 * BXT. i.e. BXT: total size of the binary’s .text and .rodata
 */
struct adsp_fw_header {
	/* ROM immutable start */
	uint8_t header_id[4];
	uint32_t header_len; /* sizeof(this) in bytes (0x34) */
	uint8_t name[MAN_FW_HDR_FW_NAME_LEN];
	uint32_t preload_page_count; /* number of pages of preloaded image loaded by driver */
	uint32_t fw_image_flags;
	uint32_t feature_mask;
	uint16_t major_version;
	uint16_t minor_version;
	uint16_t hotfix_version;
	uint16_t build_version;
	uint32_t num_module_entries;
	/* ROM immutable end - members after this point may be modified */
	uint32_t hw_buf_base_addr;
	uint32_t hw_buf_length;
	/* target address for binary loading as offset in IMR - must be == base offset */
	uint32_t load_offset;
} __attribute__((packed));


/* just base module atm */
#define MAN_BXT_NUM_MODULES		2

struct adsp_fw_desc {
	struct adsp_fw_header header;
	struct module module[MAN_BXT_NUM_MODULES];
	struct mod_config config[MAN_BXT_NUM_MODULES];
} __attribute__((packed));

#endif

