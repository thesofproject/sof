// SPDX-License-Identifier: BSD-3-Clause

#include <rtos/clk.h>

#ifndef __ZEPHYR__
uint64_t clock_ms_to_ticks(int clock, uint64_t ms)
{
	return 0;
}

uint64_t clock_us_to_ticks(int clock, uint64_t us)
{
	return 0;
}

uint64_t clock_ns_to_ticks(int clock, uint64_t ns)
{
	return 0;
}
#endif /* __ZEPHYR__ */
