// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.

#include <sof/ipc/common.h>
#include <sof/lib/pm_runtime.h>
#include <sof/lib/uuid.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <sof_versions.h>
#include <stdint.h>
#include <zephyr/pm/policy.h>
#include <sof/ipc/driver.h>

LOG_MODULE_REGISTER(power, CONFIG_SOF_LOG_LEVEL);

/* 76cc9773-440c-4df9-95a8-72defe7796fc */
SOF_DEFINE_UUID("power", power_uuid, 0x76cc9773, 0x440c, 0x4df9,
		 0x95, 0xa8, 0x72, 0xde, 0xfe, 0x77, 0x96, 0xfc);

DECLARE_TR_CTX(power_tr, SOF_UUID(power_uuid), LOG_LEVEL_INFO);

#if defined(CONFIG_PM_POLICY_CUSTOM)
const struct pm_state_info *pm_policy_next_state(uint8_t cpu, int32_t ticks)
{
	unsigned int num_cpu_states;
	const struct pm_state_info *cpu_states;

	num_cpu_states = pm_state_cpu_get_all(cpu, &cpu_states);

	for (int i = num_cpu_states - 1; i >= 0; i--) {
		const struct pm_state_info *state = &cpu_states[i];
		uint32_t min_residency, exit_latency;

		/* policy cannot lead to D3 */
		if (state->state == PM_STATE_SOFT_OFF)
			continue;

		/* check if there is a lock on state + substate */
		if (pm_policy_state_lock_is_active(state->state, state->substate_id))
			continue;

		/* checking conditions for D0i3 */
		if (state->state == PM_STATE_RUNTIME_IDLE) {
			/* skipping when secondary cores are active */
			if (cpu_enabled_cores() & ~BIT(PLATFORM_PRIMARY_CORE_ID))
				continue;

			/* skipping when some ipc task is not finished */
			if (ipc_get()->task_mask || !ipc_platform_poll_is_host_ready())
				continue;
		}

		min_residency = k_us_to_ticks_ceil32(state->min_residency_us);
		exit_latency = k_us_to_ticks_ceil32(state->exit_latency_us);

		if (ticks == K_TICKS_FOREVER ||
		    (ticks >= (min_residency + exit_latency))) {
			tr_dbg(&power_tr, "transition to state %x (min_residency = %u, exit_latency = %u)",
			       state->state, min_residency, exit_latency);
			return state;
		}
	}

	return NULL;
}
#endif /* CONFIG_PM_POLICY_CUSTOM */

void platform_pm_runtime_enable(uint32_t context, uint32_t index)
{
	switch (context) {
	case PM_RUNTIME_DSP:
		pm_policy_state_lock_put(PM_STATE_RUNTIME_IDLE, PM_ALL_SUBSTATES);
		tr_dbg(&power_tr, "removing prevent on d0i3 (lock is active=%d)",
		       pm_policy_state_lock_is_active(PM_STATE_RUNTIME_IDLE, PM_ALL_SUBSTATES));
		break;
	default:
		break;
	}
}

void platform_pm_runtime_disable(uint32_t context, uint32_t index)
{
	switch (context) {
	case PM_RUNTIME_DSP:
		tr_dbg(&power_tr, "putting prevent on d0i3");
		pm_policy_state_lock_get(PM_STATE_RUNTIME_IDLE, PM_ALL_SUBSTATES);
		break;
	default:
		break;
	}
}

void platform_pm_runtime_init(struct pm_runtime_data *prd)
{ }

void platform_pm_runtime_get(enum pm_runtime_context context, uint32_t index,
			     uint32_t flags)
{ }

void platform_pm_runtime_put(enum pm_runtime_context context, uint32_t index,
			     uint32_t flags)
{ }

void platform_pm_runtime_prepare_d0ix_en(uint32_t index)
{ }

void platform_pm_runtime_power_off(void)
{ }
