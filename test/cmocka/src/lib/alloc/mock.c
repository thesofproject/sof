// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <stdint.h>

#include <sof/alloc.h>
#include <sof/trace.h>

#include <mock_trace.h>

TRACE_IMPL()

struct dma_copy;
struct dma_sg_config;

void arch_dump_regs_a(void *dump_buf, uint32_t ps)
{
	(void)dump_buf;
	(void)ps;
}

int rstrlen(const char *s)
{
	(void)s;

	return 0;
}

void trace_flush(void)
{
}
