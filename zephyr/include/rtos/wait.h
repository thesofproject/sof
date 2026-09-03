/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __ZEPHYR_RTOS_WAIT_H__
#define __ZEPHYR_RTOS_WAIT_H__

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/time_units.h>
#include <rtos/timer.h>

static inline void wait_delay_us(uint64_t us)
{
	k_busy_wait(us);
}

static inline void wait_delay(uint64_t number_of_clks)
{
	k_busy_wait(k_cyc_to_us_floor64(number_of_clks));
}

static inline void wait_delay_ms(uint64_t ms)
{
	k_busy_wait(ms * 1000);
}

int poll_for_register_delay(uint32_t reg, uint32_t mask,
			    uint32_t val, uint64_t us);

/* TODO: this is tmp used by some IMX IPC drivers that have not transitioned
 * to Zephyr. To be removed after drivers migrated to Zephyr. It's xtensa only too.
 */
static inline void wait_for_interrupt(int level)
{
	__asm__ volatile("waiti 0");
}

#endif /* __ZEPHYR_RTOS_WAIT_H__ */
