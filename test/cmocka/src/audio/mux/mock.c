// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Daniel Bogdzia <danielx.bogdzia@linux.intel.com>

#include <sof/audio/component.h>
#include <sof/lib/alloc.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <cmocka.h>

static struct sof sof;

void pipeline_xrun(struct pipeline *p, struct comp_dev *dev, int32_t bytes)
{
}

struct sof *sof_get(void)
{
	return &sof;
}

struct schedulers **arch_schedulers_get(void)
{
	return NULL;
}

#if CONFIG_MULTICORE

int idc_send_msg(struct idc_msg *msg, uint32_t mode)
{
	(void)msg;
	(void)mode;

	return 0;
}

#endif
