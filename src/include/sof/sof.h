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
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define container_of(ptr, type, member) \
	({const typeof(((type *)0)->member)*__memberptr = (ptr); \
	(type *)((char *)__memberptr - offsetof(type, member)); })

#define ALIGN(size, align) (((size) + (align) - 1) & ~((align) - 1))

#define __packed __attribute__((packed))

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
#define DECLARE_MODULE(init) __attribute__((__used__)) \
	__attribute__((section(".module_init"))) static void(*f)(void) = init
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
