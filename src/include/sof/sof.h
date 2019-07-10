/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_SOF_H__
#define __SOF_SOF_H__

#include <stdint.h>
#include <stddef.h>
#include <arch/sof.h>
#include <sof/common.h>
#include <sof/panic.h>
#include <sof/preproc.h>
#include <config.h>

struct ipc;
struct sa;

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

#endif /* __SOF_SOF_H__ */
