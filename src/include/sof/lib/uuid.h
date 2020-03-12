/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Maka <marcin.maka@linux.intel.com>
 */

#ifndef __SOF_LIB_UUID_H__
#define __SOF_LIB_UUID_H__

#include <sof/common.h>

/** \addtogroup uuid_api UUID API
 *  UUID API specification.
 *  @{
 */

/** \brief UUID is 16 bytes long */
#define UUID_SIZE 16

/**
 * \brief UUID (Universally Unique IDentifier) structure.
 *
 * Use DECLARE_SOF_UUID() to assigned UUID to the fw part (component
 * implementation, dai implementation, ...).
 *
 * Use SOF_UUID() to store an address of declared UUID.
 *
 * See existing implementation of components and dais for examples how to
 * UUIDs are declared and assigned to the drivers to provide identification
 * of the source for their log entries.
 *
 * UUID for a new component may be generated with uuidgen Linux tool, part
 * of the util-linux package.
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

/** \brief Declares UUID (aaaaaaaa-bbbb-cccc-d0d1-d2d3d4d5d6d7) and name.
 *
 * \param entity_name Name of the object pinted by the software tools.
 * \param uuid_name Uuid symbol name used with SOF_UUID().
 * \param va aaaaaaaa value.
 * \param vb bbbb value.
 * \param vc cccc value.
 * \param vd0 d0 value (note how d0 and d1 are grouped in formatted uuid)
 * \param vd1 d1 value.
 * \param vd2 d2 value.
 * \param vd3 d3 value.
 * \param vd4 d4 value.
 * \param vd5 d5 value.
 * \param vd6 d6 value.
 * \param vd7 d7 value.
 */
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

/** \brief Creates local unique 32-bit representation of UUID structure.
 *
 * \param uuid_name UUID symbol name declared with DECLARE_SOF_UUID().
 */
#define SOF_UUID(uuid_name) ((uintptr_t)&(uuid_name))
#endif

/** @}*/

#endif /* __SOF_LIB_UUID_H__ */
