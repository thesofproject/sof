// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/common.h>
#include <sof/drivers/ssp.h>
#include <sof/lib/clk.h>
#include <sof/lib/notifier.h>

static struct clock_info platform_clocks_info[NUM_CLOCKS];

struct clock_info *clocks = platform_clocks_info;

#if CONFIG_CAVS_USE_LPRO_IN_WAITI
/* Track freq_idx value, so it can be stored before switching to LPRO. */
static int cpu_current_freq_idx;

static inline int get_cpu_current_freq_idx(void)
{
	return *cache_to_uncache(&cpu_current_freq_idx);
}

static inline void set_cpu_current_freq_idx(int freq_idx)
{
	*cache_to_uncache(&cpu_current_freq_idx) = freq_idx;
}
#else
static inline void set_cpu_current_freq_idx(int freq_idx)
{
}
#endif

#if CAVS_VERSION == CAVS_VERSION_1_5
static inline void select_cpu_clock(int freq_idx, bool release_unused)
{
	uint32_t enc = cpu_freq_enc[freq_idx];

	io_reg_update_bits(SHIM_BASE + SHIM_CLKCTL, SHIM_CLKCTL_HDCS, 0);
	io_reg_update_bits(SHIM_BASE + SHIM_CLKCTL,
			   SHIM_CLKCTL_DPCS_MASK(cpu_get_id()),
			   enc);

	set_cpu_current_freq_idx(freq_idx);
}
#else
static inline void select_cpu_clock(int freq_idx, bool release_unused)
{
	uint32_t enc = cpu_freq_enc[freq_idx];
	uint32_t status_mask = cpu_freq_status_mask[freq_idx];

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
	}

	set_cpu_current_freq_idx(freq_idx);
}
#endif

static int clock_platform_set_cpu_freq(int clock, int freq_idx)
{
	select_cpu_clock(freq_idx, true);
	return 0;
}

#if CONFIG_CAVS_USE_LPRO_IN_WAITI
/* Store clock source that was active before going to waiti,
 * so it can be restored on wake up.
 */
static int active_freq_idx = CPU_DEFAULT_IDX;

void platform_clock_on_wakeup(void)
{
	int freq_idx = *cache_to_uncache(&active_freq_idx);

	if (freq_idx != get_cpu_current_freq_idx())
		select_cpu_clock(freq_idx, true);
}

void platform_clock_on_waiti(void)
{
	int freq_idx = get_cpu_current_freq_idx();

	*cache_to_uncache(&active_freq_idx) = freq_idx;

	if (freq_idx != CPU_LPRO_FREQ_IDX)
		/* LPRO requests are fast, but requests for other ROs
		 * can take a lot of time. That's why it's better to
		 * not release active clock just for waiti,
		 * so they can be switched without delay on wake up.
		 */
		select_cpu_clock(CPU_LPRO_FREQ_IDX, false);
}
#endif

void platform_clock_init(void)
{
	int i;

	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		platform_clocks_info[i] = (struct clock_info) {
			.freqs_num = NUM_CPU_FREQ,
			.freqs = cpu_freq,
			.default_freq_idx = CPU_DEFAULT_IDX,
			.notification_id = NOTIFIER_ID_CPU_FREQ,
			.notification_mask = NOTIFIER_TARGET_CORE_MASK(i),
			.set_freq = clock_platform_set_cpu_freq,
		};
	}

	platform_clocks_info[CLK_SSP] = (struct clock_info) {
		.freqs_num = NUM_SSP_FREQ,
		.freqs = ssp_freq,
		.default_freq_idx = SSP_DEFAULT_IDX,
		.notification_id = NOTIFIER_ID_SSP_FREQ,
		.notification_mask = NOTIFIER_TARGET_CORE_ALL_MASK,
		.set_freq = NULL,
	};

	set_cpu_current_freq_idx(CPU_DEFAULT_IDX);
}
