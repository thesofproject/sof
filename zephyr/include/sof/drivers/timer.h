/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_DRIVERS_TIMER1_H__
#define __SOF_DRIVERS_TIMER1_H__

#include <stdint.h>
#include <sof/lib/cpu.h>
#include <sof/sof.h>
#include <sof/platform.h>

struct comp_dev;
struct sof_ipc_stream_posn;

#define k_cycle_get_64_safe()		k_cycle_get_64()
#define k_cycle_get_64_atomic()		k_cycle_get_64()
#define platform_timer_stop(x)

/* get timestamp for host stream DMA position */
void platform_host_timestamp(struct comp_dev *host,
			     struct sof_ipc_stream_posn *posn);

/* get timestamp for DAI stream DMA position */
void platform_dai_timestamp(struct comp_dev *dai,
			    struct sof_ipc_stream_posn *posn);

/* get current wallclock for componnent */
void platform_dai_wallclock(struct comp_dev *dai, uint64_t *wallclock);

#endif /* __SOF_DRIVERS_TIMER_H1__ */
