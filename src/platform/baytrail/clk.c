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
#include <stdint.h>

#define NUM_CLOCKS	3
#define CLOCK_DEF(x)	x, x / 1000

struct clk_data {
	uint32_t freq;
	uint32_t ticks_per_msec;
	spinlock_t lock;
};

struct clk_pdata {
	struct clk_data clk[NUM_CLOCKS];
};

struct freq_table {
	uint32_t freq;
	uint32_t ticks_per_msec;
	uint32_t enc;
};

static struct clk_pdata *clk_pdata;

/* increasing frequency order */
static const struct freq_table cpu_freq[] = {
	{CLOCK_DEF(19200000), 0x0},
	{CLOCK_DEF(19200000), 0x1},
	{CLOCK_DEF(38400000), 0x2},
	{CLOCK_DEF(50000000), 0x3},	/* default */
	{CLOCK_DEF(100000000), 0x4},
	{CLOCK_DEF(200000000), 0x5},
	{CLOCK_DEF(267000000), 0x6},
	{CLOCK_DEF(343000000), 0x7},
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
	default:
		break;
	}
}

unsigned int clock_set_freq(int clock, unsigned int hz)
{
	struct clock_notify_data notify_data;
	uint32_t idx, flags;

	notify_data.old_freq = clk_pdata->clk[clock].freq;
	notify_data.old_ticks_per_msec = clk_pdata->clk[clock].ticks_per_msec;

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

		/* change CPU frequency */
		io_reg_update_bits(SHIM_BASE + SHIM_CSR,
				SHIM_CSR_DCS_MASK, cpu_freq[idx].enc);

		/* update clock freqency */
		clk_pdata->clk[clock].freq = cpu_freq[idx].freq;
		clk_pdata->clk[clock].ticks_per_msec =
			cpu_freq[idx].ticks_per_msec;

		/* tell anyone interested we have now changed CPU freq */
		notifier_event(NOTIFIER_ID_CPU_FREQ, CLOCK_NOTIFY_POST,
			&notify_data);
		break;
	case CLK_SSP0:
	case CLK_SSP1:
	default:
		break;
	}

	spin_unlock_irq(clk_pdata->clk[clock].lock, flags);

	return clk_pdata->clk[clock].freq;
}

unsigned int clock_get_freq(int clock)
{
	return clk_pdata->clk[clock].freq;
}

unsigned int clock_ms_to_ticks(int clock, int ms)
{
	return clk_pdata->clk[clock].ticks_per_msec * ms;
}

void init_platform_clocks(void)
{
	clk_pdata = rmalloc(RZONE_DEV, RMOD_SYS, sizeof(*clk_pdata));

	spinlock_init(&clk_pdata->clk[0].lock);
	spinlock_init(&clk_pdata->clk[1].lock);
	spinlock_init(&clk_pdata->clk[2].lock);

	/* Set CPU to default frequency for booting */
	clock_set_freq(CLK_CPU, CLK_DEFAULT_CPU_HZ);
}
