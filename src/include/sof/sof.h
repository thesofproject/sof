/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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

struct ipc;
struct sa;

/* use same syntax as Linux for simplicity */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define container_of(ptr, type, member) \
	({const typeof(((type *)0)->member) *__memberptr = (ptr); \
	(type *)((char *)__memberptr - offsetof(type, member));})

#define ALIGN(size, align) (((size) + (align) - 1) & ~((align) - 1))

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
#elif CONFIG_HOST
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
