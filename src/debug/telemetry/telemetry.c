// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation. All rights reserved.
//
// Author: Tobiasz Dryjanski <tobiaszx.dryjanski@intel.com>

#include <zephyr/debug/sparse.h>
#include <zephyr/kernel.h>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/pipeline.h>
#include <sof/debug/telemetry/telemetry.h>
#include <sof/ipc/common.h>
#include <sof/ipc/topology.h>
#include <sof/lib/cold.h>
#include <sof/lib/cpu-clk-manager.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dai.h>
#include <sof/lib_manager.h>
#include <sof/list.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <rtos/cache.h>
#include <rtos/spinlock.h>

#include "../../audio/copier/host_copier.h"

#include <ipc/trace.h>
#include <ipc4/base_fw.h>
#include <ipc4/base_fw_vendor.h>

#include <adsp_debug_window.h>
#include <errno.h>
#include <limits.h>
#include <mem_window.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

LOG_MODULE_DECLARE(ipc, CONFIG_SOF_LOG_LEVEL);

#ifdef CONFIG_SOF_TELEMETRY_EXTENDED_WINDOW
BUILD_ASSERT(sizeof(struct sof_tel_ext_hdr) == 64, "sof_tel_ext_hdr must be 64B");
BUILD_ASSERT(sizeof(struct sof_tel_hot_line0) == 64, "sof_tel_hot_line0 must be 64B");
BUILD_ASSERT(sizeof(struct sof_tel_gw_hot) == 8, "sof_tel_gw_hot must be 8B");
BUILD_ASSERT(sizeof(struct sof_tel_mem_section) == 256, "sof_tel_mem_section must be 256B");
BUILD_ASSERT(sizeof(struct sof_tel_vregion_section) == 512, "sof_tel_vregion_section must be 512B");
BUILD_ASSERT(sizeof(struct sof_tel_ppl_section) == 384, "sof_tel_ppl_section must be 384B");
BUILD_ASSERT(sizeof(struct sof_tel_dai_section) == 256, "sof_tel_dai_section must be 256B");
BUILD_ASSERT(sizeof(struct sof_tel_thread_section) == 832, "sof_tel_thread_section must be 832B");
BUILD_ASSERT(offsetof(struct telemetry_wnd_data, ext_hdr) == 0x0100, "ext_hdr offset != 0x0100");
BUILD_ASSERT(offsetof(struct telemetry_wnd_data, hot_line0) == 0x0140, "hot_line0 offset != 0x0140");
BUILD_ASSERT(offsetof(struct telemetry_wnd_data, gw_hot) == 0x0180, "gw_hot offset != 0x0180");
BUILD_ASSERT(offsetof(struct telemetry_wnd_data, mem_sec) == 0x0240, "mem_sec offset != 0x0240");
BUILD_ASSERT(offsetof(struct telemetry_wnd_data, vreg_sec) == 0x0340, "vreg_sec offset != 0x0340");
BUILD_ASSERT(offsetof(struct telemetry_wnd_data, ppl_sec) == 0x0540, "ppl_sec offset != 0x0540");
BUILD_ASSERT(offsetof(struct telemetry_wnd_data, dai_sec) == 0x06C0, "dai_sec offset != 0x06C0");
BUILD_ASSERT(offsetof(struct telemetry_wnd_data, thd_sec) == 0x07C0, "thd_sec offset != 0x07C0");
BUILD_ASSERT(sizeof(struct telemetry_wnd_data) <= 4096, "telemetry_wnd_data exceeds 4KB slot");
#endif

/* Systick variables, one set per core */
static int telemetry_systick_counter[CONFIG_MAX_CORE_COUNT];
#ifdef CONFIG_INTEL_ADSP_DEBUG_SLOT_MANAGER
static struct telemetry_wnd_data *wnd_data;
#endif

#ifdef CONFIG_SOF_TELEMETRY_PERFORMANCE_MEASUREMENTS
static int telemetry_perf_period_sum[CONFIG_MAX_CORE_COUNT];
static int telemetry_perf_period_cnt[CONFIG_MAX_CORE_COUNT];
static struct telemetry_perf_queue telemetry_perf_queue[CONFIG_MAX_CORE_COUNT];
#endif

#ifdef CONFIG_SOF_TELEMETRY_EXTENDED_WINDOW
#define TELEMETRY_BG_REFRESH_PERIOD K_MSEC(1000)

static struct k_spinlock tel_lock;
static struct k_work_delayable telemetry_bg_dwork;
static bool telemetry_ext_ready;

struct telemetry_shared_state {
#ifdef CONFIG_SOF_TELEMETRY_HOT_CACHE_STAGING
	struct sof_tel_hot_line0 hot_line0_cache __aligned(64);
	struct sof_tel_gw_hot gw_hot_cache[SOF_TEL_MAX_GATEWAYS] __aligned(64);
	bool hot_line0_dirty;
	bool gw_line1_dirty;
	bool gw_line2_dirty;
#endif
	uint32_t total_tick_counter;
} __aligned(64);

static struct telemetry_shared_state tel_state_storage __aligned(64);

static inline struct telemetry_shared_state *tel_state(void)
{
	return (struct telemetry_shared_state *)cache_to_uncache(
		(__sparse_force void __sparse_cache *)&tel_state_storage);
}

#ifdef CONFIG_SOF_TELEMETRY_PIPELINES
struct tel_ppl_slot {
	bool in_use;
	uint16_t pipeline_id;
	uint16_t xrun_count;
	int32_t last_xrun_bytes;
};

static struct tel_ppl_slot tel_ppl_slots[SOF_TEL_MAX_PIPELINES];
#endif

#ifdef CONFIG_SOF_TELEMETRY_DAIS
struct tel_gw_slot {
	bool in_use;
	bool is_host;
	const void *key;
	struct sof_tel_gw_cold cold;
};

static struct tel_gw_slot tel_gw_slots[SOF_TEL_MAX_GATEWAYS];
#endif

struct telemetry_wnd_data *telemetry_get_wnd_ptr(void)
{
#ifdef CONFIG_INTEL_ADSP_DEBUG_SLOT_MANAGER
	return wnd_data;
#else
	return (struct telemetry_wnd_data *)ADSP_DW->slots[SOF_DW_TELEMETRY_SLOT];
#endif
}

/* Must be called with tel_lock held */
static inline struct sof_tel_hot_line0 *telemetry_get_hot_line0_locked(void)
{
#ifdef CONFIG_SOF_TELEMETRY_HOT_CACHE_STAGING
	struct telemetry_shared_state *st = tel_state();

	st->hot_line0_dirty = true;
	return &st->hot_line0_cache;
#else
	struct telemetry_wnd_data *wnd = telemetry_get_wnd_ptr();

	return wnd ? &wnd->hot_line0 : NULL;
#endif
}

/* Must be called with tel_lock held */
static inline struct sof_tel_gw_hot *telemetry_get_gw_hot_locked(int slot)
{
#ifdef CONFIG_SOF_TELEMETRY_HOT_CACHE_STAGING
	struct telemetry_shared_state *st = tel_state();

	if (slot < 8)
		st->gw_line1_dirty = true;
	else
		st->gw_line2_dirty = true;
	return &st->gw_hot_cache[slot];
#else
	struct telemetry_wnd_data *wnd = telemetry_get_wnd_ptr();

	return wnd ? &wnd->gw_hot[slot] : NULL;
#endif
}

/* Must be called with tel_lock held */
static inline void telemetry_flush_hot_staging_locked(struct telemetry_wnd_data *wnd)
{
#ifdef CONFIG_SOF_TELEMETRY_HOT_CACHE_STAGING
	struct telemetry_shared_state *st = tel_state();

	if (!wnd)
		return;

	if (st->hot_line0_dirty) {
		wnd->hot_line0 = st->hot_line0_cache;
		st->hot_line0_dirty = false;
	}
	if (st->gw_line1_dirty) {
		memcpy(&wnd->gw_hot[0], &st->gw_hot_cache[0], 64);
		st->gw_line1_dirty = false;
	}
	if (st->gw_line2_dirty) {
		memcpy(&wnd->gw_hot[8], &st->gw_hot_cache[8], 64);
		st->gw_line2_dirty = false;
	}
#else
	(void)wnd;
#endif
}

/* Must be called with tel_lock held */
static inline void telemetry_seq_begin(struct telemetry_wnd_data *wnd)
{
	wnd->ext_hdr.seq |= 1U;
	compiler_barrier();
}

/* Must be called with tel_lock held */
static inline void telemetry_seq_end(struct telemetry_wnd_data *wnd)
{
	compiler_barrier();
	wnd->ext_hdr.timestamp_us = (uint32_t)k_cyc_to_us_floor64(k_cycle_get_64());
	compiler_barrier();
	wnd->ext_hdr.seq = (wnd->ext_hdr.seq + 1U) & ~1U;
}

#ifdef CONFIG_SOF_TELEMETRY_PIPELINES
/* Must be called with tel_lock held */
static void telemetry_update_pipelines_section(struct telemetry_wnd_data *wnd)
{
	struct ipc *ipc = ipc_get();
	struct sof_tel_ppl_section *psec = &wnd->ppl_sec;
	struct sof_tel_hot_line0 *hot0 = telemetry_get_hot_line0_locked();
	struct tel_ppl_slot new_slots[SOF_TEL_MAX_PIPELINES];
	struct list_item *clist;
	k_spinlock_key_t ipc_key;
	uint16_t ppl_cnt = 0;
	uint16_t comp_cnt = 0;

	memset(new_slots, 0, sizeof(new_slots));
	memset(psec->core_sched, 0, sizeof(psec->core_sched));
	for (int c = 0; c < MIN(CONFIG_MAX_CORE_COUNT, SOF_TEL_MAX_CORES); c++) {
		psec->core_sched[c].core_id = (uint8_t)c;
#if CONFIG_KCPS_DYNAMIC_CLOCK_CONTROL
		if (cpu_is_core_enabled(c))
			psec->core_sched[c].active_kcps_budget = (uint32_t)core_kcps_get(c);
#endif
	}

	if (!ipc) {
		memset(tel_ppl_slots, 0, sizeof(tel_ppl_slots));
		if (hot0)
			memset(hot0->ppl_xrun_count, 0, sizeof(hot0->ppl_xrun_count));
		psec->active_ppl_count = 0;
		psec->total_comp_count = 0;
		wnd->ext_hdr.secs[3].active_mask = 0;
		return;
	}

	ipc_key = k_spin_lock(&ipc->lock);

	/* First pass: collect pipelines */
	list_for_item(clist, &ipc->comp_list) {
		struct ipc_comp_dev *icd = container_of(clist, struct ipc_comp_dev, list);

		if (icd->type == COMP_TYPE_COMPONENT) {
			comp_cnt++;
			if (icd->cd && icd->core < SOF_TEL_MAX_CORES) {
				if (icd->cd->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP)
					psec->core_sched[icd->core].dp_task_count++;
			}
			continue;
		}

		if (icd->type != COMP_TYPE_PIPELINE || !icd->pipeline)
			continue;

		if (ppl_cnt >= SOF_TEL_MAX_PIPELINES)
			continue;

		struct pipeline *p = icd->pipeline;
		struct sof_tel_ppl_info *pinfo = &psec->pipelines[ppl_cnt];
		uint16_t pid = (uint16_t)p->pipeline_id;
		uint16_t prev_xruns = 0;
		int32_t prev_xrun_bytes = 0;

		for (int s = 0; s < SOF_TEL_MAX_PIPELINES; s++) {
			if (tel_ppl_slots[s].in_use && tel_ppl_slots[s].pipeline_id == pid) {
				prev_xruns = tel_ppl_slots[s].xrun_count;
				prev_xrun_bytes = tel_ppl_slots[s].last_xrun_bytes;
				break;
			}
		}

		new_slots[ppl_cnt].in_use = true;
		new_slots[ppl_cnt].pipeline_id = pid;
		new_slots[ppl_cnt].xrun_count = prev_xruns;
		new_slots[ppl_cnt].last_xrun_bytes = p->xrun_bytes ? p->xrun_bytes :
						     prev_xrun_bytes;

		pinfo->pipeline_id = pid;
		pinfo->core = (uint8_t)p->core;
		pinfo->priority = (uint8_t)p->priority;
		pinfo->status = (uint8_t)p->status;
		pinfo->comp_count = 0;
		pinfo->attributes = (uint16_t)p->attributes;
		pinfo->period_us = p->period;
		pinfo->sched_comp_id = p->sched_id;
		pinfo->last_xrun_bytes = new_slots[ppl_cnt].last_xrun_bytes;

		if (p->core < SOF_TEL_MAX_CORES) {
			psec->core_sched[p->core].active_ppl_count++;
			if (p->status == COMP_STATE_ACTIVE)
				psec->core_sched[p->core].ll_task_count++;
		}
		ppl_cnt++;
	}

	/* Clear unused pipeline slots */
	for (uint16_t i = ppl_cnt; i < SOF_TEL_MAX_PIPELINES; i++)
		memset(&psec->pipelines[i], 0, sizeof(psec->pipelines[i]));

	/* Second pass: count components per pipeline */
	list_for_item(clist, &ipc->comp_list) {
		struct ipc_comp_dev *icd = container_of(clist, struct ipc_comp_dev, list);

		if (icd->type != COMP_TYPE_COMPONENT || !icd->cd)
			continue;

		uint32_t pid = dev_comp_pipe_id(icd->cd);

		for (uint16_t i = 0; i < ppl_cnt; i++) {
			if (psec->pipelines[i].pipeline_id == (uint16_t)pid) {
				psec->pipelines[i].comp_count++;
				break;
			}
		}
	}

	k_spin_unlock(&ipc->lock, ipc_key);

	memcpy(tel_ppl_slots, new_slots, sizeof(tel_ppl_slots));
	if (hot0) {
		for (uint16_t i = 0; i < SOF_TEL_MAX_PIPELINES; i++)
			hot0->ppl_xrun_count[i] = tel_ppl_slots[i].in_use ?
						  tel_ppl_slots[i].xrun_count : 0;
	}

	for (int c = 0; c < MIN(CONFIG_MAX_CORE_COUNT, SOF_TEL_MAX_CORES); c++) {
		if (hot0)
			hot0->active_ll_tasks[c] = psec->core_sched[c].ll_task_count;
		if (psec->core_sched[c].ll_task_count == 0) {
			wnd->system_tick_info[c].last_time_elapsed = 0;
			wnd->system_tick_info[c].avg_utilization = 0;
#ifdef CONFIG_SOF_TELEMETRY_PERFORMANCE_MEASUREMENTS
			telemetry_perf_period_sum[c] = 0;
			telemetry_perf_period_cnt[c] = 0;
			memset(&telemetry_perf_queue[c], 0, sizeof(telemetry_perf_queue[c]));
#endif
		}
	}

	psec->active_ppl_count = ppl_cnt;
	psec->total_comp_count = comp_cnt;
	wnd->ext_hdr.secs[3].active_mask = ppl_cnt;
}

void telemetry_notify_pipeline_change(void)
{
	struct telemetry_wnd_data *wnd = telemetry_get_wnd_ptr();
	k_spinlock_key_t key;

	if (!telemetry_ext_ready || !wnd)
		return;

	key = k_spin_lock(&tel_lock);
	telemetry_seq_begin(wnd);
	telemetry_update_pipelines_section(wnd);
	telemetry_flush_hot_staging_locked(wnd);
	telemetry_seq_end(wnd);
	k_spin_unlock(&tel_lock, key);

	k_work_reschedule(&telemetry_bg_dwork, K_NO_WAIT);
}

void telemetry_notify_pipeline_xrun(struct pipeline *p, int32_t bytes)
{
	struct telemetry_wnd_data *wnd = telemetry_get_wnd_ptr();
	k_spinlock_key_t key;
	uint16_t pid;
	int slot = -1;
	int free_slot = -1;

	if (!telemetry_ext_ready || !wnd || !p)
		return;

	pid = (uint16_t)p->pipeline_id;
	key = k_spin_lock(&tel_lock);
	struct sof_tel_hot_line0 *hot0 = telemetry_get_hot_line0_locked();

	if (hot0)
		hot0->total_xrun_events++;

	for (int i = 0; i < SOF_TEL_MAX_PIPELINES; i++) {
		if (tel_ppl_slots[i].in_use && tel_ppl_slots[i].pipeline_id == pid) {
			slot = i;
			break;
		}
		if (!tel_ppl_slots[i].in_use && free_slot < 0)
			free_slot = i;
	}

	if (slot < 0 && free_slot >= 0) {
		slot = free_slot;
		tel_ppl_slots[slot].in_use = true;
		tel_ppl_slots[slot].pipeline_id = pid;
		tel_ppl_slots[slot].xrun_count = 0;
	}

	if (slot >= 0) {
		tel_ppl_slots[slot].xrun_count++;
		tel_ppl_slots[slot].last_xrun_bytes = bytes;
		if (hot0)
			hot0->ppl_xrun_count[slot] = tel_ppl_slots[slot].xrun_count;
		telemetry_seq_begin(wnd);
		wnd->ppl_sec.pipelines[slot].pipeline_id = pid;
		wnd->ppl_sec.pipelines[slot].last_xrun_bytes = bytes;
		telemetry_seq_end(wnd);
	}
	k_spin_unlock(&tel_lock, key);
}

void telemetry_notify_dp_deadline_miss(int core)
{
	k_spinlock_key_t key;

	if (!telemetry_ext_ready || core < 0 || core >= SOF_TEL_MAX_CORES)
		return;

	key = k_spin_lock(&tel_lock);
	struct sof_tel_hot_line0 *hot0 = telemetry_get_hot_line0_locked();

	if (hot0)
		hot0->dp_deadline_misses[core]++;
	k_spin_unlock(&tel_lock, key);
}
#endif /* CONFIG_SOF_TELEMETRY_PIPELINES */

#ifdef CONFIG_SOF_TELEMETRY_DAIS
static uint8_t map_sof_dai_type(uint32_t type)
{
	switch (type) {
	case SOF_DAI_INTEL_SSP:
		return SOF_TEL_GW_SSP;
	case SOF_DAI_INTEL_DMIC:
		return SOF_TEL_GW_DMIC;
	case SOF_DAI_INTEL_ALH:
		return SOF_TEL_GW_ALH;
	case SOF_DAI_INTEL_HDA:
		return SOF_TEL_GW_HDA;
	default:
		return SOF_TEL_GW_OTHER;
	}
}

static void telemetry_fill_dai_cold(struct sof_tel_gw_cold *cold,
				    struct dai_data *dd, struct comp_dev *dev)
{
	memset(cold, 0, sizeof(*cold));
	cold->gw_type = map_sof_dai_type(dd->ipc_config.type);
	cold->index = (uint8_t)dd->ipc_config.dai_index;
	cold->direction = dev ? (uint8_t)dev->direction : (uint8_t)dd->ipc_config.direction;
	cold->dma_chan = (int8_t)dd->chan_index;
	cold->comp_id = dev ? (uint32_t)dev_comp_id(dev) : 0;
	cold->state = dev ? (uint8_t)dev->state : 0;
	cold->group_id = dd->group ? (uint8_t)dd->group->group_id : 0;
	cold->period_bytes = (uint16_t)MIN(dd->period_bytes, UINT16_MAX);
	cold->dma_buf_size = dd->dma_buffer ?
			     (uint32_t)audio_stream_get_size(&dd->dma_buffer->stream) :
			     dd->ipc_config.dma_buffer_size;
}

static void telemetry_fill_host_cold(struct sof_tel_gw_cold *cold,
				     struct host_data *hd, struct comp_dev *dev)
{
	memset(cold, 0, sizeof(*cold));
	cold->gw_type = SOF_TEL_GW_HOST_DMA;
	cold->index = (uint8_t)MAX(hd->chan_index, 0);
	cold->direction = dev ? (uint8_t)dev->direction : 0;
	cold->dma_chan = (int8_t)hd->chan_index;
	cold->comp_id = dev ? (uint32_t)dev_comp_id(dev) : 0;
	cold->state = dev ? (uint8_t)dev->state : 0;
	cold->group_id = 0;
	cold->period_bytes = (uint16_t)MIN(hd->period_bytes, UINT16_MAX);
	cold->dma_buf_size = hd->dma_buffer ?
			     (uint32_t)audio_stream_get_size(&hd->dma_buffer->stream) :
			     hd->host_size;
}

/* Must be called with tel_lock held */
static void telemetry_sync_gw_cold_slot_locked(struct telemetry_wnd_data *wnd, int slot)
{
	uint16_t mask = 0;

	if (!telemetry_ext_ready || !wnd || slot < 0 || slot >= SOF_TEL_MAX_GATEWAYS)
		return;

	struct sof_tel_gw_cold *cold = &wnd->dai_sec.gateways[slot];
	struct tel_gw_slot *s = &tel_gw_slots[slot];

	if (s->in_use)
		*cold = s->cold;
	else
		memset(cold, 0, sizeof(*cold));

	for (int i = 0; i < SOF_TEL_MAX_GATEWAYS; i++) {
		if (tel_gw_slots[i].in_use)
			mask |= BIT(i);
	}
	wnd->ext_hdr.secs[4].active_mask = mask;
}

int telemetry_register_dai(struct dai_data *dd, struct comp_dev *dev)
{
	struct telemetry_wnd_data *wnd = telemetry_get_wnd_ptr();
	struct sof_tel_gw_cold cold;
	k_spinlock_key_t key;
	int free_slot = -1;
	int slot = -1;

	if (!dd)
		return -EINVAL;

	telemetry_fill_dai_cold(&cold, dd, dev);
	key = k_spin_lock(&tel_lock);

	for (int i = 0; i < SOF_TEL_MAX_GATEWAYS; i++) {
		if (tel_gw_slots[i].in_use && !tel_gw_slots[i].is_host &&
		    tel_gw_slots[i].key == dd) {
			slot = i;
			break;
		}
		if (!tel_gw_slots[i].in_use && free_slot < 0)
			free_slot = i;
	}

	if (slot < 0) {
		if (free_slot < 0) {
			k_spin_unlock(&tel_lock, key);
			return -ENOMEM;
		}
		slot = free_slot;
		tel_gw_slots[slot].in_use = true;
		tel_gw_slots[slot].is_host = false;
		tel_gw_slots[slot].key = dd;
	}

	tel_gw_slots[slot].cold = cold;
	if (telemetry_ext_ready && wnd) {
		telemetry_seq_begin(wnd);
		telemetry_sync_gw_cold_slot_locked(wnd, slot);
		telemetry_seq_end(wnd);
	}
	k_spin_unlock(&tel_lock, key);
	return slot;
}

void telemetry_unregister_dai(struct dai_data *dd)
{
	struct telemetry_wnd_data *wnd = telemetry_get_wnd_ptr();
	k_spinlock_key_t key = k_spin_lock(&tel_lock);

	for (int i = 0; i < SOF_TEL_MAX_GATEWAYS; i++) {
		if (tel_gw_slots[i].in_use && !tel_gw_slots[i].is_host &&
		    tel_gw_slots[i].key == dd) {
			struct sof_tel_gw_hot *hot = telemetry_get_gw_hot_locked(i);

			tel_gw_slots[i].in_use = false;
			tel_gw_slots[i].key = NULL;
			memset(&tel_gw_slots[i].cold, 0, sizeof(tel_gw_slots[i].cold));
			if (hot)
				memset(hot, 0, sizeof(*hot));
			if (telemetry_ext_ready && wnd) {
				telemetry_seq_begin(wnd);
				telemetry_sync_gw_cold_slot_locked(wnd, i);
				telemetry_flush_hot_staging_locked(wnd);
				telemetry_seq_end(wnd);
			}
			break;
		}
	}
	k_spin_unlock(&tel_lock, key);
}

void telemetry_update_dai_hot(struct dai_data *dd, struct comp_dev *dev)
{
	struct sof_tel_gw_cold cold;
	uint32_t avail = 0;
	bool has_avail = false;
	k_spinlock_key_t key;

	if (!dd)
		return;

	telemetry_fill_dai_cold(&cold, dd, dev);
	if (dd->dma_buffer) {
		avail = audio_stream_get_avail_bytes(&dd->dma_buffer->stream);
		has_avail = true;
	}

	key = k_spin_lock(&tel_lock);
	for (int i = 0; i < SOF_TEL_MAX_GATEWAYS; i++) {
		if (tel_gw_slots[i].in_use && !tel_gw_slots[i].is_host &&
		    tel_gw_slots[i].key == dd) {
			struct sof_tel_gw_hot *hot = telemetry_get_gw_hot_locked(i);

			tel_gw_slots[i].cold = cold;
			if (!hot)
				break;

			hot->total_kb_or_frames = (uint32_t)(dd->total_data_processed >> 10);
			if (has_avail)
				hot->dma_buf_avail_bytes = (uint16_t)MIN(avail, UINT16_MAX);
			if (dd->xrun)
				hot->xrun_or_skip_count++;
			break;
		}
	}
	k_spin_unlock(&tel_lock, key);
}

int telemetry_register_host(struct host_data *hd, struct comp_dev *dev)
{
	struct telemetry_wnd_data *wnd = telemetry_get_wnd_ptr();
	struct sof_tel_gw_cold cold;
	k_spinlock_key_t key;
	int free_slot = -1;
	int slot = -1;

	if (!hd)
		return -EINVAL;

	telemetry_fill_host_cold(&cold, hd, dev);
	key = k_spin_lock(&tel_lock);

	for (int i = 0; i < SOF_TEL_MAX_GATEWAYS; i++) {
		if (tel_gw_slots[i].in_use && tel_gw_slots[i].is_host &&
		    tel_gw_slots[i].key == hd) {
			slot = i;
			break;
		}
		if (!tel_gw_slots[i].in_use && free_slot < 0)
			free_slot = i;
	}

	if (slot < 0) {
		if (free_slot < 0) {
			k_spin_unlock(&tel_lock, key);
			return -ENOMEM;
		}
		slot = free_slot;
		tel_gw_slots[slot].in_use = true;
		tel_gw_slots[slot].is_host = true;
		tel_gw_slots[slot].key = hd;
	}

	tel_gw_slots[slot].cold = cold;
	if (telemetry_ext_ready && wnd) {
		telemetry_seq_begin(wnd);
		telemetry_sync_gw_cold_slot_locked(wnd, slot);
		telemetry_seq_end(wnd);
	}
	k_spin_unlock(&tel_lock, key);
	return slot;
}

void telemetry_unregister_host(struct host_data *hd)
{
	struct telemetry_wnd_data *wnd = telemetry_get_wnd_ptr();
	k_spinlock_key_t key = k_spin_lock(&tel_lock);

	for (int i = 0; i < SOF_TEL_MAX_GATEWAYS; i++) {
		if (tel_gw_slots[i].in_use && tel_gw_slots[i].is_host &&
		    tel_gw_slots[i].key == hd) {
			struct sof_tel_gw_hot *hot = telemetry_get_gw_hot_locked(i);

			tel_gw_slots[i].in_use = false;
			tel_gw_slots[i].key = NULL;
			memset(&tel_gw_slots[i].cold, 0, sizeof(tel_gw_slots[i].cold));
			if (hot)
				memset(hot, 0, sizeof(*hot));
			if (telemetry_ext_ready && wnd) {
				telemetry_seq_begin(wnd);
				telemetry_sync_gw_cold_slot_locked(wnd, i);
				telemetry_flush_hot_staging_locked(wnd);
				telemetry_seq_end(wnd);
			}
			break;
		}
	}
	k_spin_unlock(&tel_lock, key);
}

void telemetry_update_host_hot(struct host_data *hd, struct comp_dev *dev)
{
	struct sof_tel_gw_cold cold;
	uint32_t avail = 0;
	bool has_avail = false;
	k_spinlock_key_t key;

	if (!hd)
		return;

	telemetry_fill_host_cold(&cold, hd, dev);
	if (hd->local_buffer) {
		avail = audio_stream_get_avail_bytes(&hd->local_buffer->stream);
		has_avail = true;
	}

	key = k_spin_lock(&tel_lock);
	for (int i = 0; i < SOF_TEL_MAX_GATEWAYS; i++) {
		if (tel_gw_slots[i].in_use && tel_gw_slots[i].is_host &&
		    tel_gw_slots[i].key == hd) {
			struct sof_tel_gw_hot *hot = telemetry_get_gw_hot_locked(i);

			tel_gw_slots[i].cold = cold;
			if (!hot)
				break;

			hot->total_kb_or_frames = (uint32_t)(hd->total_data_processed >> 10);
			if (has_avail)
				hot->dma_buf_avail_bytes = (uint16_t)MIN(avail, UINT16_MAX);
			if (hd->n_skipped)
				hot->xrun_or_skip_count++;
			break;
		}
	}
	k_spin_unlock(&tel_lock, key);
}
#endif /* CONFIG_SOF_TELEMETRY_DAIS */

#ifdef CONFIG_SOF_TELEMETRY_VREGION
void telemetry_notify_vregion_change(void)
{
	struct telemetry_wnd_data *wnd = telemetry_get_wnd_ptr();
	k_spinlock_key_t key;

	if (!telemetry_ext_ready || !wnd)
		return;

	key = k_spin_lock(&tel_lock);
	telemetry_seq_begin(wnd);
	telemetry_collect_vpage_stats(&wnd->vreg_sec);
	telemetry_collect_vregion_stats(&wnd->vreg_sec);
	wnd->ext_hdr.secs[2].active_mask = wnd->vreg_sec.active_vregion_count;
	telemetry_seq_end(wnd);
	k_spin_unlock(&tel_lock, key);
}
#endif

#ifdef CONFIG_SOF_TELEMETRY_THREADS
extern int z_impl_k_thread_stack_space_get(const struct k_thread *thread, size_t *unused_ptr);

#ifdef CONFIG_THREAD_RUNTIME_STATS
static uint64_t tel_prev_core_active[SOF_TEL_MAX_CORES];
static uint64_t tel_prev_core_all[SOF_TEL_MAX_CORES];
static uint32_t tel_last_core_delta_all[SOF_TEL_MAX_CORES];
static struct {
	const struct k_thread *tid;
	uint64_t cycles;
	uint32_t seen_seq;
} tel_prev_thread_cycles[SOF_TEL_MAX_THREADS];
#endif

static void telemetry_thread_cb(const struct k_thread *cthread, void *user_data)
{
	struct sof_tel_thread_section *tsec = user_data;
	struct k_thread *thread = (struct k_thread *)cthread;
	uint16_t idx = tsec->thread_count;

	if (idx >= SOF_TEL_MAX_THREADS) {
		tsec->dropped_threads++;
		return;
	}

	struct sof_tel_thread_info *ti = &tsec->threads[idx];
	const char *name = k_thread_name_get((k_tid_t)thread);

	memset(ti, 0, sizeof(*ti));
	ti->thread_id = (uint32_t)POINTER_TO_UINT(thread);
#ifdef CONFIG_SMP
	ti->core = thread->base.cpu;
	if (thread->base.is_idle)
		ti->flags |= SOF_TEL_THD_FLG_IDLE;
#endif
	ti->prio = thread->base.prio;
	ti->state = thread->base.thread_state;
#ifdef CONFIG_USERSPACE
	if (thread->base.user_options & K_USER)
		ti->flags |= SOF_TEL_THD_FLG_USER;
#endif
	if (name) {
		if (strncmp(name, "ll_thread", 9) == 0)
			ti->flags |= SOF_TEL_THD_FLG_LL;
		else if (strncmp(name, "DP_", 3) == 0 || strncmp(name, "edf_work", 8) == 0)
			ti->flags |= SOF_TEL_THD_FLG_DP;
		else if (strncmp(name, "idle", 4) == 0)
			ti->flags |= SOF_TEL_THD_FLG_IDLE;
	}

#ifdef CONFIG_THREAD_STACK_INFO
	if (thread->stack_info.size > 0) {
		size_t unused = 0;

		ti->stack_size_chunks = (uint16_t)(thread->stack_info.size >> 6);
		if (z_impl_k_thread_stack_space_get(thread, &unused) == 0 &&
		    unused <= thread->stack_info.size) {
			ti->stack_usage = (uint8_t)((255ULL * (thread->stack_info.size - unused)) /
						    thread->stack_info.size);
		}
	}
#endif

#ifdef CONFIG_THREAD_RUNTIME_STATS
	k_thread_runtime_stats_t rt_stats;

	if (k_thread_runtime_stats_get(thread, &rt_stats) == 0) {
		uint32_t cur_seq = tsec->sample_seq + 1;
		uint32_t delta_cyc = 0;
		int empty_slot = -1;
		int stale_slot = -1;

		ti->total_exec_kcycles = (uint32_t)(rt_stats.execution_cycles >> 10);
		for (int i = 0; i < SOF_TEL_MAX_THREADS; i++) {
			if (tel_prev_thread_cycles[i].tid == cthread) {
				delta_cyc = (uint32_t)(rt_stats.execution_cycles -
						       tel_prev_thread_cycles[i].cycles);
				tel_prev_thread_cycles[i].cycles = rt_stats.execution_cycles;
				tel_prev_thread_cycles[i].seen_seq = cur_seq;
				empty_slot = -2;
				break;
			}
			if (empty_slot == -1 && !tel_prev_thread_cycles[i].tid)
				empty_slot = i;
			else if (stale_slot == -1 &&
				 tel_prev_thread_cycles[i].seen_seq != cur_seq)
				stale_slot = i;
		}
		if (empty_slot == -1)
			empty_slot = stale_slot;
		if (empty_slot >= 0) {
			tel_prev_thread_cycles[empty_slot].tid = cthread;
			tel_prev_thread_cycles[empty_slot].cycles = rt_stats.execution_cycles;
			tel_prev_thread_cycles[empty_slot].seen_seq = cur_seq;
		}
		uint8_t c = ti->core < SOF_TEL_MAX_CORES ? ti->core : 0;

		if (tel_last_core_delta_all[c] > 0 && delta_cyc <= tel_last_core_delta_all[c]) {
			ti->cpu_load = (uint8_t)((255ULL * delta_cyc) /
						 tel_last_core_delta_all[c]);
		}
	}
#endif

	if (name && name[0] != '\0')
		strncpy(ti->name, name, sizeof(ti->name));

	tsec->thread_count++;
}

static void telemetry_update_threads_section(struct telemetry_wnd_data *wnd)
{
	struct sof_tel_thread_section *tsec = &wnd->thd_sec;

#ifdef CONFIG_THREAD_RUNTIME_STATS
	for (int c = 0; c < MIN(CONFIG_MAX_CORE_COUNT, SOF_TEL_MAX_CORES); c++) {
		k_thread_runtime_stats_t core_stats;

		if (!cpu_is_core_enabled(c)) {
			tsec->core_load[c] = 0;
			tel_last_core_delta_all[c] = 0;
			continue;
		}
		if (k_thread_runtime_stats_cpu_get(c, &core_stats) == 0) {
			uint32_t act = (uint32_t)(core_stats.total_cycles - tel_prev_core_active[c]);
			uint32_t all = (uint32_t)(core_stats.execution_cycles - tel_prev_core_all[c]);

			tel_prev_core_active[c] = core_stats.total_cycles;
			tel_prev_core_all[c] = core_stats.execution_cycles;
			tel_last_core_delta_all[c] = all;
			tsec->core_load[c] = all ? (uint8_t)MIN(255ULL, (255ULL * act) / all) : 0;
		}
	}
#endif

	tsec->thread_count = 0;
	tsec->dropped_threads = 0;
	k_thread_foreach_unlocked(telemetry_thread_cb, tsec);
	for (uint16_t i = tsec->thread_count; i < SOF_TEL_MAX_THREADS; i++)
		memset(&tsec->threads[i], 0, sizeof(tsec->threads[i]));
	tsec->sample_seq++;
#ifdef CONFIG_THREAD_RUNTIME_STATS
	for (int i = 0; i < SOF_TEL_MAX_THREADS; i++) {
		if (tel_prev_thread_cycles[i].tid &&
		    tel_prev_thread_cycles[i].seen_seq != tsec->sample_seq) {
			tel_prev_thread_cycles[i].tid = NULL;
			tel_prev_thread_cycles[i].cycles = 0;
			tel_prev_thread_cycles[i].seen_seq = 0;
		}
	}
#endif
	wnd->ext_hdr.secs[5].active_mask = tsec->thread_count;
}
#endif /* CONFIG_SOF_TELEMETRY_THREADS */

void telemetry_refresh_cold_sections(void)
{
	struct telemetry_wnd_data *wnd = telemetry_get_wnd_ptr();
	k_spinlock_key_t key;

	if (!telemetry_ext_ready || !wnd)
		return;

	key = k_spin_lock(&tel_lock);
	telemetry_seq_begin(wnd);

#ifdef CONFIG_SOF_TELEMETRY_MEMORY
	telemetry_collect_mem_stats(&wnd->mem_sec);
	wnd->ext_hdr.secs[1].active_mask = SOF_TEL_MAX_HEAPS;
#endif

#ifdef CONFIG_SOF_TELEMETRY_VREGION
	telemetry_collect_vpage_stats(&wnd->vreg_sec);
	telemetry_collect_vregion_stats(&wnd->vreg_sec);
	wnd->ext_hdr.secs[2].active_mask = wnd->vreg_sec.active_vregion_count;
#endif

#ifdef CONFIG_SOF_TELEMETRY_PIPELINES
	telemetry_update_pipelines_section(wnd);
#endif

#ifdef CONFIG_SOF_TELEMETRY_DAIS
	for (int i = 0; i < SOF_TEL_MAX_GATEWAYS; i++)
		telemetry_sync_gw_cold_slot_locked(wnd, i);
#endif

#ifdef CONFIG_SOF_TELEMETRY_THREADS
	telemetry_update_threads_section(wnd);
#endif

	telemetry_flush_hot_staging_locked(wnd);
	telemetry_seq_end(wnd);
	k_spin_unlock(&tel_lock, key);
}

static void telemetry_bg_work_handler(struct k_work *work)
{
	(void)work;
	telemetry_refresh_cold_sections();
	if (telemetry_ext_ready)
		k_work_reschedule(&telemetry_bg_dwork, TELEMETRY_BG_REFRESH_PERIOD);
}

static void telemetry_ext_init(struct telemetry_wnd_data *wnd)
{
	struct sof_tel_ext_hdr *hdr = &wnd->ext_hdr;

	memset((uint8_t *)wnd + offsetof(struct telemetry_wnd_data, pad_to_0x100), 0,
	       sizeof(*wnd) - offsetof(struct telemetry_wnd_data, pad_to_0x100));

	hdr->magic = SOF_TEL_EXT_MAGIC;
	hdr->version = SOF_TEL_EXT_VERSION;
	hdr->max_cores = CONFIG_MAX_CORE_COUNT;
	hdr->num_secs = SOF_TEL_MAX_SECTIONS;
	hdr->seq = 0;

	/* Section 0: Hot Counter Zone (0x0140, 256 B) */
	hdr->secs[0].id = SOF_TEL_SEC_HOT_ZONE;
	hdr->secs[0].elem_size = 64;
	hdr->secs[0].offset = (uint16_t)offsetof(struct telemetry_wnd_data, hot_line0);
	hdr->secs[0].max_count = 4;
	hdr->secs[0].active_mask = 3;

	/* Section 1: Memory & Heaps (0x0240, 256 B = 4 cache lines) */
	hdr->secs[1].id = SOF_TEL_SEC_MEMORY;
	hdr->secs[1].elem_size = 64;
	hdr->secs[1].offset = (uint16_t)offsetof(struct telemetry_wnd_data, mem_sec);
	hdr->secs[1].max_count = IS_ENABLED(CONFIG_SOF_TELEMETRY_MEMORY) ? 4 : 0;

	/* Section 2: vpage & vregion (0x0340, 512 B) */
	hdr->secs[2].id = SOF_TEL_SEC_VREGION;
	hdr->secs[2].elem_size = sizeof(struct sof_tel_vregion_info);
	hdr->secs[2].offset = (uint16_t)offsetof(struct telemetry_wnd_data, vreg_sec);
	hdr->secs[2].max_count = IS_ENABLED(CONFIG_SOF_TELEMETRY_VREGION) ?
				 SOF_TEL_MAX_VREGIONS : 0;

	/* Section 3: Pipelines & Schedulers (0x0540, 384 B) */
	hdr->secs[3].id = SOF_TEL_SEC_PIPELINES;
	hdr->secs[3].elem_size = sizeof(struct sof_tel_ppl_info);
	hdr->secs[3].offset = (uint16_t)offsetof(struct telemetry_wnd_data, ppl_sec);
	hdr->secs[3].max_count = IS_ENABLED(CONFIG_SOF_TELEMETRY_PIPELINES) ?
				 SOF_TEL_MAX_PIPELINES : 0;

	/* Section 4: DAIs & Host Gateways (0x06C0, 256 B) */
	hdr->secs[4].id = SOF_TEL_SEC_DAIS;
	hdr->secs[4].elem_size = sizeof(struct sof_tel_gw_cold);
	hdr->secs[4].offset = (uint16_t)offsetof(struct telemetry_wnd_data, dai_sec);
	hdr->secs[4].max_count = IS_ENABLED(CONFIG_SOF_TELEMETRY_DAIS) ?
				 SOF_TEL_MAX_GATEWAYS : 0;

	/* Section 5: Zephyr Threads (0x07C0, 832 B) */
	hdr->secs[5].id = SOF_TEL_SEC_THREADS;
	hdr->secs[5].elem_size = sizeof(struct sof_tel_thread_info);
	hdr->secs[5].offset = (uint16_t)offsetof(struct telemetry_wnd_data, thd_sec);
	hdr->secs[5].max_count = IS_ENABLED(CONFIG_SOF_TELEMETRY_THREADS) ?
				 SOF_TEL_MAX_THREADS : 0;

	k_work_init_delayable(&telemetry_bg_dwork, telemetry_bg_work_handler);
}
#endif /* CONFIG_SOF_TELEMETRY_EXTENDED_WINDOW */

#ifdef CONFIG_SOF_TELEMETRY_PERFORMANCE_MEASUREMENTS
static void telemetry_perf_queue_append(struct telemetry_perf_queue *q, size_t element)
{
	if (!q->full) {
		q->elements[q->index] = element;
		q->sum += element;
		q->index++;
		q->size++;
		if (q->index >= SOF_AVG_PERF_MEAS_DEPTH) {
			q->index = 0;
			q->size = SOF_AVG_PERF_MEAS_DEPTH;
			q->full = true;
		}
	} else {
		/* no space, pop tail */
		q->sum -= q->elements[q->index];
		/* replace tail */
		q->elements[q->index] = element;
		q->sum += element;
		/* move tail */
		q->index++;
		if (q->index >= SOF_AVG_PERF_MEAS_DEPTH)
			q->index = 0;
	}
}

static size_t telemetry_perf_queue_avg(struct telemetry_perf_queue *q)
{
	if (!q->size)
		return 0;
	return q->sum / q->size;
}
#endif

#ifdef CONFIG_INTEL_ADSP_DEBUG_SLOT_MANAGER
struct system_tick_info *telemetry_get_systick_info_ptr(void)
{
	if (!wnd_data)
		return NULL;

	return (struct system_tick_info *)wnd_data->system_tick_info;
}
#endif

__cold int telemetry_init(void)
{
	/* systick_init */
#ifdef CONFIG_INTEL_ADSP_DEBUG_SLOT_MANAGER
	struct adsp_dw_desc slot_desc = { .type = ADSP_DW_SLOT_TELEMETRY, };
	struct system_tick_info *systick_info;

	assert_can_be_cold();

	if (wnd_data)
		return 0;

	wnd_data = (struct telemetry_wnd_data *)adsp_dw_request_slot(&slot_desc,
								     NULL);
	if (!wnd_data)
		return -ENOMEM;

	systick_info = (struct system_tick_info *)wnd_data->system_tick_info;
#else
	uint8_t slot_num = SOF_DW_TELEMETRY_SLOT;
	volatile struct adsp_debug_window *window = ADSP_DW;
	struct telemetry_wnd_data *wnd_data = (struct telemetry_wnd_data *)ADSP_DW->slots[slot_num];
	struct system_tick_info *systick_info =
			(struct system_tick_info *)wnd_data->system_tick_info;

	assert_can_be_cold();

	window->descs[slot_num].type = ADSP_DW_SLOT_TELEMETRY;
	window->descs[slot_num].resource_id = 0;
#endif

	tr_info(&ipc_tr, "Telemetry enabled. May affect performance");

	wnd_data->separator_1 = 0x0000C0DE;

	/* Zero values per core */
	for (int i = 0; i < CONFIG_MAX_CORE_COUNT; i++) {
		systick_info[i].count = 0;
		systick_info[i].last_time_elapsed = 0;
		systick_info[i].max_time_elapsed = 0;
		systick_info[i].last_ccount = 0;
		systick_info[i].avg_utilization = 0;
		systick_info[i].peak_utilization = 0;
		systick_info[i].peak_utilization_4k = 0;
		systick_info[i].peak_utilization_8k = 0;
	}

#ifdef CONFIG_SOF_TELEMETRY_EXTENDED_WINDOW
	telemetry_ext_init(wnd_data);
#endif

	return 0;
}

void telemetry_update(uint32_t begin_stamp, uint32_t current_stamp)
{
	int prid = cpu_get_id();
	uint32_t elapsed = current_stamp - begin_stamp;
#ifdef CONFIG_INTEL_ADSP_DEBUG_SLOT_MANAGER
	struct system_tick_info *systick_info = telemetry_get_systick_info_ptr();

	if (!systick_info)
		return;
#else
	struct telemetry_wnd_data *wnd_data =
		(struct telemetry_wnd_data *)ADSP_DW->slots[SOF_DW_TELEMETRY_SLOT];
	struct system_tick_info *systick_info =
		(struct system_tick_info *)wnd_data->system_tick_info;
#endif

	++telemetry_systick_counter[prid];

	systick_info[prid].count = telemetry_systick_counter[prid];
	systick_info[prid].last_time_elapsed = elapsed;
	systick_info[prid].max_time_elapsed =
			MAX(elapsed, systick_info[prid].max_time_elapsed);
	systick_info[prid].last_ccount = current_stamp;

#ifdef CONFIG_SOF_TELEMETRY_PERFORMANCE_MEASUREMENTS
	const size_t measured_systick = elapsed;

	if (telemetry_systick_counter[prid] > 2) {
		telemetry_perf_period_sum[prid] += measured_systick;
		telemetry_perf_period_cnt[prid] =
			(telemetry_perf_period_cnt[prid] + 1) % SOF_AVG_PERF_MEAS_PERIOD;
		if (telemetry_perf_period_cnt[prid] == 0) {
			/* Append average of last SOF_AVG_PERF_MEAS_PERIOD runs */
			telemetry_perf_queue_append(&telemetry_perf_queue[prid],
						    telemetry_perf_period_sum[prid]
						    / SOF_AVG_PERF_MEAS_PERIOD);
			telemetry_perf_period_sum[prid] = 0;
			/* Calculate average from all buckets */
			systick_info[prid].avg_utilization =
				telemetry_perf_queue_avg(&telemetry_perf_queue[prid]);
		}

		systick_info[prid].peak_utilization =
			MAX(systick_info[prid].peak_utilization,
			    measured_systick);
		systick_info[prid].peak_utilization_4k =
			MAX(systick_info[prid].peak_utilization_4k,
			    measured_systick);
		systick_info[prid].peak_utilization_8k =
			MAX(systick_info[prid].peak_utilization_8k,
			    measured_systick);

		/* optimized: counter % 0x1000 */
		if ((telemetry_systick_counter[prid] & 0xfff) == 0)
			systick_info[prid].peak_utilization_4k = 0;
		/* optimized: counter % 0x2000 */
		if ((telemetry_systick_counter[prid] & 0x1fff) == 0)
			systick_info[prid].peak_utilization_8k = 0;
	}
#endif

#ifdef CONFIG_SOF_TELEMETRY_EXTENDED_WINDOW
	if (telemetry_ext_ready) {
		uint32_t ll_budget_cycles = (uint32_t)k_us_to_cyc_ceil32(LL_TIMER_PERIOD_US);
		uint32_t ticks;
		k_spinlock_key_t key = k_spin_lock(&tel_lock);

		if (elapsed > ll_budget_cycles && prid < SOF_TEL_MAX_CORES) {
			struct sof_tel_hot_line0 *hot0 = telemetry_get_hot_line0_locked();

			if (hot0)
				hot0->ll_overrun_count[prid]++;
		}

#ifdef CONFIG_SOF_TELEMETRY_PIPELINES
		if (prid < SOF_TEL_MAX_CORES &&
		    wnd_data->ppl_sec.core_sched[prid].ll_task_count == 0) {
			systick_info[prid].last_time_elapsed = 0;
			systick_info[prid].avg_utilization = 0;
		}
#endif

		telemetry_flush_hot_staging_locked(wnd_data);
		ticks = ++tel_state()->total_tick_counter;
		k_spin_unlock(&tel_lock, key);

		if ((ticks & 0x3ff) == 0)
			k_work_reschedule(&telemetry_bg_dwork, K_NO_WAIT);
	}
#endif
}

/*
 * Register the telemetry debug window slot before notify_host_boot_complete
 * (POST_KERNEL, 0) sends IPC4_FW_READY, then refresh all cold sections once
 * SOF and Zephyr application init (APPLICATION, 99) have completed.
 */
static int telemetry_late_init(void)
{
#ifdef CONFIG_SOF_TELEMETRY_EXTENDED_WINDOW
	telemetry_ext_ready = true;
	telemetry_refresh_cold_sections();
	k_work_reschedule(&telemetry_bg_dwork, TELEMETRY_BG_REFRESH_PERIOD);
#endif
	return 0;
}

SYS_INIT(telemetry_init, PRE_KERNEL_2, 0);
SYS_INIT(telemetry_late_init, APPLICATION, 99);
