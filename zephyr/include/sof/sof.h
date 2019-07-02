/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __INCLUDE_SOF_SOF__
#define __INCLUDE_SOF_SOF__

#include <stdint.h>
#include <stddef.h>
#include <arch/sof.h>
#include <sof/panic.h>
#include <sof/preproc.h>
#include <config.h>

struct ipc;
struct sa;

/* use same syntax as Linux for simplicity */
#define container_of(ptr, type, member) \
	CONTAINER_OF(ptr, type, member)

/* Align the number to the nearest alignment value */
#define ALIGN_UP(size, alignment) \
	(((size) % (alignment) == 0) ? (size) : \
	((size) - ((size) % (alignment)) + (alignment)))
#define ALIGN_DOWN(size, alignment) \
	((size) - ((size) % (alignment)))

#define __aligned(x) __attribute__((__aligned__(x)))

/* count number of var args */
#define PP_NARG(...) (sizeof((unsigned int[]){0, ##__VA_ARGS__}) \
	/ sizeof(unsigned int) - 1)

/* compile-time assertion */
#define STATIC_ASSERT(COND, MESSAGE)	\
	__attribute__((unused))		\
	typedef char META_CONCAT(assertion_failed_, MESSAGE)[(COND) ? 1 : -1]

/** \name Declare module macro
 *  \brief Usage at the end of an independent module file:
 *         DECLARE_MODULE(sys_*_init);
 *  @{
 */
#ifdef UNIT_TEST
#define DECLARE_MODULE(init)
#elif CONFIG_LIBRARY
/* In case of shared libs components are initialised in dlopen */
#define DECLARE_MODULE(init) __attribute__((constructor)) \
	static void _module_init(void) { init(); }
#else
struct _sof_module {
	void (*init)(void);
};

#define DECLARE_MODULE(_init) \
	const Z_STRUCT_SECTION_ITERABLE(_sof_module, _sof_module_##_init) = { \
				.init = _init,               \
			}
#endif

/* general firmware context */
struct sof {
	/* init data */
	int argc;
	char **argv;

	/* ipc */
	struct ipc *ipc;

	/* system agent */
	struct sa *sa;

	/* DMA for Trace*/
	struct dma_trace_data *dmat;
};

#endif

