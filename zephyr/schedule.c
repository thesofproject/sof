// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>

/* Generic scheduler */
#include <sof/schedule/schedule.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/lib/alloc.h>
#include <ipc/topology.h>

static struct schedulers *_schedulers;

/**
 * Retrieves registered schedulers.
 * @return List of registered schedulers.
 */
struct schedulers **arch_schedulers_get(void)
{
	return &_schedulers;
}
