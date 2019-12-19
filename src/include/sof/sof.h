/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_SOF_H__
#define __SOF_SOF_H__

#include <arch/sof.h>
#include <sof/common.h>
#include <sof/lib/memory.h>

struct clock_info;
struct dma_trace_data;
struct ipc;
struct sa;
struct trace;

/**
 * \brief General firmware context.
 * This structure holds all the global pointers, which can potentially
 * be accessed by SMP code, hence it should be aligned to platform's
 * data cache line size. Alignments in the both beginning and end are needed
 * to avoid potential before and after data evictions.
 */
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

	/* generic trace structure */
	struct trace *trace;

	/* platform clock information */
	struct clock_info *clocks;

	__aligned(PLATFORM_DCACHE_ALIGN) int alignment[0];
} __aligned(PLATFORM_DCACHE_ALIGN);

struct sof *sof_get(void);

#endif /* __SOF_SOF_H__ */
