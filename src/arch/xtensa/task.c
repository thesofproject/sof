/*
 * Copyright (c) 2017, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 */

#include <reef/schedule.h>
#include <reef/interrupt.h>
#include <platform/platform.h>
#include <reef/debug.h>
#include <stdint.h>
#include <errno.h>

static struct task *_irq_low_task = NULL;
static struct task *_irq_med_task = NULL;
static struct task *_irq_high_task = NULL;

static inline uint32_t task_get_irq(struct task *task)
{
	uint32_t irq;

	switch (task->priority) {
	case TASK_PRI_MED + 1 ... TASK_PRI_LOW:
		irq = PLATFORM_IRQ_TASK_LOW;
		break;
	case TASK_PRI_HIGH ... TASK_PRI_MED - 1:
		irq = PLATFORM_IRQ_TASK_HIGH;
		break;
	case TASK_PRI_MED:
	default:
		irq = PLATFORM_IRQ_TASK_MED;
		break;
	}

	return irq;
}

static inline void task_set_data(struct task *task)
{
	switch (task->priority) {
	case TASK_PRI_MED + 1 ... TASK_PRI_LOW:
		_irq_low_task = task;
		break;
	case TASK_PRI_HIGH ... TASK_PRI_MED - 1:
		_irq_high_task = task;
		break;
	case TASK_PRI_MED:
	default:
		_irq_med_task = task;
		break;
	}
}

static void _irq_low(void *arg)
{
	struct task *task = *(struct task **)arg;
	uint32_t irq;

	if (task->func)
		task->func(task->data);

	schedule_task_complete(task);
	irq = task_get_irq(task);
	interrupt_clear(irq);
}

static void _irq_med(void *arg)
{
	struct task *task = *(struct task **)arg;
	uint32_t irq;

	if (task->func)
		task->func(task->data);

	schedule_task_complete(task);
	irq = task_get_irq(task);
	interrupt_clear(irq);
}

static void _irq_high(void *arg)
{
	struct task *task = *(struct task **)arg;
	uint32_t irq;

	if (task->func)
		task->func(task->data);

	schedule_task_complete(task);
	irq = task_get_irq(task);
	interrupt_clear(irq);
}

/* architecture specific method of running task */
void arch_run_task(struct task *task)
{
	uint32_t irq;

	task_set_data(task);
	irq = task_get_irq(task);
	interrupt_set(irq);
}

int arch_init_tasks(void)
{
	interrupt_register(PLATFORM_IRQ_TASK_LOW, _irq_low, &_irq_low_task);
	interrupt_enable(PLATFORM_IRQ_TASK_LOW);

	interrupt_register(PLATFORM_IRQ_TASK_MED, _irq_med, &_irq_med_task);
	interrupt_enable(PLATFORM_IRQ_TASK_MED);

	interrupt_register(PLATFORM_IRQ_TASK_HIGH, _irq_high, &_irq_high_task);
	interrupt_enable(PLATFORM_IRQ_TASK_HIGH);

	return 0;
}
