/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

#ifndef __ZEPHYR_RTOS_TIMER_H__
#define __ZEPHYR_RTOS_TIMER_H__

#include <stdint.h>
#include <sof/lib/cpu.h>
#include <sof/sof.h>
#include <sof/platform.h>

struct comp_dev;
struct sof_ipc_stream_posn;

static inline uint64_t sof_cycle_get_64(void)
{
	if (IS_ENABLED(CONFIG_TIMER_HAS_64BIT_CYCLE_COUNTER))
		return k_cycle_get_64();
	else
		return k_ticks_to_cyc_floor64(k_uptime_ticks());
}

#define sof_cycle_get_64_safe()		sof_cycle_get_64()
#define sof_cycle_get_64_atomic()	sof_cycle_get_64()
#define platform_timer_stop(x)

/* get timestamp for host stream DMA position */
void platform_host_timestamp(struct comp_dev *host,
			     struct sof_ipc_stream_posn *posn);

/* get timestamp for DAI stream DMA position */
void platform_dai_timestamp(struct comp_dev *dai,
			    struct sof_ipc_stream_posn *posn);

/* get current wallclock for componnent */
void platform_dai_wallclock(struct comp_dev *dai, uint64_t *wallclock);

#endif /* __ZEPHYR_RTOS_TIMER_H__ */
