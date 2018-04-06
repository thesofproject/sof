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

struct ipc;
struct sa;

/* use same syntax as Linux for simplicity */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define container_of(ptr, type, member) \
	({const typeof(((type *)0)->member) *__memberptr = (ptr); \
	(type *)((char *)__memberptr - offsetof(type, member));})

/*
 * Firmware symbol table.
 */
struct sof_symbol {
	unsigned long value;
	const char *name;
};

#define SOF_SYMBOL_STRING(sym) \
	static const char _symbolstr_##sym[] \
	__attribute__((section("_symbol_strings"), aligned(1)))	= #sym

#define SOF_SYMBOL_ELEM(sym) \
	static const struct sof_symbol _symbol_elem_##sym \
	__attribute__((used)) \
	__attribute__((section("_symbol_table"), used)) \
	= { (unsigned long)&sym, _symbolstr_##sym }

/* functions must be exported to be in the symbol table */
#define EXPORT(sym) \
	extern typeof(sym) sym;	\
	SOF_SYMBOL_STRING(sym); \
	SOF_SYMBOL_ELEM(sym);

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
