// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

/**
 * \file arch/xtensa/up/schedule.c
 * \brief Xtensa UP schedule implementation file
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include <sof/schedule/schedule.h>

/** \brief Schedule data pointer */
static struct schedule_data *sch;

struct schedule_data **arch_schedule_get_data(void)
{
	return &sch;
}
