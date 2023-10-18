/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 *
 * Author: Rander Wang <rander.wang@linux.intel.com>
 */

#ifndef __RIMAGE_CAVS_EXT_MANIFEST_H__
#define __RIMAGE_CAVS_EXT_MANIFEST_H__

/* Structure of ext manifest :
 * ExtendedManifestHeader
 *  ExtendedModuleConfig[0]
 *      SchedulingCapability[]
 *      PinDescr[]
 *  ExtendedModuleConfig[1]
 *      SchedulingCapability[]
 *      PinDescr[]
 *  ...
 *  ExtendedModuleConfig[N]
 *      SchedulingCapability[]
 *      PinDescr[]
 */

/* ExtendedManifestHeader id $AE1 */
#define EXTENDED_MANIFEST_MAGIC_HEADER_ID	0x31454124
#define EXTENDED_MANIFEST_VERSION_MAJOR		0x0001
#define EXTENDED_MANIFEST_VERSION_MINOR		0x0000

#define FW_MAX_EXT_MODULE_NUM	32

struct uuid_t {
	uint32_t d0;
	uint16_t d1;
	uint16_t d2;
	uint8_t d3;
	uint8_t d4;
	uint8_t d5;
	uint8_t d6;
	uint8_t d7;
	uint8_t d8;
	uint8_t d9;
	uint8_t d10;
} __attribute__((packed));

union mod_multiples {
	uint16_t ul;
	struct {
		uint16_t x1 : 1;
		uint16_t x2 : 1;
		uint16_t x3 : 1;
		uint16_t x4 : 1;
		uint16_t x5 : 1;
		uint16_t x6 : 1;
		uint16_t x7 : 1;
		uint16_t x8 : 1;
		uint16_t x9 : 1;
		uint16_t x10 : 1;
		uint16_t x11 : 1;
		uint16_t x12 : 1;
		uint16_t x13 : 1;
		uint16_t x14 : 1;
		uint16_t x15 : 1;
		uint16_t all : 1;
	} r;
} __attribute__((packed));

struct mod_scheduling_caps {
	/* scheduling period in Samples (sample groups) (note: 1 Sample = 1 sample per channel) */
	uint16_t    frame_length;
	union mod_multiples   multiples_supported;
} __attribute__((packed));

enum mod_pin_direction {
	pin_input = 0,
	pin_output = 1
};

union mod_pin_caps {
	uint32_t ul;
	struct {
		uint16_t    direction : 1;	/* 0 : input; 1: output */
		uint16_t    reserved0 : 15;
		uint16_t    reserved1 : 16;
	} r;
} __attribute__((packed));

union mod_sample_rates {
	uint32_t ul;
	struct {
		uint32_t freq_8000 : 1;
		uint32_t freq_11025 : 1;
		uint32_t freq_12000 : 1;
		uint32_t freq_16000 : 1;
		uint32_t freq_18900 : 1;
		uint32_t freq_22050 : 1;
		uint32_t freq_24000 : 1;
		uint32_t freq_32000 : 1;
		uint32_t freq_37800 : 1;
		uint32_t freq_44100 : 1;
		uint32_t freq_48000 : 1;
		uint32_t freq_64000 : 1;
		uint32_t freq_88200 : 1;
		uint32_t freq_96000 : 1;
		uint32_t freq_176400 : 1;
		uint32_t freq_192000 : 1;
		uint32_t reserved : 16;
	} r;
} __attribute__((packed));

union mod_sample_sizes {
	uint32_t ul;
	struct {
		uint16_t bits_8 : 1;
		uint16_t bits_16 : 1;
		uint16_t bits_24 : 1;
		uint16_t bits_32 : 1;
		uint16_t bits_64 : 1;
		uint16_t reserved0 : 11;
		uint16_t reserved1 : 16;
	} r;
} __attribute__((packed));

union mod_sample_containers {
	uint32_t ul;
	struct {
		uint16_t bits_8 : 1;
		uint16_t bits_16 : 1;
		uint16_t bits_24 : 1;
		uint16_t bits_32 : 1;
		uint16_t bits_64 : 1;
		uint16_t reserved0 : 11;
		uint16_t reserved1 : 16;
	} r;
} __attribute__((packed));

union mod_channel_config {
	uint32_t ul;
	struct {
		/* FRONT_CENTER */
		uint32_t channel_mono : 1;
		/* FRONT_LEFT | BACK_LEFT */
		uint32_t channel_dual_mono : 1;
		/* FRONT_LEFT | FRONT_RIGHT */
		uint32_t channel_stereo : 1;
		/* FRONT_LEFT | FRONT_RIGHT | LOW_FREQUENCY */
		uint32_t channel_2_1 : 1;
		/* FRONT_LEFT | FRONT_RIGHT | FRONT_CENTER */
		uint32_t channel_3_0 : 1;
		/* FRONT_LEFT | FRONT_RIGHT | BACK_LEFT  | BACK_RIGHT */
		uint32_t channel_quad : 1;
		/* FRONT_LEFT | FRONT_RIGHT | FRONT_CENTER | BACK_CENTER */
		uint32_t channel_surround : 1;
		/* FRONT_LEFT | FRONT_RIGHT | FRONT_CENTER | LOW_FREQUENCY */
		uint32_t channel_3_1 : 1;
		/* FRONT_LEFT | FRONT_RIGHT | FRONT_CENTER | BACK_LEFT | BACK_RIGHT */
		uint32_t channel_5_0 : 1;
		/* FRONT_LEFT | FRONT_RIGHT | FRONT_CENTER | SIDE_LEFT | SIDE_RIGHT */
		uint32_t channel_5_0_surround : 1;
		/* FRONT_LEFT | FRONT_RIGHT | FRONT_CENTER | LOW_FREQUENCY | BACK_LEFT |
		 * BACK_RIGHT
		 */
		uint32_t channel_5_1 : 1;
		/* FRONT_LEFT | FRONT_RIGHT | FRONT_CENTER | LOW_FREQUENCY | SIDE_LEFT |
		 * SIDE_RIGHT
		 */
		uint32_t channel_5_1_surround : 1;
		/* FRONT_LEFT | FRONT_RIGHT | FRONT_CENTER | BACK_LEFT | BACK_RIGHT |
		 * FRONT_LEFT_OF_CENTER | FRONT_RIGHT_OF_CENTER
		 */
		uint32_t channel_7_0 : 1;
		/* FRONT_LEFT | FRONT_RIGHT | FRONT_CENTER | BACK_LEFT | BACK_RIGHT |
		 * SIDE_LEFT | SIDE_RIGHT
		 */
		uint32_t channel_7_0_surround : 1;
		/* FRONT_LEFT | FRONT_RIGHT | FRONT_CENTER | LOW_FREQUENCY | BACK_LEFT |
		 * BACK_RIGHT | FRONT_LEFT_OF_CENTER | FRONT_RIGHT_OF_CENTER
		 */
		uint32_t channel_7_1 : 1;
		/* FRONT_LEFT | FRONT_RIGHT | FRONT_CENTER | LOW_FREQUENCY | BACK_LEFT |
		 * BACK_RIGHT | SIDE_LEFT | SIDE_RIGHT
		 */
		uint32_t channel_7_1_surround : 1;
		uint32_t reserved : 16;
	} r;
} __attribute__((packed));

enum mod_stream_type {
	epcm = 0,	/* PCM stream */
	emp3,	/* MP3 encoded stream */
	eaac,	/* AAC encoded stream */
	emax_stream_type,
	estream_type_invalid = 0xFF
};

enum mod_type {
	ebasefw = 0,
	emixin,
	emixout,
	ecopier,
	epeakvol,
	eupdwmix,
	emux,
	esrc,
	ewov,
	efx,
	eaec,
	ekpb,
	emicselect,
	efxf,	/*i.e.SmartAmp */
	eaudclass,
	efakecopier,
	eiodriver,
	ewhm,
	egdbstub,
	esensing,
	emax,
	einvalid = emax
} ;

struct fw_pin_description {
	union mod_pin_caps		caps;
	enum mod_stream_type		format_type;
	union mod_sample_rates		sample_rate;
	union mod_sample_sizes		sample_size;
	union mod_sample_containers	sample_container;
	union mod_channel_config	ch_cfg;
} __attribute__((packed));

struct fw_ext_man_cavs_header {
	uint32_t id;
	uint32_t len;	/* sizeof(Extend Manifest) in bytes */
	uint16_t version_major;	/* Version of Extended Manifest structure */
	uint16_t version_minor;	/* Version of Extended Manifest structure */
	uint32_t num_module_entries;
} __attribute__((packed));

struct fw_ext_mod_config_header {
	uint32_t ext_module_config_length;	/* sizeof(fw_ext_mod_config_header) in bytes */
	uint32_t guid[4];	/* Module GUID */
	uint16_t version_major;	/* Module version */
	uint16_t version_minor;	/* Module version */
	uint16_t version_hotfix;	/* Module version */
	uint16_t version_build;	/* Module version */
	enum mod_type module_type;
	uint32_t init_settings_min_size;	/* Minimum size of initialization settings (in bytes) */
	uint16_t num_scheduling_capabilities;	/* number scheduling capabilities supported by the module */
	uint16_t num_pin_entries;	/* Number of Pin (inputs + ouptuts) */
} __attribute__((packed));

#endif
