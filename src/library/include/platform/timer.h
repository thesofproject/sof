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
 */


#ifndef __PLATFORM_HOST_TIMER_H__
#define __PLATFORM_HOST_TIMER_H__

#include <stdint.h>
#include <sof/timer.h>
#include <platform/interrupt.h>

struct comp_dev;
struct sof_ipc_stream_posn;

int platform_timer_set(struct timer *timer, uint64_t ticks);
void platform_timer_clear(struct timer *timer);
uint64_t platform_timer_get(struct timer *timer);
void platform_timer_start(struct timer *timer);
void platform_timer_stop(struct timer *timer);
int platform_timer_register(struct timer *timer,
			    void (*handler)(void *arg), void *arg);

/* get timestamp for host stream DMA position */
static inline void platform_host_timestamp(struct comp_dev *host,
	struct sof_ipc_stream_posn *posn) {}

/* get timestamp for DAI stream DMA position */
static inline void platform_dai_timestamp(struct comp_dev *dai,
	struct sof_ipc_stream_posn *posn) {}

/* get current wallclock for componnent */
static inline void platform_dai_wallclock(struct comp_dev *dai,
	uint64_t *wallclock) {}

#endif
