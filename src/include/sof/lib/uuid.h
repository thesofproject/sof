/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Maka <marcin.maka@linux.intel.com>
 */

#ifndef __SOF_LIB_UUID_H__
#define __SOF_LIB_UUID_H__

#include <sof/common.h>

#define UUID_SIZE 16

/**
 * \brief UUID (universally unique identifier) structure.
 *
 * Use DECLARE_SOF_UUID() to assigned UUID to the fw part (component
 * implementation, dai implementation, ...).
 *
 * Use SOF_UUID() to store an address of declared UUID.
 *
 * See existing implementation of components and dais for examples how to
 * UUIDs are declared and assigned to the drivers to provide identification
 * of the source for their log entries.
 */
struct sof_uuid {
	uint32_t a;
	uint16_t b;
	uint16_t c;
	uint8_t  d[8];
};

#if CONFIG_LIBRARY
#define DECLARE_SOF_UUID(entity_name, uuid_name,			\
			 va, vb, vc,					\
			 vd0, vd1, vd2, vd3, vd4, vd5, vd6, vd7)

#define SOF_UUID(uuid_name) 0
#else
#define DECLARE_SOF_UUID(entity_name, uuid_name,			\
			 va, vb, vc,					\
			 vd0, vd1, vd2, vd3, vd4, vd5, vd6, vd7)	\
	__section(".static_uuids")					\
	static const struct {						\
		struct sof_uuid id;					\
		uint32_t name_len;					\
		const char name[sizeof(entity_name)];			\
	} uuid_name = {							\
		{.a = va, .b = vb, .c = vc,				\
		 .d = {vd0, vd1, vd2, vd3, vd4, vd5, vd6, vd7}},	\
		sizeof(entity_name),					\
		entity_name						\
	}

#define SOF_UUID(uuid_name) ((uintptr_t)&(uuid_name))
#endif
#endif /* __SOF_LIB_UUID_H__ */
