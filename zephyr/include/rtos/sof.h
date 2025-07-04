/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __ZEPHYR_RTOS_SOF_H__
#define __ZEPHYR_RTOS_SOF_H__

#include <sof/common.h>
#include <sof/lib/memory.h>
#include <rtos/spinlock.h>

struct clock_info;
struct comp_driver_list;
struct dai_info;
struct dma_info;
struct dma_trace_data;
struct ipc;
struct ll_schedule_domain;
struct mm;
struct ams_shared_context;
struct notify_data;
struct pm_runtime_data;
struct sa;
struct trace;
struct pipeline_posn;
struct probe_pdata;

/**
 * \brief General firmware context.
 * This structure holds all the global pointers, which can potentially
 * be accessed by SMP code, hence it should be aligned to platform's
 * data cache line size. Alignments in the both beginning and end are needed
 * to avoid potential before and after data evictions.
 */
struct sof {
	/* init data */
	int argc;
	char **argv;

	/* ipc */
	struct ipc *ipc;

	/* system agent */
	struct sa *sa;

	/* DMA for Trace*/
	struct dma_trace_data *dmat;

	/* generic trace structure */
	struct trace *trace;

	/* platform clock information */
	struct clock_info *clocks;

	/* timer domain for driving timer LL scheduler */
	struct ll_schedule_domain *platform_timer_domain;

	/* DMA domain for driving DMA LL scheduler */
	struct ll_schedule_domain *platform_dma_domain;

	/* memory map */
	struct mm *memory_map;

	/* runtime power management data */
	struct pm_runtime_data *prd;

#ifdef CONFIG_AMS
	/* asynchronous messaging service */
	struct ams_shared_context *ams_shared_ctx;
#endif

	/* shared notifier data */
	struct notify_data *notify_data;

	/* platform dai information */
	const struct dai_info *dai_info;

	/* platform DMA information */
	const struct dma_info *dma_info;

	/* list of registered component drivers */
	struct comp_driver_list *comp_drivers;

	/* probes */
	struct probe_pdata *probe;

	/* pipelines stream position */
	struct pipeline_posn *pipeline_posn;

#ifdef CONFIG_LIBRARY_MANAGER
	/* dynamically loaded libraries */
	struct ext_library *ext_library;
#endif

#if CONFIG_IPC_MAJOR_4
	/* lock for fw_reg access */
	struct k_spinlock fw_reg_lock;
#endif

	__aligned(PLATFORM_DCACHE_ALIGN) int alignment[0];
} __aligned(PLATFORM_DCACHE_ALIGN);

struct sof *sof_get(void);

#endif /* __ZEPHYR_RTOS_SOF_H__ */
