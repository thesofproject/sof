/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

/**
 * \file include/user/manifest.h
 * \brief FW Image Manifest definitions.
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __RIMAGE_USER_MANIFEST_H__
#define __RIMAGE_USER_MANIFEST_H__

#include <stdint.h>

/* start offset for base FW module */
#define SOF_MAN_ELF_TEXT_OFFSET		0x2000

/* FW Extended Manifest Header id = $AE1 */
#define SOF_MAN_EXT_HEADER_MAGIC	0x31454124

/* module type load type */
#define SOF_MAN_MOD_TYPE_BUILTIN	0
#define SOF_MAN_MOD_TYPE_MODULE		1
#define SOF_MAN_MOD_TYPE_LLEXT		2	/* Zephyr LLEXT-style dynamically linked */
#define SOF_MAN_MOD_TYPE_LLEXT_AUX	3	/* Zephyr LLEXT-style dynamically linked auxiliary */

/* module init config */
#define SOF_MAN_MOD_INIT_CONFIG_BASE_CFG               0 /* Base config only */
#define SOF_MAN_MOD_INIT_CONFIG_BASE_CFG_WITH_EXT      1 /* Base config with extension */

#define MAN_MAX_SIZE_V1_8		(38 * 1024)


struct sof_man_module_type {
	uint32_t load_type:4;	/* SOF_MAN_MOD_TYPE_ */
	uint32_t auto_start:1;
	uint32_t domain_ll:1;
	uint32_t domain_dp:1;
	uint32_t lib_code:1;
	uint32_t domain_rtos:1;
	uint32_t core_type:8;
	uint32_t user_mode:1;
	uint32_t large_param:1;
	uint32_t init_config:4; /* SOF_MAN_MOD_INIT_CONFIG_ */
	uint32_t rsvd_:9;
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
#define SOF_MAN_MOD_SHA384_LEN		48
#define SOF_MAN_MOD_ID			{'$', 'A', 'M', 'E'}

struct sof_man_uuid {
	uint32_t a;
	uint16_t b;
	uint16_t c;
	uint8_t  d[8];
};

/*
 * Each module has an entry in the FW header. Used by ROM - Immutable.
 */
struct sof_man_module {
	uint8_t struct_id[SOF_MAN_MOD_ID_LEN];	/* SOF_MAN_MOD_ID */
	uint8_t name[SOF_MAN_MOD_NAME_LEN];
	struct sof_man_uuid uuid;
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

union sof_man_fw_header_image_flags {
	uint32_t raw;
	struct {
		uint32_t tp : 1;
		uint32_t image_type : 2;
		uint32_t relocatable_lib : 1;
		uint32_t _rsvd0 : 28;
	} fields;
};

#define SOF_MAN_FW_HDR_IMG_TYPE_ROM_EXT	0
#define SOF_MAN_FW_HDR_IMG_TYPE_MAIN_FW	1
#define SOF_MAN_FW_HDR_IMG_TYPE_LIB	2

#define SOF_MAN_FW_HDR_FW_NAME_LEN	8
#define SOF_MAN_FW_HDR_ID		{'$', 'A', 'M', '1'}
#define SOF_MAN_FW_HDR_NAME		"ADSPFW"
#define SOF_MAN_FW_HDR_FLAGS		0x0
#define SOF_MAN_FW_HDR_FEATURES		0xffff

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
	union sof_man_fw_header_image_flags fw_image_flags;
	uint32_t feature_mask;
	uint16_t major_version;
	uint16_t minor_version;
	uint16_t hotfix_version;
	uint16_t build_version;
	uint32_t num_module_entries;
	uint32_t fw_compat;
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

#define SOF_MAN_COMP_SHA256_LEN		32
#define SOF_MAN_COMP_SHA384_LEN		48

/*
 * Component Descriptor for manifest v1.8. Used by ROM - Immutable.
 */
struct sof_man_component_desc_v1_8 {
	uint32_t reserved[2];	/* all 0 */
	uint32_t version;
	uint8_t hash[SOF_MAN_COMP_SHA256_LEN];
	uint32_t base_offset;
	uint32_t limit_offset;
	uint32_t attributes[4];
} __attribute__((packed));

/*
 * Audio DSP extended metadata for manifest v1.8. Used by ROM - Immutable.
 */
struct sof_man_adsp_meta_file_ext_v1_8 {
	uint32_t ext_type;	/* always 17 for ADSP extension */
	uint32_t ext_len;
	uint32_t imr_type;
	uint8_t reserved[16];	/* all 0 */
	struct sof_man_component_desc_v1_8 comp_desc[1];
} __attribute__((packed));

/*
 * Component Descriptor for manifest v2.5. Used by ROM - Immutable.
 */
struct sof_man_component_desc_v2_5 {
	uint32_t reserved[2];	/* all 0 */
	uint32_t version;
	uint8_t hash[SOF_MAN_COMP_SHA384_LEN];
	uint32_t base_offset;
	uint32_t limit_offset;
	uint32_t attributes[4];
} __attribute__((packed));

/*
 * Audio DSP extended metadata for manifest v2.5. Used by ROM - Immutable.
 */
struct sof_man_adsp_meta_file_ext_v2_5 {
	uint32_t ext_type;	/* always 17 for ADSP extension */
	uint32_t ext_len;
	uint32_t imr_type;
	uint8_t reserved[16];	/* all 0 */
	struct sof_man_component_desc_v2_5 comp_desc[1];
} __attribute__((packed));

/*
 * Module Manifest for rimage module metadata. Not used by ROM.
 */
struct sof_man_module_manifest {
	struct sof_man_module module;
	uint32_t text_size;
};

/*
 * Module offset in manifest.
 */
#define SOF_MAN_MODULE_OFFSET(index) \
	(sizeof(struct sof_man_fw_header) + \
		(index) * sizeof(struct sof_man_module))

/*
 * LLEXT module link area for detached sections. When an LLEXT module contains
 * detached sections, they will be linked with addresses in this range. The
 * upper limit has no special meaning, simply assuming that 128MiB should be
 * enough and that SRAM will not use these addresses.
 */
#define SOF_MODULE_DRAM_LINK_START	0
#define SOF_MODULE_DRAM_LINK_END	0x08000000

#endif /* __RIMAGE_USER_MANIFEST_H__ */
