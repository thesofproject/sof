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
 *
 * Simple wait for event completion and signaling with timeouts.
 */

#ifndef __INCLUDE_WAIT__
#define __INCLUDE_WAIT__

#include <stdint.h>

#include <arch/wait.h>

#include <sof/work.h>

typedef struct {
	uint32_t complete;
	struct work work;
	uint64_t timeout;
} completion_t;

static uint64_t _wait_cb(void *data, uint64_t delay)
{
	volatile completion_t *wc = (volatile completion_t*)data;

	wc->timeout = 1;
	return 0;
}

static inline uint32_t wait_is_completed(completion_t *comp)
{
	volatile completion_t *c = (volatile completion_t *)comp;

	return c->complete;
}

static inline void wait_completed(completion_t *comp)
{
	volatile completion_t *c = (volatile completion_t *)comp;

	c->complete = 1;
}

static inline void wait_init(completion_t *comp)
{
	volatile completion_t *c = (volatile completion_t *)comp;

	c->complete = 0;
	work_init(&comp->work, _wait_cb, comp, WORK_MED_PRI, WORK_ASYNC);
}

static inline void wait_clear(completion_t *comp)
{
	volatile completion_t *c = (volatile completion_t *)comp;

	c->complete = 0;
}

void wait_for_interrupt(int level);
int wait_for_completion_timeout(completion_t *comp);
void wait_delay(uint64_t number_of_clks);
int poll_for_completion_delay(completion_t *comp, uint64_t us);
int poll_for_register_delay(uint32_t reg, uint32_t mask,
			    uint32_t val, uint64_t us);

/* simple interrupt based wait for completion */
static inline void wait_for_completion(completion_t *comp)
{
	/* check for completion after every wake from IRQ */
	while (comp->complete == 0)
		wait_for_interrupt(0);
}

#endif
