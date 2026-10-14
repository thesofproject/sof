/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/**
 * \file include/sof/debug/debug.h
 * \brief Early firmware bootstrap debug dumps, mailbox peek/poke, and diagnostic helpers.
 * \ingroup debug_stream_api
 */

#ifndef __SOF_DEBUG_DEBUG_H__
#define __SOF_DEBUG_DEBUG_H__

#include <sof/common.h>
#include <rtos/panic.h>
#include <rtos/cache.h>
#include <sof/lib/cpu.h>
#include <rtos/sof.h>
#include <rtos/string.h>
#include <ipc/info.h>
#include <ipc/trace.h>

#include <stddef.h>
#include <stdint.h>

#ifndef RELATIVE_FILE
#error "This file requires RELATIVE_FILE to be defined. " \
	"Add it to CMake's target with sof_append_relative_path_definitions."
#endif

#if CONFIG_DEBUG

#include <sof/lib/mailbox.h>

/** Firmware ready flags describing debug capabilities (GDB, build flags) */
#define DEBUG_SET_FW_READY_FLAGS					\
(									\
	SOF_IPC_INFO_BUILD |						\
	(IS_ENABLED(CONFIG_GDB_DEBUG) ? SOF_IPC_INFO_GDB : 0)	\
)

/** \brief Dump current file name, function name, and line number to base of debug mailbox */
#define dbg() \
	do { \
		volatile uint32_t *__m = (uint32_t *)mailbox_get_debug_base(); \
		*(__m++) = (RELATIVE_FILE[0] << 24) + \
			   (RELATIVE_FILE[1] << 16) + \
			   (RELATIVE_FILE[2] << 8) + (RELATIVE_FILE[3]); \
		*(__m++) = (__func__[0] << 24) + (__func__[1] << 16) + \
			(__func__[2] << 8) + (__func__[3]); \
		*__m = __LINE__; \
	} while (0)

/** \brief Dump current file, function, and line to specific 32-bit word offset in debug mailbox */
#define dbg_at(__x) \
	do { \
		volatile uint32_t *__m = \
			(uint32_t *)mailbox_get_debug_base() + __x; \
		*(__m++) = (RELATIVE_FILE[0] << 24) + \
			   (RELATIVE_FILE[1] << 16) + \
			   (RELATIVE_FILE[2] << 8) + (RELATIVE_FILE[3]); \
		*(__m++) = (__func__[0] << 24) + (__func__[1] << 16) + \
			(__func__[2] << 8) + (__func__[3]); \
		*__m = __LINE__; \
	} while (0)

/** \brief Dump a 32-bit integer value directly to start of debug mailbox */
#define dbg_val(__v) \
	do { \
		volatile uint32_t *__m = \
			(volatile uint32_t *)mailbox_get_debug_base(); \
		*__m = __v; \
	} while (0)

/** \brief Dump a 32-bit integer value to specific word offset in debug mailbox */
#define dbg_val_at(__v, __x) \
	do { \
		volatile uint32_t *__m = \
			(volatile uint32_t *)mailbox_get_debug_base() + __x; \
		*__m = __v; \
	} while (0)

/** \brief Dump raw memory buffer of given 32-bit word count to start of debug mailbox */
#define dump(addr, count) \
	do { \
		volatile uint32_t *__m = (uint32_t *)mailbox_get_debug_base(); \
		volatile uint32_t *__a = (uint32_t *)addr; \
		volatile int __c = count; \
		while (__c--) \
			*(__m++) = *(__a++); \
	} while (0)

/** \brief Dump memory buffer of given 32-bit word count to mailbox offset */
#define dump_at(addr, count, offset) \
	do { \
		volatile uint32_t *__m = \
			(uint32_t *)mailbox_get_debug_base() + offset; \
		volatile uint32_t *__a = (uint32_t *)addr; \
		volatile int __c = count; \
		while (__c--) \
			*(__m++) = *(__a++); \
	} while (0)

/** \brief Dump a stack/global object and source location to start of debug mailbox */
#define dump_object(__o) \
	do {						\
		dbg();					\
		dump(&__o, sizeof(__o) >> 2);		\
	} while (0)

/** \brief Dump an object from pointer and source location to start of debug mailbox */
#define dump_object_ptr(__o) \
	do {						\
		dbg();					\
		dump(__o, sizeof(*(__o)) >> 2);		\
	} while (0)

/** \brief Dump an object from pointer to specific offset in debug mailbox */
#define dump_object_ptr_at(__o, __at) \
	do {						\
		dbg();					\
		dump_at(__o, sizeof(*(__o)) >> 2, __at);\
	} while (0)

#else

#define DEBUG_SET_FW_READY_FLAGS					\
(									\
	(IS_ENABLED(CONFIG_GDB_DEBUG) ? SOF_IPC_INFO_GDB : 0)	\
)

#define dbg() do {} while (0)
#define dbg_at(__x) do {} while (0)
#define dbg_val(__v) do {} while (0)
#define dbg_val_at(__v, __x) do {} while (0)
#define dump(addr, count) do {} while (0)
#define dump_object(__o) do {} while (0)
#define dump_object_ptr(__o) do {} while (0)
#endif

/* swap an endianness of 32-bit word */
#define bswap32(N) ({ \
	__typeof__(N) n = (N); \
	((n & 0x000000FF) << 24) | \
	((n & 0x0000FF00) << 8) | \
	((n & 0x00FF0000) >> 8) | \
	((n & 0xFF000000) >> 24); })

#if CONFIG_TRACE
/* dump up to 4 32-bit words into trace starting in ptr, idx is incremented inside
 * unconditionally swaps endianness of data
 * todo: use cpu_to_endianness so trace data will be independent of platform
 */
#define dump_hex(ptr, idx, len) \
	do { \
		__typeof__(idx) __i = (idx); \
		__typeof__(ptr) __p = (ptr) + __i; \
		__typeof__(idx) __l = (len) - __i; \
		__typeof__(idx) __n = __l > 4 ? 4 : __l; \
		if (__n == 4) { \
			comp_info(dev, "%08x%08x%08x%08x", bswap32(__p[0]), bswap32(__p[1]), \
				  bswap32(__p[2]), bswap32(__p[3])); \
		} else if (__n == 3) { \
			comp_info(dev, "%08x%08x%08x", bswap32(__p[0]), bswap32(__p[1]), \
				  bswap32(__p[2])); \
		} else if (__n == 2) { \
			comp_info(dev, "%08x%08x", bswap32(__p[0]), bswap32(__p[1])); \
		} else if (__n == 1) { \
			comp_info(dev, "%08x", bswap32(__p[0])); \
		} \
		idx += __n; \
	} while (0)
#else

#define dump_hex() do {} while (0)

#endif

#endif /* __SOF_DEBUG_DEBUG_H__ */
