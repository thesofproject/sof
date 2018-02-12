/*
 * Copyright (c) 2016, Intel Corporation
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
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#include <platform/timer.h>
#include <platform/shim.h>
#include <reef/debug.h>
#include <reef/audio/component.h>
#include <stdint.h>

void platform_timer_start(struct timer *timer)
{
	/* run timer */
	shim_write64(SHIM_DSPWCT0C, 0);
	shim_write(SHIM_DSPWCTCS,
		shim_read(SHIM_DSPWCTCS) | SHIM_DSPWCTCS_T0A);
}

void platform_timer_stop(struct timer *timer)
{
	/* stop timer */
	shim_write64(SHIM_DSPWCT0C, 0);
	shim_write(SHIM_DSPWCTCS,
		shim_read(SHIM_DSPWCTCS) & ~SHIM_DSPWCTCS_T0A);
}

int platform_timer_set(struct timer *timer, uint64_t ticks)
{
	/* a tick value of 0 will not generate an IRQ */
	if (ticks == 0)
		ticks = 1;

	/* set new value and run */
	shim_write64(SHIM_DSPWCT0C, (uint64_t)ticks);
	shim_write(SHIM_DSPWCTCS,
		shim_read(SHIM_DSPWCTCS) | SHIM_DSPWCTCS_T0A);

	return 0;
}

void platform_timer_clear(struct timer *timer)
{
	/* write 1 to clear the timer interrupt */
	shim_write(SHIM_DSPWCTCS,
		shim_read(SHIM_DSPWCTCS) | SHIM_DSPWCTCS_T0T);
}

uint64_t platform_timer_get(struct timer *timer)
{
//	return arch_timer_get_system(timer);
	return (uint64_t)shim_read64(SHIM_DSPWC);
}

/* get timestamp for host stream DMA position */
void platform_host_timestamp(struct comp_dev *host,
	struct sof_ipc_stream_posn *posn)
{
	int err;

	/* get host postion */
	err = comp_position(host, posn);
	if (err == 0)
		posn->flags |= SOF_TIME_HOST_VALID;
}

/* get timestamp for DAI stream DMA position */
void platform_dai_timestamp(struct comp_dev *dai,
	struct sof_ipc_stream_posn *posn)
{
	int err;

	/* get DAI postion */
	err = comp_position(dai, posn);
	if (err == 0)
		posn->flags |= SOF_TIME_DAI_VALID;

	/* get SSP wallclock - DAI sets this to stream start value */
	posn->wallclock = shim_read64(SHIM_DSPWC) - posn->wallclock;
	posn->flags |= SOF_TIME_WALL_VALID;
}

/* get current wallclock for componnent */
void platform_dai_wallclock(struct comp_dev *dai, uint64_t *wallclock)
{
	*wallclock = shim_read64(SHIM_DSPWC);
}

int platform_timer_register(struct timer *timer,
	void (*handler)(void *arg), void *arg)
{
	return interrupt_register(timer->irq, handler, arg);
}

