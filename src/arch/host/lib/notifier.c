// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>

/**
 * \file arch/host/lib/notifier.c
 * \brief Host notifier implementation file
 * \authors Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 */

#include <ipc/topology.h>
#include <rtos/alloc.h>
#include <sof/lib/notifier.h>

static struct notify *host_notify;

struct notify **arch_notify_get(void)
{
	return &host_notify;
}
