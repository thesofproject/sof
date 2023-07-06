/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Leman <tomasz.m.leman@intel.com>
 */

#ifndef __SOF_TLV_H__
#define __SOF_TLV_H__

#include <stdint.h>
#include <string.h>
#include <sof/compiler_attributes.h>

/**
 * @brief Type–length–value struct.
 *
 * The TLV structure is used to pass data between the SW and FW. The data block may include TLV
 * sequences of variable size and in random order. Data sequences can be easily search using
 * generalized parsing function.
 */
struct sof_tlv {
	uint32_t type;
	uint32_t length;
	char value[];
} __packed __aligned(4);

/**
 * @brief Allows to step through successive values in a sequence of TLV structures.
 *
 * @param tlv Pointer to the base TLV.
 * @return struct sof_tlv* Pointer to the next TLV.
 */
static inline struct sof_tlv *tlv_next(const struct sof_tlv *tlv)
{
	return (struct sof_tlv *)((char *)(tlv) + sizeof(*tlv) + tlv->length);
}

/**
 * @brief Fills the TLV Structure (version for 32-bit values).
 *
 * @param tlv TLV struct pointer.
 * @param type Value type.
 * @param value The value.
 */
static inline void tlv_value_uint32_set(struct sof_tlv *tlv, uint32_t type, uint32_t value)
{
	tlv->type = type;
	tlv->length = sizeof(uint32_t);
	memcpy_s(tlv->value, tlv->length, &value, tlv->length);
}

/**
 * @brief Fills the TLV Structure (general purpose version).
 *
 * @param tlv TLV struct pointer.
 * @param type Value type.
 * @param length Value size.
 * @param value Pointer to the value.
 */
static inline void tlv_value_set(struct sof_tlv *tlv, uint32_t type, uint32_t length, void *value)
{
	tlv->type = type;
	tlv->length = length;
	memcpy_s(tlv->value, length, value, length);
}

/**
 * @brief Searches a sequence of TLV structures for values of the specified type.
 *
 * @param data Pointer to the beginning of the TLV sequence.
 * @param size The size of the data block containing the TLV structure sequences.
 * @param type The type of the searched value.
 * @param value A pointer that will point to the found value.
 * @param length The size of the found value.
 */
static inline void tlv_value_get(const void *data,
				 uint32_t size,
				 uint32_t type,
				 void **value,
				 uint32_t *length)
{
	const struct sof_tlv *tlv = (const struct sof_tlv *)data;
	const uint32_t end_addr = (uint32_t)data + size;

	while ((uint32_t)tlv < end_addr) {
		if (tlv->type == type) {
			*value = (void *)tlv->value;
			*length = tlv->length;
			break;
		}

		tlv = tlv_next(tlv);
	}
}

#endif /* __SOF_TLV_H__ */
