/*
 * Copyright (c) 2017, Intel Corporation
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

/**
 * \file include/uapi/user/manifest.h
 * \brief FW Image Manifest definitions.
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __INCLUDE_UAPI_USER_MANIFEST_H__
#define __INCLUDE_UAPI_USER_MANIFEST_H__

#include <stdint.h>

/* start offset for base FW module */
#define SOF_MAN_ELF_TEXT_OFFSET		0x2000

/* FW Extended Manifest Header id = $AE1 */
#define SOF_MAN_EXT_HEADER_MAGIC	0x31454124

/* module type load type */
#define SOF_MAN_MOD_TYPE_BUILTIN	0
#define SOF_MAN_MOD_TYPE_MODULE		1

struct sof_man_module_type {
	uint32_t load_type:4;	/* SOF_MAN_MOD_TYPE_ */
	uint32_t auto_start:1;
	uint32_t domain_ll:1;
	uint32_t domain_dp:1;
	uint32_t rsvd_:25;
};

/* segment flags.type */
#define SOF_MAN_SEGMENT_TEXT		0
#define SOF_MAN_SEGMENT_RODATA		1
#define SOF_MAN_SEGMENT_DATA		1
#define SOF_MAN_SEGMENT_BSS		2
#define SOF_MAN_SEGMENT_EMPTY		15

union sof_man_segment_flags {
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
 * Module segment descriptor. Used by ROM - Immutable.
 */
struct sof_man_segment_desc {
	union sof_man_segment_flags flags;
	uint32_t v_base_addr;
	uint32_t file_offset;
} __attribute__((packed));

/*
 * The firmware binary can be split into several modules.
 */

#define SOF_MAN_MOD_ID_LEN		4
#define SOF_MAN_MOD_NAME_LEN		8
#define SOF_MAN_MOD_SHA256_LEN		32
#define SOF_MAN_MOD_ID			{'$', 'A', 'M', 'E'}

/*
 * Each module has an entry in the FW header. Used by ROM - Immutable.
 */
struct sof_man_module {
	uint8_t struct_id[SOF_MAN_MOD_ID_LEN];	/* SOF_MAN_MOD_ID */
	uint8_t name[SOF_MAN_MOD_NAME_LEN];
	uint8_t uuid[16];
	struct sof_man_module_type type;
	uint8_t hash[SOF_MAN_MOD_SHA256_LEN];
	uint32_t entry_point;
	uint16_t cfg_offset;
	uint16_t cfg_count;
	uint32_t affinity_mask;
	uint16_t instance_max_count;	/* max number of instances */
	uint16_t instance_bss_size;	/* instance (pages) */
	struct sof_man_segment_desc segment[3];
} __attribute__((packed));

/*
 * Each module has a configuration in the FW header. Used by ROM - Immutable.
 */
struct sof_man_mod_config {
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

#define SOF_MAN_FW_HDR_FW_NAME_LEN	8
#define SOF_MAN_FW_HDR_ID		{'$', 'A', 'M', '1'}
#define SOF_MAN_FW_HDR_NAME		"ADSPFW"
#define SOF_MAN_FW_HDR_FLAGS		0x0
#define SOF_MAN_FW_HDR_FEATURES		0xff

/*
 * The firmware has a standard header that is checked by the ROM on firmware
 * loading. preload_page_count is used by DMA code loader and is entire
 * image size on CNL. i.e. CNL: total size of the binary’s .text and .rodata
 * Used by ROM - Immutable.
 */
struct sof_man_fw_header {
	uint8_t header_id[4];
	uint32_t header_len;
	uint8_t name[SOF_MAN_FW_HDR_FW_NAME_LEN];
	/* number of pages of preloaded image loaded by driver */
	uint32_t preload_page_count;
	uint32_t fw_image_flags;
	uint32_t feature_mask;
	uint16_t major_version;
	uint16_t minor_version;
	uint16_t hotfix_version;
	uint16_t build_version;
	uint32_t num_module_entries;
	uint32_t hw_buf_base_addr;
	uint32_t hw_buf_length;
	/* target address for binary loading as offset in IMR - must be == base offset */
	uint32_t load_offset;
} __attribute__((packed));

/*
 * Firmware manifest descriptor. This can contain N modules and N module
 * configs. Used by ROM - Immutable.
 */
struct sof_man_fw_desc {
	struct sof_man_fw_header header;

	/* Warning - hack for module arrays. For some unknown reason the we
	 * have a variable size array of struct man_module followed by a
	 * variable size array of struct mod_config. These should have been
	 * merged into a variable array of a parent structure. We have to hack
	 * around this in many places....
	 *
	 * struct sof_man_module man_module[];
	 * struct sof_man_mod_config mod_config[];
	 */

} __attribute__((packed));

/*
 * Component Descriptor. Used by ROM - Immutable.
 */
struct sof_man_component_desc {
	uint32_t reserved[2];	/* all 0 */
	uint32_t version;
	uint8_t hash[SOF_MAN_MOD_SHA256_LEN];
	uint32_t base_offset;
	uint32_t limit_offset;
	uint32_t attributes[4];
} __attribute__((packed));

/*
 * Audio DSP extended metadata. Used by ROM - Immutable.
 */
struct sof_man_adsp_meta_file_ext {
	uint32_t ext_type;	/* always 17 for ADSP extension */
	uint32_t ext_len;
	uint32_t imr_type;
	uint8_t reserved[16];	/* all 0 */
	struct sof_man_component_desc comp_desc[1];
} __attribute__((packed));

/*
 * Module Manifest for rimage module metadata. Not used by ROM.
 */
struct sof_man_module_manifest {
	struct sof_man_module module;
	uint32_t text_size;
};

/**
 * \brief Utility to get module pointer from position.
 * \param [in,out] desc FW descriptor in manifest.
 * \param [in] index Index of the module.
 * \return Pointer to module descriptor.
 *
 * Note that index is not verified against OOB.
 */
static inline struct sof_man_module *sof_man_get_module(
	struct sof_man_fw_desc *desc, int index)
{
	return (void *)desc + sizeof(struct sof_man_fw_header) +
			index * sizeof(struct sof_man_module);
}

#endif
