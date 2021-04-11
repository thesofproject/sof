// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

/**
 * \file lib/pm_runtime.c
 * \brief Runtime power management implementation
 * \author Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/memory.h>
#include <sof/lib/pm_runtime.h>
#include <sof/lib/uuid.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <ipc/topology.h>
#include <stdint.h>

/* d7f6712d-131c-45a7-82ed-6aa9dc2291ea */
DECLARE_SOF_UUID("pm-runtime", pm_runtime_uuid, 0xd7f6712d, 0x131c, 0x45a7,
		 0x82, 0xed, 0x6a, 0xa9, 0xdc, 0x22, 0x91, 0xea);

DECLARE_TR_CTX(pm_tr, SOF_UUID(pm_runtime_uuid), LOG_LEVEL_INFO);

void pm_runtime_init(struct sof *sof)
{
	sof->prd = rzalloc(SOF_MEM_ZONE_SYS_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*sof->prd));
	spinlock_init(&sof->prd->lock);

	platform_pm_runtime_init(sof->prd);

}

void pm_runtime_get(enum pm_runtime_context context, uint32_t index)
{
	tr_dbg(&pm_tr, "pm_runtime_get() context %d index %d", context, index);

	switch (context) {
	default:
		platform_pm_runtime_get(context, index, RPM_ASYNC);
		break;
	}
}

void pm_runtime_get_sync(enum pm_runtime_context context, uint32_t index)
{
	tr_dbg(&pm_tr, "pm_runtime_get_sync() context %d index %d", context,
	       index);

	switch (context) {
	default:
		platform_pm_runtime_get(context, index, 0);
		break;
	}
}

void pm_runtime_put(enum pm_runtime_context context, uint32_t index)
{
	tr_dbg(&pm_tr, "pm_runtime_put() context %d index %d", context, index);

	switch (context) {
	default:
		platform_pm_runtime_put(context, index, RPM_ASYNC);
		break;
	}
}

void pm_runtime_put_sync(enum pm_runtime_context context, uint32_t index)
{
	tr_dbg(&pm_tr, "pm_runtime_put_sync() context %d index %d", context,
	       index);

	switch (context) {
	default:
		platform_pm_runtime_put(context, index, 0);
		break;
	}
}

void pm_runtime_enable(enum pm_runtime_context context, uint32_t index)
{
	tr_dbg(&pm_tr, "pm_runtime_enable() context %d index %d", context,
	       index);

	switch (context) {
	default:
		platform_pm_runtime_enable(context, index);
		break;
	}
}

void pm_runtime_disable(enum pm_runtime_context context, uint32_t index)
{
	tr_dbg(&pm_tr, "pm_runtime_disable() context %d index %d", context,
	       index);

	switch (context) {
	default:
		platform_pm_runtime_disable(context, index);
		break;
	}
}

bool pm_runtime_is_active(enum pm_runtime_context context, uint32_t index)
{
	tr_dbg(&pm_tr, "pm_runtime_is_active() context %d index %d", context,
	       index);

	switch (context) {
	default:
		return platform_pm_runtime_is_active(context, index);
	}
}

#if CONFIG_DSP_RESIDENCY_COUNTERS
void init_dsp_r_state(enum dsp_r_state r_state)
{
	struct pm_runtime_data *prd = pm_runtime_data_get();
	struct r_counters_data *r_counters;

	r_counters = rzalloc(SOF_MEM_ZONE_SYS_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*r_counters));
	prd->r_counters = r_counters;

	r_counters->ts = platform_timer_get(timer_get());
	r_counters->cur_r_state = r_state;
}

void report_dsp_r_state(enum dsp_r_state r_state)
{
	struct r_counters_data *r_counters = pm_runtime_data_get()->r_counters;
	uint64_t ts, delta;

	/* It is possible to call report_dsp_r_state in early platform init from
	 * pm_runtime_disable so a safe check for r_counters is required
	 */
	if (!r_counters || r_counters->cur_r_state == r_state)
		return;

	ts = platform_timer_get(timer_get());
	delta = ts - r_counters->ts;

	delta += mailbox_sw_reg_read64(SRAM_REG_R_STATE_TRACE_BASE +
				       r_counters->cur_r_state *
				       sizeof(uint64_t));

	mailbox_sw_reg_write64(SRAM_REG_R_STATE_TRACE_BASE + r_counters->cur_r_state
			       * sizeof(uint64_t), delta);

	r_counters->cur_r_state = r_state;
	r_counters->ts = ts;
}

enum dsp_r_state get_dsp_r_state(void)
{
	struct r_counters_data *r_counters = pm_runtime_data_get()->r_counters;

	return r_counters ? r_counters->cur_r_state : 0;
}
#endif
