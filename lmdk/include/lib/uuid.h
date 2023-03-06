/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Maka <marcin.maka@linux.intel.com>
 */

#ifndef __SOF_LIB_UUID_H__
#define __SOF_LIB_UUID_H__

#include <../include/common.h>

/** \addtogroup uuid_api UUID API
 *  UUID API specification.
 *  @{
 */

/** \brief UUID is 16 bytes long */
#define UUID_SIZE 16

/** \brief UUID name string max length in bytes, including null termination */
#define UUID_NAME_MAX_LEN 32

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

/**
 * \brief Connects UUID with component description
 *
 * Declaration of this structure should be done by DECLARE_SOF_UUID(),
 * then declaration will be part of `.static_uuids` section and `SMEX` tool
 * use it during `ldc` file creation.
 */
struct sof_uuid_entry {
	struct sof_uuid id;
	const char name[UUID_NAME_MAX_LEN];
};

/** \brief Declares UUID (aaaaaaaa-bbbb-cccc-d0d1-d2d3d4d5d6d7) and name.
 *
 * UUID value from variables declared with this macro are unaccessible in
 * runtime code - UUID dictionary from ldc file is needed get UUID value.
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
	static const struct sof_uuid_entry uuid_name ## _ldc = {	\
		{.a = va, .b = vb, .c = vc,				\
		 .d = {vd0, vd1, vd2, vd3, vd4, vd5, vd6, vd7}},	\
		entity_name "\0"					\
	}

/** \brief Declares runtime UUID (aaaaaaaa-bbbb-cccc-d0d1-d2d3d4d5d6d7) and name.
 *
 * UUID value from variables declared with this macro are accessible in
 * runtime code - to dereference use SOF_RT_UUID()
 *
 * \param entity_name Name of the object pinted by the software tools.
 * \param uuid_name Uuid symbol name used with SOF_UUID() and SOF_RT_UUID().
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
#define DECLARE_SOF_RT_UUID(entity_name, uuid_name,			\
			 va, vb, vc,					\
			 vd0, vd1, vd2, vd3, vd4, vd5, vd6, vd7)	\
	DECLARE_SOF_UUID(entity_name, uuid_name,			\
			 va, vb, vc,					\
			 vd0, vd1, vd2, vd3, vd4, vd5, vd6, vd7);	\
	const struct sof_uuid uuid_name = {				\
		.a = va, .b = vb, .c = vc,				\
		.d = {vd0, vd1, vd2, vd3, vd4, vd5, vd6, vd7}		\
	}

/** \brief Creates local unique 32-bit representation of UUID structure.
 *
 * \param uuid_name UUID symbol name declared with DECLARE_SOF_UUID() or
 *		    DECLARE_SOF_RT_UUID().
 */
#define SOF_UUID(uuid_name) (&(uuid_name ## _ldc))

/** \brief Dereference unique 32-bit representation of UUID structure in runtime.
 *
 * \param uuid_name UUID symbol name declared with DECLARE_SOF_RT_UUID().
 */
#define SOF_RT_UUID(uuid_name) (&(uuid_name))

/** @}*/

#endif /* __SOF_LIB_UUID_H__ */
