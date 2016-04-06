/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <reef/clock.h>
#include <reef/io.h>
#include <reef/reef.h>
#include <reef/list.h>
#include <reef/alloc.h>
#include <reef/notifier.h>
#include <reef/lock.h>
#include <platform/clk.h>
#include <platform/shim.h>
#include <platform/timer.h>
#include <platform/pmc.h>
#include <stdint.h>

#define NUM_CLOCKS	2

struct clk_data {
	uint32_t freq;
	uint32_t ticks_per_usec;
	spinlock_t lock;
};

struct clk_pdata {
	struct clk_data clk[NUM_CLOCKS];
};

struct freq_table {
	uint32_t freq;
	uint32_t ticks_per_usec;
	uint32_t enc;
};

static struct clk_pdata *clk_pdata;

/* increasing frequency order */
static const struct freq_table cpu_freq[] = {
	{19200000, 19, 0x0},
	{19200000, 19, 0x1},
	{38400000, 38, 0x2},
	{50000000, 50, 0x3},	/* default */
	{100000000, 100, 0x4},
	{200000000, 200, 0x5},
	{267000000, 267, 0x6},
	{343000000, 343, 0x7},
};

static const struct freq_table ssp_freq[] = {
	{19200000, 19, PMC_SET_SSP_19M2},
	{25000000, 25, PMC_SET_SSP_25M},	/* default */
};

#define CPU_DEFAULT_IDX		3
#define SSP_DEFAULT_IDX		1

static inline uint32_t get_freq(const struct freq_table *table, int size,
	unsigned int hz)
{
	uint32_t i;

	/* find lowest available frequency that is >= requested hz */
	for (i = 0; i < size; i++) {
		if (hz <= table[i].freq)
			return i;
	}

	/* not found, so return max frequency */
	return size - 1;
}

void clock_enable(int clock)
{
	switch (clock) {
	case CLK_CPU:
		break;
	case CLK_SSP:
	default:
		break;
	}
}

void clock_disable(int clock)
{
	switch (clock) {
	case CLK_CPU:
		break;
	case CLK_SSP:
	default:
		break;
	}
}

uint32_t clock_set_freq(int clock, uint32_t hz)
{
	struct clock_notify_data notify_data;
	uint32_t idx, flags;
	int err = 0;

	notify_data.old_freq = clk_pdata->clk[clock].freq;
	notify_data.old_ticks_per_usec = clk_pdata->clk[clock].ticks_per_usec;

	/* atomic context for chaning clocks */
	spin_lock_irq(clk_pdata->clk[clock].lock, flags);

	switch (clock) {
	case CLK_CPU:

		/* get nearest frequency that is >= requested Hz */
		idx = get_freq(cpu_freq, ARRAY_SIZE(cpu_freq), hz);
		notify_data.freq = cpu_freq[idx].freq;

		/* tell anyone interested we are about to change CPU freq */
		notifier_event(NOTIFIER_ID_CPU_FREQ, CLOCK_NOTIFY_PRE,
			&notify_data);

		/* set CPU frequency request for CCU */
		io_reg_update_bits(SHIM_BASE + SHIM_FR_LAT_REQ,
				SHIM_FR_LAT_CLK_MASK, cpu_freq[idx].enc);
		
		/* send freq request to SC */
		err = ipc_pmc_send_msg(PMC_SET_LPECLK);
		if (err == 0) {

			/* update clock frequency */
			clk_pdata->clk[clock].freq = cpu_freq[idx].freq;
			clk_pdata->clk[clock].ticks_per_usec =
				cpu_freq[idx].ticks_per_usec;
		}

		/* tell anyone interested we have now changed CPU freq */
		notifier_event(NOTIFIER_ID_CPU_FREQ, CLOCK_NOTIFY_POST,
			&notify_data);
		break;
	case CLK_SSP:
		/* get nearest frequency that is >= requested Hz */
		idx = get_freq(ssp_freq, ARRAY_SIZE(ssp_freq), hz);
		notify_data.freq = ssp_freq[idx].freq;

		/* tell anyone interested we are about to change CPU freq */
		notifier_event(NOTIFIER_ID_SSP_FREQ, CLOCK_NOTIFY_PRE,
			&notify_data);

		/* send SSP freq request to SC */
		err = ipc_pmc_send_msg(ssp_freq[idx].enc);
		if (err == 0) {

			/* update clock frequency */
			clk_pdata->clk[clock].freq = ssp_freq[idx].freq;
			clk_pdata->clk[clock].ticks_per_usec =
				ssp_freq[idx].ticks_per_usec;
		}

		/* tell anyone interested we have now changed CPU freq */
		notifier_event(NOTIFIER_ID_SSP_FREQ, CLOCK_NOTIFY_POST,
			&notify_data);

	default:
		break;
	}

	spin_unlock_irq(clk_pdata->clk[clock].lock, flags);
	return clk_pdata->clk[clock].freq;
}

uint32_t clock_get_freq(int clock)
{
	return clk_pdata->clk[clock].freq;
}

uint32_t clock_us_to_ticks(int clock, uint32_t us)
{
	return clk_pdata->clk[clock].ticks_per_usec * us;
}

uint32_t clock_time_elapsed(int clock, uint32_t previous, uint32_t *current)
{
	uint32_t _current;

	// TODO: change timer APIs to clk APIs ??
	switch (clock) {
	case CLK_CPU:
		_current = arch_timer_get_system();
		break;
	case CLK_SSP:
		_current = platform_timer_get(clock);
		break;
	default:
		return 0;
	}

	*current = _current;
	if (_current >= previous)
		return (_current - previous) /
			clk_pdata->clk[clock].ticks_per_usec;
	else
		return (_current + (MAX_INT - previous)) /
			clk_pdata->clk[clock].ticks_per_usec;
}

void init_platform_clocks(void)
{
	clk_pdata = rmalloc(RZONE_DEV, RMOD_SYS, sizeof(*clk_pdata));

	spinlock_init(&clk_pdata->clk[0].lock);
	spinlock_init(&clk_pdata->clk[1].lock);
	spinlock_init(&clk_pdata->clk[2].lock);
	spinlock_init(&clk_pdata->clk[3].lock);

	/* set defaults */
	clk_pdata->clk[CLK_CPU].freq = cpu_freq[CPU_DEFAULT_IDX].freq;
	clk_pdata->clk[CLK_CPU].ticks_per_usec =
			cpu_freq[CPU_DEFAULT_IDX].ticks_per_usec;
	clk_pdata->clk[CLK_SSP].freq = ssp_freq[SSP_DEFAULT_IDX].freq;
	clk_pdata->clk[CLK_SSP].ticks_per_usec =
			ssp_freq[SSP_DEFAULT_IDX].ticks_per_usec;
}
