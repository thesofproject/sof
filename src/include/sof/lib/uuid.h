/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Maka <marcin.maka@linux.intel.com>
 */

#ifndef __SOF_LIB_UUID_H__
#define __SOF_LIB_UUID_H__

#include <sof/common.h>
#include <uuid-registry.h>

#ifdef __ZEPHYR__
#include <zephyr/sys/iterable_sections.h>
#endif

#include <stdint.h>

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
 *
 * FIXME: this struct scheme has an endianness bug.  On BE systems,
 * the same initialier for the a/b/c fields will produce different
 * memory layout than on LE systems.  Within C code, that's fine, but
 * when compared with external representations (c.f. topology) that
 * pass UUIDs as a linear array of bytes, only one endianness will
 * work.  If SOF ever ships on a BE system all use of sof_uuid will
 * need to be modified to byte swap the a/b/c values.
 *
 * Some identifiers are taken from the module manifest. Since the module
 * manifest structure (sof_man_module) is marked as packed, the pointer
 * to the sof_uuid structure may not be properly aligned. To avoid possible
 * problems with accessing fields of this structure from unaligned addresses,
 * it has been marked as packed.
 */
struct sof_uuid {
	uint32_t a;
	uint16_t b;
	uint16_t c;
	uint8_t  d[8];
} __packed;

#define _UUID_INIT(va, vb, vc, d0, d1, d2, d3, d4, d5, d6, d7) \
	{ va, vb, vc, { d0, d1, d2, d3, d4, d5, d6, d7 } }

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

#ifdef __ZEPHYR__
/* Zephyr puts all the UUID structs into the firmware .rodata as an
 * ITERABLE array.  Note the alias emitted to get the typing correct,
 * Zephyr defines the full suf_uuid_entry struct, where the API
 * demands that the symbol name refer to a struct sof_uuid.
 */
#define _UUID(uuid_name)    (&_##uuid_name)
#define _RT_UUID(uuid_name) (&uuid_name)
#define _DEF_UUID(entity_name, uuid_name, initializer)			\
	const STRUCT_SECTION_ITERABLE(sof_uuid_entry, _##uuid_name) =	\
		{ .id = initializer, .name = entity_name };		\
	extern const struct sof_uuid					\
		__attribute__((alias("_" #uuid_name))) uuid_name

#else
/* XTOS SOF emits two definitions, one into the runtime (which may not
 * be linked if unreferenced) and a separate one that goes into a
 * special section via handling in the linker script.
 */
#define _UUID(uuid_name)    (&(uuid_name ## _ldc))
#define _RT_UUID(uuid_name) (&(uuid_name))
#define _DEF_UUID(entity_name, uuid_name, initializer)		\
	__section(".static_uuids")				\
	static const struct sof_uuid_entry uuid_name ## _ldc	\
		= { .id = initializer, .name = entity_name };	\
	const struct sof_uuid uuid_name = initializer

#endif /* __ZEPHYR__ */

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
#define SOF_DEFINE_UUID(entity_name, uuid_name, va, vb, vc,	\
			vd0, vd1, vd2, vd3, vd4, vd5, vd6, vd7)	\
	_DEF_UUID(entity_name, uuid_name,			\
		  _UUID_INIT(va, vb, vc, vd0, vd1, vd2, vd3, vd4, vd5, vd6, vd7))

/** \brief Get UUID value sourced from the fixed SOF registry
 *
 * The ID value is sourced by name from the uuid-registry.txt file distributed with the source tree.
 *
 * \param name Name of the UUID, must match an entry in uuid-registry.txt
 */
#define SOF_REG_UUID(name) _UUIDREG_##name

/** \brief Defines UUID sourced from the fixed SOF registry
 *
 * As for SOF_DEFINE_UUID(), but the ID value is sourced by name from
 * the uuid-registry.txt file distributed with the source tree. The
 * string name field will be identical with the name passed (which is
 * passed as a symbol!), runtime symbol (e.g. the "uuid_name" argument
 * to SOF_DEFINE_UUID()) will be the same, postfixed with a "_uuid".
 *
 * \param name Name of the UUID, must match an entry in uuid-registry.txt
 */
#define SOF_DEFINE_REG_UUID(name) _DEF_UUID(#name, name##_uuid, SOF_REG_UUID(name))

/** \brief Creates local unique 32-bit representation of UUID structure.
 *
 * In Zephyr builds, this has the same address as the result of
 * SOF_RT_UUID, but has type of "struct sof_uuid *" and not "struct
 * sof_uid_record *"
 *
 * \param uuid_name UUID symbol name declared with DECLARE_SOF_UUID() or
 *                 DECLARE_SOF_RT_UUID().
 */
#define SOF_UUID(uuid_name) _UUID(uuid_name)

/** \brief Dereference unique 32-bit representation of UUID structure in runtime.
 *
 * \param uuid_name UUID symbol name declared with DECLARE_SOF_RT_UUID().
 */
#define SOF_RT_UUID(uuid_name) _RT_UUID(uuid_name)

/** @}*/

#endif /* __SOF_LIB_UUID_H__ */
