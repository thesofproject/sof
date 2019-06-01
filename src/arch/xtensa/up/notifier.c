// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

/**
 * \file arch/xtensa/up/notifier.c
 * \brief Xtensa UP notifier implementation file
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include <sof/notifier.h>

/** \brief Notify data pointer. */
static struct notify *notify;

struct notify **arch_notify_get(void)
{
	return &notify;
}
