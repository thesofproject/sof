/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Jaska Uimonen <jaska.uimonen@linux.intel.com>
 */

#include <sof/common.h>

struct wav_fmt {
	uint16_t fmt_tag;
	uint16_t channels;
	uint32_t samples_per_sec;
	uint32_t avg_bytes_per_sec;
	uint16_t block_align;
	uint16_t bits_per_sample;
	uint16_t cb_size;
} __packed;

struct wav_fmt_ext {
	struct wav_fmt fmt;
	union samples {
		uint16_t valid_bits_per_sample;
		uint16_t samples_per_block;
		uint16_t reserved;
	} sample;
	uint32_t channel_mask;
	uint8_t sub_fmt[16];
} __packed;

enum nhlt_link_type {
	NHLT_LINK_HDA = 0,
	NHLT_LINK_DSP = 1,
	NHLT_LINK_DMIC = 2,
	NHLT_LINK_SSP = 3,
	NHLT_LINK_INVALID
};

enum nhlt_device_type {
	NHLT_DEVICE_BT = 0,
	NHLT_DEVICE_DMIC = 1,
	NHLT_DEVICE_I2S = 4,
	NHLT_DEVICE_INVALID
};

struct nhlt_specific_cfg {
	uint32_t size;
	uint8_t caps[];
} __packed;

struct nhlt_fmt_cfg {
	struct wav_fmt_ext fmt_ext;
	struct nhlt_specific_cfg config;
} __packed;

struct nhlt_fmt {
	uint8_t fmt_count;
	struct nhlt_fmt_cfg fmt_config[];
} __packed;

struct nhlt_endpoint {
	uint32_t  length;
	uint8_t   linktype;
	uint8_t   instance_id;
	uint16_t  vendor_id;
	uint16_t  device_id;
	uint16_t  revision_id;
	uint32_t  subsystem_id;
	uint8_t   device_type;
	uint8_t   direction;
	uint8_t   virtual_bus_id;
	struct nhlt_specific_cfg config;
} __packed;

struct acpi_table_header {
	uint32_t signature;
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	uint8_t oem_id[6];
	uint64_t oem_table_id;
	uint32_t oem_revision;
	uint32_t creator_id;
	uint32_t creator_revision;
} __packed;

struct nhlt_acpi_table {
	struct acpi_table_header header;
	uint8_t endpoint_count;
	struct nhlt_endpoint desc[];
} __packed;

struct nhlt_resource_desc  {
	uint32_t extra;
	uint16_t flags;
	uint64_t addr_spc_gra;
	uint64_t min_addr;
	uint64_t max_addr;
	uint64_t addr_trans_offset;
	uint64_t length;
} __packed;

#define MIC_ARRAY_2CH 2
#define MIC_ARRAY_4CH 4

struct nhlt_device_specific_config {
	uint8_t virtual_slot;
	uint8_t config_type;
} __packed;

struct nhlt_dmic_array_config {
	struct nhlt_device_specific_config device_config;
	uint8_t array_type;
} __packed;

struct nhlt_vendor_dmic_array_config {
	struct nhlt_dmic_array_config dmic_config;
	uint8_t nb_mics;
	/* TODO add vendor mic config */
} __packed;

enum {
	NHLT_CONFIG_TYPE_GENERIC = 0,
	NHLT_CONFIG_TYPE_MIC_ARRAY = 1
};

enum {
	NHLT_MIC_ARRAY_2CH_SMALL = 0xa,
	NHLT_MIC_ARRAY_2CH_BIG = 0xb,
	NHLT_MIC_ARRAY_4CH_1ST_GEOM = 0xc,
	NHLT_MIC_ARRAY_4CH_L_SHAPED = 0xd,
	NHLT_MIC_ARRAY_4CH_2ND_GEOM = 0xe,
	NHLT_MIC_ARRAY_VENDOR_DEFINED = 0xf,
};
