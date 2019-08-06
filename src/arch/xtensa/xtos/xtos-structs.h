/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __XTOS_XTOS_STRUCTS_H__
#define __XTOS_XTOS_STRUCTS_H__

#include "xtos-internal.h"
#include <sof/lib/memory.h>
#include <config.h>
#include <xtensa/xtruntime-frames.h>
#include <stdint.h>

struct idc;
struct irq_task;
struct notify;
struct schedule_data;

struct thread_data {
	xtos_structures_pointers xtos_ptrs;
	volatile xtos_task_context *xtos_active_task;
};

struct xtos_core_data {
#if CONFIG_SMP
	struct XtosInterruptStructure xtos_int_data;
#endif
	uint8_t xtos_stack_for_interrupt_1[SOF_STACK_SIZE];
	uint8_t xtos_stack_for_interrupt_2[SOF_STACK_SIZE];
	uint8_t xtos_stack_for_interrupt_3[SOF_STACK_SIZE];
	uint8_t xtos_stack_for_interrupt_4[SOF_STACK_SIZE];
	uint8_t xtos_stack_for_interrupt_5[SOF_STACK_SIZE];
	uintptr_t xtos_saved_sp;
	struct thread_data *thread_data_ptr;
};

struct core_context {
	struct thread_data td;
	struct irq_task *irq_low_task;
	struct irq_task *irq_med_task;
	struct irq_task *irq_high_task;
	struct schedule_data *sch_data;
	struct notify *notify;
	struct idc *idc;
};

#endif /* __XTOS_XTOS_STRUCTS_H__ */
