// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/common.h>
#include <sof/drivers/ssp.h>
#include <sof/lib/clk.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/pm_runtime.h>
#include <sof/sof.h>
#include <sof/spinlock.h>

#include <cavs/version.h>

static SHARED_DATA struct clock_info platform_clocks_info[NUM_CLOCKS];

#if CAVS_VERSION == CAVS_VERSION_1_5
static inline void select_cpu_clock_hw(int freq_idx, bool release_unused)
{
	uint32_t enc = cpu_freq_enc[freq_idx];

	io_reg_update_bits(SHIM_BASE + SHIM_CLKCTL, SHIM_CLKCTL_HDCS, 0);
	io_reg_update_bits(SHIM_BASE + SHIM_CLKCTL,
			   SHIM_CLKCTL_DPCS_MASK(cpu_get_id()),
			   enc);
}
#else
static inline void select_cpu_clock_hw(int freq_idx, bool release_unused)
{
	uint32_t enc = cpu_freq_enc[freq_idx];
	uint32_t status_mask = cpu_freq_status_mask[freq_idx];

#if CONFIG_TIGERLAKE
	/* TGL specific HW recommended flow */
	if (freq_idx == CPU_HPRO_FREQ_IDX)
		pm_runtime_get(PM_RUNTIME_DSP, PWRD_BY_HPRO | (CONFIG_CORE_COUNT - 1));
#endif

	/* request clock */
	io_reg_write(SHIM_BASE + SHIM_CLKCTL,
		     io_reg_read(SHIM_BASE + SHIM_CLKCTL) | enc);

	/* wait for requested clock to be on */
	while ((io_reg_read(SHIM_BASE + SHIM_CLKSTS) &
		status_mask) != status_mask)
		idelay(PLATFORM_DEFAULT_DELAY);

	/* switch to requested clock */
	io_reg_update_bits(SHIM_BASE + SHIM_CLKCTL,
			   SHIM_CLKCTL_OSC_SOURCE_MASK, enc);

	if (release_unused) {
		/* release other clocks */
		io_reg_write(SHIM_BASE + SHIM_CLKCTL,
			     (io_reg_read(SHIM_BASE + SHIM_CLKCTL) &
			      ~SHIM_CLKCTL_OSC_REQUEST_MASK) | enc);
#if CONFIG_TIGERLAKE
		/* TGL specific HW recommended flow */
		if (freq_idx != CPU_HPRO_FREQ_IDX)
			pm_runtime_put(PM_RUNTIME_DSP, PWRD_BY_HPRO | (CONFIG_CORE_COUNT - 1));
#endif
	}

#if CONFIG_DSP_RESIDENCY_COUNTERS
	if (get_dsp_r_state() != r2_r_state) {
		if (freq_idx == CPU_LPRO_FREQ_IDX)
			report_dsp_r_state(r1_r_state);
		else
			report_dsp_r_state(r0_r_state);
	}
#endif
}
#endif

static inline void select_cpu_clock(int freq_idx, bool release_unused)
{
	struct clock_info *clk_info = clocks_get();
	int flags[CONFIG_CORE_COUNT];
	int i;

	/* lock clock for all cores */
	for (i = 0; i < CONFIG_CORE_COUNT; i++)
		spin_lock_irq(&clk_info[CLK_CPU(i)].lock, flags[i]);

	/* change clock */
	select_cpu_clock_hw(freq_idx, release_unused);
	for (i = 0; i < CONFIG_CORE_COUNT; i++)
		clk_info[CLK_CPU(i)].current_freq_idx = freq_idx;

	/* unlock clock for all cores */
	for (i = CONFIG_CORE_COUNT - 1; i >= 0; i--)
		spin_unlock_irq(&clk_info[CLK_CPU(i)].lock, flags[i]);

	platform_shared_commit(clk_info,
			       sizeof(*clk_info) * CONFIG_CORE_COUNT);
}

/* LPRO_ONLY mode */
#if CONFIG_CAVS_LPRO_ONLY

/*
 * This is call from public API, there is no clock switch for core waiti, so
 * value of value frequency index in 'active' state doesn't need to be saved in
 * any additional variable and restored in future.
 */
static inline void set_cpu_current_freq_idx(int freq_idx, bool release_unused)
{
	select_cpu_clock(freq_idx, true);
}

static void platform_clock_low_power_mode(int clock, bool enable)
{
	/* do nothing for LPRO_ONLY mode */
}

void platform_clock_on_waiti(void)
{
/* do nothing for LPRO_ONLY mode */
}

void platform_clock_on_wakeup(void)
{
/* do nothing for LPRO_ONLY mode */
}

/* USE_LPRO_IN_WAITI mode */
#elif CONFIG_CAVS_USE_LPRO_IN_WAITI

/* Store clock source that was active before going to waiti,
 * so it can be restored on wake up.
 */
static SHARED_DATA int active_freq_idx = CPU_DEFAULT_IDX;

/*
 * This is call from public API, so overwrite currently used frequency index
 * in 'active' state with new value. This value will be used in each wakeup.
 * Caller should hold the pm_runtime lock.
 */
static inline void set_cpu_current_freq_idx(int freq_idx, bool release_unused)
{
	int *uncached_freq_idx = cache_to_uncache(&active_freq_idx);

	select_cpu_clock(freq_idx, release_unused);
	*uncached_freq_idx = freq_idx;
}

static inline int get_current_freq_idx(int clock)
{
	struct clock_info *clk_info = clocks_get() + clock;

	return clk_info->current_freq_idx;
}

static inline int get_lowest_freq_idx(int clock)
{
	struct clock_info *clk_info = clocks_get() + clock;

	return clk_info->lowest_freq_idx;
}

static void platform_clock_low_power_mode(int clock, bool enable)
{
	int current_freq_idx = get_current_freq_idx(clock);
	int freq_idx = *cache_to_uncache(&active_freq_idx);

	if (enable && current_freq_idx > CPU_LPRO_FREQ_IDX)
		select_cpu_clock(CPU_LPRO_FREQ_IDX, true);
	else if (!enable && current_freq_idx != freq_idx)
		select_cpu_clock(freq_idx, true);
}

void platform_clock_on_waiti(void)
{
	struct pm_runtime_data *prd = pm_runtime_data_get();
	uint32_t flags;
	int freq_idx;
	int lowest_freq_idx;
	bool pm_is_active;

	/* hold the prd->lock for possible active_freq_idx switching */
	spin_lock_irq(&prd->lock, flags);

	freq_idx = *cache_to_uncache(&active_freq_idx);
	lowest_freq_idx = get_lowest_freq_idx(CLK_CPU(cpu_get_id()));
	pm_is_active = pm_runtime_is_active(PM_RUNTIME_DSP, PLATFORM_PRIMARY_CORE_ID);

	if (pm_is_active) {
		/* set HPRO clock if not already enabled */
		if (freq_idx != CPU_HPRO_FREQ_IDX)
			set_cpu_current_freq_idx(CPU_HPRO_FREQ_IDX, true);
	} else {
		/* set lowest clock if not already enabled */
		if (freq_idx != lowest_freq_idx)
			set_cpu_current_freq_idx(lowest_freq_idx, true);
	}

	spin_unlock_irq(&prd->lock, flags);

	/* check if waiti HPRO->LPRO switching is needed */
	pm_runtime_put(CORE_HP_CLK, cpu_get_id());
}

void platform_clock_on_wakeup(void)
{
	/* check if HPRO switching back is needed */
	pm_runtime_get(CORE_HP_CLK, cpu_get_id());
}

#else

/* Store clock source that was active before going to waiti,
 * so it can be restored on wake up.
 */
static SHARED_DATA int active_freq_idx = CPU_DEFAULT_IDX;

/*
 * This is call from public API, so overwrite currently used frequency index
 * in 'active' state with new value. This value will be used in each wakeup.
 * Caller should hold the pm_runtime lock.
 */
static inline void set_cpu_current_freq_idx(int freq_idx, bool release_unused)
{
	int *uncached_freq_idx = cache_to_uncache(&active_freq_idx);

	select_cpu_clock(freq_idx, release_unused);
	*uncached_freq_idx = freq_idx;
}

static inline int get_current_freq_idx(int clock)
{
	struct clock_info *clk_info = clocks_get() + clock;

	return clk_info->current_freq_idx;
}

static inline int get_lowest_freq_idx(int clock)
{
	struct clock_info *clk_info = clocks_get() + clock;

	return clk_info->lowest_freq_idx;
}

static void platform_clock_low_power_mode(int clock, bool enable)
{
}

void platform_clock_on_waiti(void)
{
	struct pm_runtime_data *prd = pm_runtime_data_get();
	uint32_t flags;
	int freq_idx;
	int lowest_freq_idx;
	bool pm_is_active;

	/* hold the prd->lock for possible active_freq_idx switching */
	spin_lock_irq(&prd->lock, flags);

	freq_idx = *cache_to_uncache(&active_freq_idx);
	lowest_freq_idx = get_lowest_freq_idx(CLK_CPU(cpu_get_id()));
	pm_is_active = pm_runtime_is_active(PM_RUNTIME_DSP, PLATFORM_PRIMARY_CORE_ID);

	if (pm_is_active) {
		/* set HPRO clock if not already enabled */
		if (freq_idx != CPU_HPRO_FREQ_IDX)
			set_cpu_current_freq_idx(CPU_HPRO_FREQ_IDX, true);
	} else {
		/* set lowest clock if not already enabled */
		if (freq_idx != lowest_freq_idx)
			set_cpu_current_freq_idx(lowest_freq_idx, true);
	}

	spin_unlock_irq(&prd->lock, flags);
}

void platform_clock_on_wakeup(void)
{
	struct pm_runtime_data *prd = pm_runtime_data_get();
	uint32_t flags;
	int current_idx;
	int target_idx;

	/* hold the prd->lock for possible active_freq_idx switching */
	spin_lock_irq(&prd->lock, flags);

	current_idx = get_current_freq_idx(CLK_CPU(cpu_get_id()));
	target_idx = *cache_to_uncache(&active_freq_idx);

	/* restore the active cpu freq_idx manually */
	if (current_idx != target_idx)
		set_cpu_current_freq_idx(target_idx, true);

	spin_unlock_irq(&prd->lock, flags);
}

#endif

static int clock_platform_set_cpu_freq(int clock, int freq_idx)
{
	set_cpu_current_freq_idx(freq_idx, true);
	return 0;
}

void platform_clock_init(struct sof *sof)
{
	uint32_t platform_lowest_clock = CPU_LOWEST_FREQ_IDX;
	int i;

	sof->clocks =
		cache_to_uncache((struct clock_info *)platform_clocks_info);

#if CAVS_VERSION == CAVS_VERSION_2_5
	/*
	 * Check HW version clock capabilities
	 * Try to request WOV_CRO clock, if it fails use LPRO clock
	 */

	shim_write(SHIM_CLKCTL, shim_read(SHIM_CLKCTL) | SHIM_CLKCTL_WOV_CRO_REQUEST);
	if (shim_read(SHIM_CLKCTL) & SHIM_CLKCTL_WOV_CRO_REQUEST)
		shim_write(SHIM_CLKCTL, shim_read(SHIM_CLKCTL) & ~SHIM_CLKCTL_WOV_CRO_REQUEST);
	else
		platform_lowest_clock = CPU_LPRO_FREQ_IDX;
#endif

	for (i = 0; i < CONFIG_CORE_COUNT; i++) {
		sof->clocks[i] = (struct clock_info) {
			.freqs_num = NUM_CPU_FREQ,
			.freqs = cpu_freq,
			.default_freq_idx = CPU_DEFAULT_IDX,
			.current_freq_idx = CPU_DEFAULT_IDX,
			.lowest_freq_idx = platform_lowest_clock,
			.notification_id = NOTIFIER_ID_CPU_FREQ,
			.notification_mask = NOTIFIER_TARGET_CORE_MASK(i),
			.set_freq = clock_platform_set_cpu_freq,
			.low_power_mode = platform_clock_low_power_mode,
		};

		spinlock_init(&sof->clocks[i].lock);
	}

	sof->clocks[CLK_SSP] = (struct clock_info) {
		.freqs_num = NUM_SSP_FREQ,
		.freqs = ssp_freq,
		.default_freq_idx = SSP_DEFAULT_IDX,
		.current_freq_idx = SSP_DEFAULT_IDX,
		.notification_id = NOTIFIER_ID_SSP_FREQ,
		.notification_mask = NOTIFIER_TARGET_CORE_ALL_MASK,
		.set_freq = NULL,
	};

	spinlock_init(&sof->clocks[CLK_SSP].lock);

	platform_shared_commit(sof->clocks, sizeof(*sof->clocks) * NUM_CLOCKS);
}
