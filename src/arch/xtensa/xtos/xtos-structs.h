/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __XTOS_XTOS_STRUCTS_H__
#define __XTOS_XTOS_STRUCTS_H__

#include <config.h>

#if CONFIG_SMP
#include "xtos-internal.h"
#endif

struct idc;
struct irq_task;
struct notify;
struct schedule_data;

#if CONFIG_SMP
struct thread_data {
	xtos_structures_pointers xtos_ptrs;
};

struct xtos_core_data {
	struct XtosInterruptStructure xtos_int_data;
	struct thread_data *thread_data_ptr;
};
#endif

struct core_context {
#if CONFIG_SMP
	struct thread_data td;
#endif
	struct irq_task *irq_low_task;
	struct irq_task *irq_med_task;
	struct irq_task *irq_high_task;
	struct schedule_data *sch_data;
	struct notify *notify;
	struct idc *idc;
};

#endif /* __XTOS_XTOS_STRUCTS_H__ */
