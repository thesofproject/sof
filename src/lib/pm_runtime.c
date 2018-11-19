/*
 * Copyright (c) 2018, Intel Corporation
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
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file lib/pm_runtime.c
 * \brief Runtime power management implementation
 * \author Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include <sof/pm_runtime.h>
#include <sof/alloc.h>
#include <platform/pm_runtime.h>

/** \brief Runtime power management data pointer. */
static struct pm_runtime_data *prd;

void pm_runtime_init(void)
{
	trace_pm("pm_runtime_init()");

	prd = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*prd));
	spinlock_init(&prd->lock);

	platform_pm_runtime_init(prd);
}

void pm_runtime_get(enum pm_runtime_context context, uint32_t index)
{
	tracev_pm("pm_runtime_get()");

	switch (context) {
	default:
		platform_pm_runtime_get(context, index, RPM_ASYNC);
		break;
	}
}

void pm_runtime_get_sync(enum pm_runtime_context context, uint32_t index)
{
	tracev_pm("pm_runtime_get_sync()");

	switch (context) {
	default:
		platform_pm_runtime_get(context, index, 0);
		break;
	}
}

void pm_runtime_put(enum pm_runtime_context context, uint32_t index)
{
	tracev_pm("pm_runtime_put()");

	switch (context) {
	default:
		platform_pm_runtime_put(context, index, RPM_ASYNC);
		break;
	}
}

void pm_runtime_put_sync(enum pm_runtime_context context, uint32_t index)
{
	tracev_pm("pm_runtime_put_sync()");

	switch (context) {
	default:
		platform_pm_runtime_put(context, index, 0);
		break;
	}
}
