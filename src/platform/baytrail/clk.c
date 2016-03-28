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

#define NUM_CLOCKS	4

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

static uint32_t get_cpu_freq(unsigned int hz)
{
	uint32_t i;

	/* find lowest available frequency that is >= requested hz */
	for (i = 0; i < ARRAY_SIZE(cpu_freq); i++) {
		if (hz <= cpu_freq[i].freq)
			return i;
	}

	/* not found, so return max frequency */
	return ARRAY_SIZE(cpu_freq) - 1;
}

void clock_enable(int clock)
{
	switch (clock) {
	case CLK_CPU:
		break;
	case CLK_SSP0:
	case CLK_SSP1:
	case CLK_SSP2:
	default:
		break;
	}
}

void clock_disable(int clock)
{
	switch (clock) {
	case CLK_CPU:
		break;
	case CLK_SSP0:
	case CLK_SSP1:
	case CLK_SSP2:
	default:
		break;
	}
}

uint32_t clock_set_freq(int clock, uint32_t hz)
{
	struct clock_notify_data notify_data;
	uint32_t idx, flags;
	int err;

	notify_data.old_freq = clk_pdata->clk[clock].freq;
	notify_data.old_ticks_per_usec = clk_pdata->clk[clock].ticks_per_usec;

	/* atomic context for chaning clocks */
	spin_lock_irq(clk_pdata->clk[clock].lock, flags);

	switch (clock) {
	case CLK_CPU:
		/* get nearest frequency that is >= requested Hz */
		idx = get_cpu_freq(hz);
		notify_data.freq = cpu_freq[idx].freq;

		/* tell anyone interested we are about to change CPU freq */
		notifier_event(NOTIFIER_ID_CPU_FREQ, CLOCK_NOTIFY_PRE,
			&notify_data);

		/* set CPU frequency request for CCU */
		io_reg_update_bits(SHIM_BASE + SHIM_FR_LAT_REQ,
				SHIM_FR_LAT_CLK_MASK, cpu_freq[idx].enc);
		
		/* send freq request to SC */
		err = ipc_pmc_send_msg(PMC_SET_LPECLK);
		if (err < 0)
			break;

dbg_val_at(shim_read(SHIM_FR_LAT_REQ), 12);
dbg_val_at(shim_read(SHIM_CLKCTL), 13);
dbg_val_at(shim_read(SHIM_MISC), 14);

		/* update clock frequency */
		clk_pdata->clk[clock].freq = cpu_freq[idx].freq;
		clk_pdata->clk[clock].ticks_per_usec =
			cpu_freq[idx].ticks_per_usec;

		/* tell anyone interested we have now changed CPU freq */
		notifier_event(NOTIFIER_ID_CPU_FREQ, CLOCK_NOTIFY_POST,
			&notify_data);
		break;
	case CLK_SSP0:
	case CLK_SSP1:
	case CLK_SSP2:
		//TODO: currently hard coded for 25M
		/* get nearest frequency that is >= requested Hz */
		notify_data.freq = 25000000;

		/* tell anyone interested we are about to change CPU freq */
		notifier_event(NOTIFIER_ID_CPU_FREQ, CLOCK_NOTIFY_PRE,
			&notify_data);

		/* change CPU frequency */
		// TODO

		/* update clock freqency */
		clk_pdata->clk[clock].freq = 25000000;
		clk_pdata->clk[clock].ticks_per_usec = 25;

		/* tell anyone interested we have now changed CPU freq */
		notifier_event(NOTIFIER_ID_CPU_FREQ, CLOCK_NOTIFY_POST,
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
	case CLK_SSP0:
	case CLK_SSP1:
	case CLK_SSP2:
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

	/* Set CPU to default frequency for booting */
	clock_set_freq(CLK_CPU, 343000000);
	clock_set_freq(CLK_SSP0, 25000000);
	clock_set_freq(CLK_SSP1, 25000000);
	clock_set_freq(CLK_SSP2, 25000000);
}
