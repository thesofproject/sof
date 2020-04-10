// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <stdint.h>

#include <sof/lib/alloc.h>
#include <sof/trace/trace.h>
#include <sof/debug/panic.h>
#include <sof/schedule/task.h>

struct dma_copy;
struct dma_sg_config;

static struct sof sof;

void arch_dump_regs_a(void *dump_buf)
{
	(void)dump_buf;
}

int rstrlen(const char *s)
{
	(void)s;

	return 0;
}

void trace_flush(void)
{
}

volatile void *task_context_get(void)
{
	return NULL;
}

struct sof *sof_get(void)
{
	return &sof;
}
