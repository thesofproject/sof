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
 * \file platform/apollolake/pm_runtime.c
 * \brief Runtime power management implementation specific for Apollolake
 * \author Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include <sof/alloc.h>
#include <platform/platform.h>
#include <platform/pm_runtime.h>
#include <platform/cavs/pm_runtime.h>
#include <platform/power_down.h>

/** \brief Runtime power management data pointer. */
struct pm_runtime_data *_prd;

void platform_pm_runtime_init(struct pm_runtime_data *prd)
{
	struct platform_pm_runtime_data *pprd;

	_prd = prd;

	pprd = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*pprd));
	_prd->platform_data = pprd;
}

void platform_pm_runtime_get(enum pm_runtime_context context)
{
	/* Action based on context */
}

void platform_pm_runtime_put(enum pm_runtime_context context)
{
	switch (context) {
	case PM_RUNTIME_HOST_DMA_L1:
		cavs_pm_runtime_force_host_dma_l1_exit();
		break;
	}
}

void platform_pm_runtime_power_off(void)
{
	uint32_t hpsram_mask[PLATFORM_HPSRAM_SEGMENTS];
	//TODO: add LDO control for LP SRAM - set LDO BYPASS & LDO ON
	//TODO: mask to be used in the future for run-time power management of
	//SRAM banks
	/* power down entire HPSRAM */
	hpsram_mask[0] = 0x1;

	power_down(true, hpsram_mask);
}

