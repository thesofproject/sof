/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 */

#ifndef __USER_MWW_H__
#define __USER_MWW_H__

#include <stdint.h>

/** \brief Magic of struct sof_mww_model_hdr, 'MWWM' in memory order */
#define SOF_MWW_MODEL_MAGIC		0x4d57574d

/** \brief Current version of struct sof_mww_model_hdr */
#define SOF_MWW_MODEL_VERSION		1

/** \brief Type of the int8 audio features the model was trained with */
enum sof_mww_feature_type {
	SOF_MWW_FEATURE_UNKNOWN = 0,	/**< Not specified */
	SOF_MWW_FEATURE_PCAN = 1,	/**< MFCC component PCAN int8 Mel */
	SOF_MWW_FEATURE_AGC8 = 2,	/**< MFCC component soft AGC int8 Mel */
};

/**
 * \brief Header of a microWakeWord model blob.
 *
 * The same blob is used for the built-in models and for the bytes control,
 * where it follows struct sof_abi_hdr. The TFLite flatbuffer follows this
 * header at offset hdr_size. Its size is a multiple of 16 bytes to keep the
 * flatbuffer aligned.
 */
struct sof_mww_model_hdr {
	uint32_t magic;		/**< SOF_MWW_MODEL_MAGIC */
	uint16_t version;	/**< SOF_MWW_MODEL_VERSION */
	uint16_t hdr_size;	/**< Size of this header, offset of the model */
	uint32_t model_size;	/**< Size of the TFLite flatbuffer in bytes */
	uint32_t model_crc32;	/**< CRC32 (IEEE 802.3, zlib) of the flatbuffer */
	uint32_t arena_size;	/**< Needed tensor arena in bytes, 0 if unknown */
	char name[16];		/**< Keyword name, zero padded, for information */
	uint8_t feature_type;	/**< enum sof_mww_feature_type */
	uint8_t num_mels;	/**< Mel bands per feature hop, e.g. 40 */
	uint8_t hop_ms;		/**< Feature hop length in ms, e.g. 10 */
	uint8_t slice_hops;	/**< Feature hops per inference, e.g. 3 */
	int16_t threshold_q8;	/**< Trigger threshold in Q0.8, 1..255 */
	uint8_t avg_len;	/**< Number of probabilities averaged for trigger */
	uint8_t reserved8;	/**< Reserved, set to zero */
	uint32_t reserved[5];	/**< Reserved for future use, set to zero */
} __attribute__((packed));

#endif /* __USER_MWW_H__ */
