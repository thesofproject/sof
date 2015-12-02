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
#include <platform/clk.h>
#include <platform/shim.h>
#include <stdint.h>

#define NUM_CLOCKS	3

struct clk_data {
	uint32_t freq;
	uint32_t ticks_per_ms;
	struct notifier *notifier;
};

struct clk_pdata {
	struct clk_data clk[NUM_CLOCKS];
};

struct freq_table {
	uint32_t freq;
	uint32_t enc;
};

static struct clk_pdata *clk_pdata;

/* increasing frequency order */
static const struct freq_table cpu_freq[] = {
	{19200000, 0x0},
	{19200000, 0x1},
	{38400000, 0x2},
	{50000000, 0x3},	/* default */
	{100000000, 0x4},
	{200000000, 0x5},
	{267000000, 0x6},
	{343000000, 0x7},
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
}

void clock_disable(int clock)
{
}

unsigned int clock_set_freq(int clock, unsigned int hz)
{
	uint32_t idx, freq;

	switch (clock) {
	case CLK_CPU:
		idx = get_cpu_freq(hz);
		io_reg_update_bits(SHIM_BASE + SHIM_CSR,
				SHIM_CSR_DCS_MASK, cpu_freq[idx].enc);
		freq = cpu_freq[idx].enc;
		break;
	case CLK_SSP0:
	case CLK_SSP1:
		freq = 0;
		break;
	}

	return freq;
}

unsigned int clock_get_freq(int clock)
{
	return clk_pdata->clk[clock].freq;
}

unsigned int clock_ms_to_ticks(int clock, int ms)
{
	return clk_pdata->clk[clock].ticks_per_ms * ms;
}

void clock_register_notifier(int clock, struct notifier *notifier)
{

}

void init_platform_clocks(void)
{
	clk_pdata = rmalloc(RZONE_DEV, RMOD_SYS, sizeof(*clk_pdata));

	clock_set_freq(CLK_CPU, CLK_DEFAULT_CPU_HZ);
}
