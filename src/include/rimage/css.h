/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 */

#ifndef __CSS_H__
#define __CSS_H__

#include <stdint.h>

struct image;

#define MAN_CSS_LT_MODULE_TYPE		0x00000006
#define MAN_CSS_MOD_TYPE		4
#define MAN_CSS_HDR_SIZE		161	/* in words */
#define MAN_CSS_HDR_SIZE_2_5		225	/* in words */
#define MAN_CSS_HDR_VERSION		0x10000
#define MAN_CSS_HDR_VERSION_2_5		0x21000
#define MAN_CSS_MOD_VENDOR		0x8086
#define MAN_CSS_HDR_ID			{'$', 'M', 'N', '2'}

#define MAN_CSS_KEY_SIZE		(MAN_RSA_KEY_MODULUS_LEN >> 2)
#define MAN_CSS_MOD_SIZE		(MAN_RSA_KEY_MODULUS_LEN >> 2)
#define MAN_CSS_MOD_SIZE_2_5		(MAN_RSA_KEY_MODULUS_LEN_2_5 >> 2)
#define MAN_CSS_EXP_SIZE		(MAN_RSA_KEY_EXPONENT_LEN >> 2)
#define MAN_CSS_MAN_SIZE_V1_8		\
	(sizeof(struct fw_image_manifest_v1_8) >> 2)
#define MAN_CSS_MAN_SIZE_V1_5		\
	(sizeof(struct fw_image_manifest_v1_5) >> 2)

/*
 * RSA Key and Crypto
 */
#define MAN_RSA_KEY_MODULUS_LEN		256
#define MAN_RSA_KEY_MODULUS_LEN_2_5	384
#define MAN_RSA_KEY_EXPONENT_LEN	4
#define MAN_RSA_SIGNATURE_LEN		256
#define MAN_RSA_SIGNATURE_LEN_2_5	384

struct fw_version {
	uint16_t major_version;
	uint16_t minor_version;
	uint16_t hotfix_version;
	uint16_t build_version;
} __attribute__((packed));

struct css_header_v2_5 {
	uint32_t header_type;
	uint32_t header_len;
	uint32_t header_version;
	uint32_t reserved0; /* must be 0x1 */
	uint32_t module_vendor;
	uint32_t date;
	uint32_t size;
	uint8_t header_id[4];
	uint32_t padding;  /* must be 0x0 */
	struct fw_version version;
	uint32_t svn;
	uint32_t reserved1[18];  /* must be 0x0 */
	uint32_t modulus_size;
	uint32_t exponent_size;
	uint8_t modulus[MAN_RSA_KEY_MODULUS_LEN_2_5];
	uint8_t exponent[MAN_RSA_KEY_EXPONENT_LEN];
	uint8_t signature[MAN_RSA_SIGNATURE_LEN_2_5];
} __attribute__((packed));

struct css_header_v1_8 {
	uint32_t header_type;
	uint32_t header_len;
	uint32_t header_version;
	uint32_t reserved0; /* must be 0x0 */
	uint32_t module_vendor;
	uint32_t date;
	uint32_t size;
	uint8_t header_id[4];
	uint32_t padding;  /* must be 0x0 */
	struct fw_version version;
	uint32_t svn;
	uint32_t reserved1[18];  /* must be 0x0 */
	uint32_t modulus_size;
	uint32_t exponent_size;
	uint8_t modulus[MAN_RSA_KEY_MODULUS_LEN];
	uint8_t exponent[MAN_RSA_KEY_EXPONENT_LEN];
	uint8_t signature[MAN_RSA_SIGNATURE_LEN];
} __attribute__((packed));

struct css_header_v1_5 {
	uint32_t    module_type;
	uint32_t    header_len;
	uint32_t    header_version;
	uint32_t    reserved0;          /* must be 0x0 */
	uint32_t    module_vendor;
	uint32_t    date;
	uint32_t    size;
	uint32_t    key_size;
	uint32_t    modulus_size;
	uint32_t    exponent_size;
	uint32_t    reserved[22];
	uint8_t     modulus[MAN_RSA_KEY_MODULUS_LEN];
	uint8_t     exponent[MAN_RSA_KEY_EXPONENT_LEN];
	uint8_t     signature[MAN_RSA_SIGNATURE_LEN];
} __attribute__((packed));

void ri_css_v1_8_hdr_create(struct image *image);
void ri_css_v1_5_hdr_create(struct image *image);
void ri_css_v2_5_hdr_create(struct image *image);

#endif
