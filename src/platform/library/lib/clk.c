// SPDX-License-Identifier: BSD-3-Clause

#include <rtos/clk.h>
#include <sof/lib/cpu.h>
#include <platform/lib/telemetry_sched.h>

uint32_t __attribute__((weak)) host_workload_sim_get_core_freq(int core);

static uint64_t get_active_core_freq_hz(int clock)
{
	(void)clock;
	if (host_workload_sim_get_core_freq) {
		uint32_t hz = host_workload_sim_get_core_freq(cpu_get_id());

		if (hz > 0)
			return (uint64_t)hz;
	}
	return TB_DSP_NOMINAL_HZ;
}

uint64_t clock_ms_to_ticks(int clock, uint64_t ms)
{
	return (get_active_core_freq_hz(clock) * ms) / 1000ULL;
}

uint64_t clock_us_to_ticks(int clock, uint64_t us)
{
	return (get_active_core_freq_hz(clock) * us) / 1000000ULL;
}

uint64_t clock_ns_to_ticks(int clock, uint64_t ns)
{
	return (get_active_core_freq_hz(clock) * ns) / 1000000000ULL;
}
