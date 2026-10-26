// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.
//
// Per-Core, Per-Pipeline, and Per-Module (LL + DP) Scheduling & Buffer Telemetry
// for SOF Testbench (Host Pthread and Xtensa Simulator).

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <platform/lib/telemetry_sched.h>
#include <sof/audio/component.h>
#include <sof/audio/buffer.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/audio_buffer.h>
#include <sof/audio/ring_buffer.h>
#include <sof/audio/source_api.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <rtos/alloc.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <time.h>

#if !defined(__XTENSA__) && !defined(CONFIG_XT_SIMULATOR)
static volatile char g_telem_lock;
#define TELEM_LOCK() \
	do { \
		while (__atomic_test_and_set(&g_telem_lock, __ATOMIC_ACQUIRE)) { \
		} \
	} while (0)
#define TELEM_UNLOCK()	__atomic_clear(&g_telem_lock, __ATOMIC_RELEASE)
#else
#define TELEM_LOCK()	do {} while (0)
#define TELEM_UNLOCK()	do {} while (0)
#endif

static const struct host_platform_preset g_platform_presets[] = {
	{
		.name              = "tgl",
		.aliases           = "cavs25,adl,rpl,tigerlake,alderlake,raptorlake",
		.desc              = "Intel Tiger Lake / Alder Lake cAVS 2.5 (Xtensa LX6)",
		.hw_ip_version     = 2,
		.num_cores         = 4,
		.freq_hz           = 400000000U,
		.hpsram_size_bytes = 2944UL * 1024UL,	/* 46 x 64 KB EBB banks */
		.lpsram_size_bytes = 64UL * 1024UL,	/* 1 x 64 KB EBB bank */
		.dram_size_bytes   = 16384UL * 1024UL,	/* 16 MB IMR */
	},
	{
		.name              = "tgl-h",
		.aliases           = "cavs25_tgph,tgph",
		.desc              = "Intel Tiger Lake-H cAVS 2.5 (Xtensa LX6, 2-Core)",
		.hw_ip_version     = 2,
		.num_cores         = 2,
		.freq_hz           = 400000000U,
		.hpsram_size_bytes = 2944UL * 1024UL,
		.lpsram_size_bytes = 64UL * 1024UL,
		.dram_size_bytes   = 16384UL * 1024UL,
	},
	{
		.name              = "mtl",
		.aliases           = "arl,arls,ace15,ace15_mtpm,meteorlake,arrowlake",
		.desc              = "Intel Meteor Lake / Arrow Lake ACE 1.5 (Xtensa LX7)",
		.hw_ip_version     = 4,
		.num_cores         = 3,
		.freq_hz           = 393216000U,
		.hpsram_size_bytes = 2816UL * 1024UL,	/* 44 x 64 KB EBB banks */
		.lpsram_size_bytes = 64UL * 1024UL,	/* 1 x 64 KB EBB bank */
		.dram_size_bytes   = 16384UL * 1024UL,	/* 16 MB IMR */
	},
	{
		.name              = "lnl",
		.aliases           = "ace20,ace20_lnl,lunarlake",
		.desc              = "Intel Lunar Lake ACE 2.0 (Xtensa LX7)",
		.hw_ip_version     = 5,
		.num_cores         = 5,
		.freq_hz           = 393216000U,
		.hpsram_size_bytes = 2816UL * 1024UL,	/* 44 x 64 KB EBB banks */
		.lpsram_size_bytes = 64UL * 1024UL,	/* 1 x 64 KB EBB bank */
		.dram_size_bytes   = 16384UL * 1024UL,	/* 16 MB IMR */
	},
	{
		.name              = "ptl",
		.aliases           = "ace30,ace30_ptl,pantherlake",
		.desc              = "Intel Panther Lake ACE 3.0 (Xtensa LX7)",
		.hw_ip_version     = 6,
		.num_cores         = 5,
		.freq_hz           = 442368000U,
		.hpsram_size_bytes = 4608UL * 1024UL,	/* 72 x 64 KB EBB banks */
		.lpsram_size_bytes = 64UL * 1024UL,	/* 1 x 64 KB EBB bank */
		.dram_size_bytes   = 16384UL * 1024UL,	/* 16 MB IMR */
	},
	{
		.name              = "wcl",
		.aliases           = "ace30_wcl,wildcatlake",
		.desc              = "Intel Wildcat Lake ACE 3.0 (Xtensa LX7)",
		.hw_ip_version     = 6,
		.num_cores         = 3,
		.freq_hz           = 614400000U,
		.hpsram_size_bytes = 3456UL * 1024UL,	/* 54 x 64 KB EBB banks */
		.lpsram_size_bytes = 64UL * 1024UL,	/* 1 x 64 KB EBB bank */
		.dram_size_bytes   = 16384UL * 1024UL,	/* 16 MB IMR */
	},
	{
		.name              = "nvl",
		.aliases           = "ace40,ace40_nvl,novalake",
		.desc              = "Intel Nova Lake ACE 4.0 (Xtensa LX7)",
		.hw_ip_version     = 7,
		.num_cores         = 4,
		.freq_hz           = 614400000U,
		.hpsram_size_bytes = 4608UL * 1024UL,	/* 72 x 64 KB EBB banks */
		.lpsram_size_bytes = 64UL * 1024UL,	/* 1 x 64 KB EBB bank */
		.dram_size_bytes   = 16384UL * 1024UL,	/* 16 MB IMR */
	},
	{
		.name              = "nvl-s",
		.aliases           = "ace40_nvls,nvls",
		.desc              = "Intel Nova Lake-S ACE 4.0 (Xtensa LX7, 2-Core)",
		.hw_ip_version     = 7,
		.num_cores         = 2,
		.freq_hz           = 438857000U,
		.hpsram_size_bytes = 4608UL * 1024UL,	/* 72 x 64 KB EBB banks */
		.lpsram_size_bytes = 64UL * 1024UL,	/* 1 x 64 KB EBB bank */
		.dram_size_bytes   = 16384UL * 1024UL,	/* 16 MB IMR */
	},
	{
		.name              = "default",
		.aliases           = "host,sof-testbench4,generic",
		.desc              = "SOF Host Testbench Default (8-Core Virtual DSP)",
		.hw_ip_version     = 4,
		.num_cores         = TB_TELEM_MAX_CORES,
		.freq_hz           = (uint32_t)TB_DSP_NOMINAL_HZ,
		.hpsram_size_bytes = 47UL * 65536UL,	/* 3008 KB (47 x 64 KB) */
		.lpsram_size_bytes = 64UL * 1024UL,	/* 64 KB */
		.dram_size_bytes   = 16384UL * 1024UL,	/* 16 MB */
	},
};

static struct host_sched_telemetry g_sched_telem = {
	.enabled = true,
	.num_cores = TB_TELEM_MAX_CORES,
	.hw_cfg = {
		.platform_name      = "sof-testbench4",
		.arch_desc          = "SOF Host Testbench Default (8-Core Virtual DSP)",
		.hw_ip_version      = 4,
		.num_cores          = TB_TELEM_MAX_CORES,
		.enabled_cores_mask = (1U << TB_TELEM_MAX_CORES) - 1U,
		.default_freq_hz    = (uint32_t)TB_DSP_NOMINAL_HZ,
		.hpsram_size_bytes  = 47UL * 65536UL,
		.lpsram_size_bytes  = 64UL * 1024UL,
		.dram_size_bytes    = 16384UL * 1024UL,
		.sram_bank_size     = 65536U,
		.enforce_mem_limit  = false,
	},
	.sim_cfg = {
		.mode = HOST_WORKLOAD_SIM_OFF,
		.default_mem_domain = HOST_MEM_DOMAIN_SRAM,
		.sram_factor_x100 = 100,
		.dram_factor_x100 = 160,
		.dram_cycles_per_word = 2,
	},
};

static void ensure_sim_cfg_defaults(void)
{
	uint32_t max_mask;
	int c;

	if (g_sched_telem.hw_cfg.num_cores == 0 ||
	    g_sched_telem.hw_cfg.num_cores > TB_TELEM_MAX_CORES)
		g_sched_telem.hw_cfg.num_cores = TB_TELEM_MAX_CORES;
	g_sched_telem.num_cores = g_sched_telem.hw_cfg.num_cores;

	max_mask = (1U << g_sched_telem.hw_cfg.num_cores) - 1U;
	if ((g_sched_telem.hw_cfg.enabled_cores_mask & max_mask) == 0)
		g_sched_telem.hw_cfg.enabled_cores_mask = max_mask;
	else
		g_sched_telem.hw_cfg.enabled_cores_mask &= max_mask;
	g_sched_telem.hw_cfg.enabled_cores_mask |= 1U; /* Core 0 is always enabled */

	if (g_sched_telem.hw_cfg.default_freq_hz == 0)
		g_sched_telem.hw_cfg.default_freq_hz = (uint32_t)TB_DSP_NOMINAL_HZ;
	if (g_sched_telem.hw_cfg.hpsram_size_bytes == 0)
		g_sched_telem.hw_cfg.hpsram_size_bytes = 47UL * 65536UL;
	if (g_sched_telem.hw_cfg.lpsram_size_bytes == 0)
		g_sched_telem.hw_cfg.lpsram_size_bytes = 64UL * 1024UL;
	if (g_sched_telem.hw_cfg.dram_size_bytes == 0)
		g_sched_telem.hw_cfg.dram_size_bytes = 16384UL * 1024UL;
	if (g_sched_telem.hw_cfg.sram_bank_size == 0)
		g_sched_telem.hw_cfg.sram_bank_size = 65536U;
	if (g_sched_telem.hw_cfg.platform_name[0] == '\0')
		snprintf(g_sched_telem.hw_cfg.platform_name,
			 sizeof(g_sched_telem.hw_cfg.platform_name), "sof-testbench4");

	if (g_sched_telem.sim_cfg.sram_factor_x100 == 0)
		g_sched_telem.sim_cfg.sram_factor_x100 = 100;
	if (g_sched_telem.sim_cfg.dram_factor_x100 == 0)
		g_sched_telem.sim_cfg.dram_factor_x100 = 160;
	if (g_sched_telem.sim_cfg.dram_cycles_per_word == 0)
		g_sched_telem.sim_cfg.dram_cycles_per_word = 2;

	for (c = 0; c < TB_TELEM_MAX_CORES; c++) {
		if (g_sched_telem.sim_cfg.core_freq_hz[c] == 0)
			g_sched_telem.sim_cfg.core_freq_hz[c] =
				g_sched_telem.hw_cfg.default_freq_hz;
		g_sched_telem.cores[c].freq_hz = g_sched_telem.sim_cfg.core_freq_hz[c];
		g_sched_telem.cores[c].ll_budget_cycles =
			g_sched_telem.cores[c].freq_hz / 1000U;
	}
}

void host_telem_reset(void)
{
	struct host_workload_sim_cfg saved_cfg;
	struct host_platform_hw_cfg saved_hw;

	TELEM_LOCK();
	ensure_sim_cfg_defaults();
	saved_cfg = g_sched_telem.sim_cfg;
	saved_hw = g_sched_telem.hw_cfg;
	memset(&g_sched_telem, 0, sizeof(g_sched_telem));
	g_sched_telem.enabled = true;
	g_sched_telem.hw_cfg = saved_hw;
	g_sched_telem.num_cores = saved_hw.num_cores;
	g_sched_telem.sim_cfg = saved_cfg;
	ensure_sim_cfg_defaults();
	TELEM_UNLOCK();
}

const struct host_platform_preset *host_platform_get_presets(size_t *out_count)
{
	if (out_count)
		*out_count = ARRAY_SIZE(g_platform_presets);
	return g_platform_presets;
}

const struct host_platform_preset *host_platform_find_preset(const char *name)
{
	size_t i, nlen;

	if (!name || !*name)
		return NULL;
	nlen = strlen(name);

	for (i = 0; i < ARRAY_SIZE(g_platform_presets); i++) {
		const struct host_platform_preset *p = &g_platform_presets[i];
		const char *cur = p->aliases;

		if (strcasecmp(p->name, name) == 0)
			return p;

		while (cur && *cur) {
			const char *comma = strchr(cur, ',');
			size_t alen = comma ? (size_t)(comma - cur) : strlen(cur);

			if (alen == nlen && strncasecmp(cur, name, nlen) == 0)
				return p;
			cur = comma ? (comma + 1) : NULL;
		}
	}
	return NULL;
}

int host_platform_apply_preset(const char *name)
{
	const struct host_platform_preset *p = host_platform_find_preset(name);
	int c;

	if (!p)
		return -1;

	TELEM_LOCK();
	ensure_sim_cfg_defaults();
	snprintf(g_sched_telem.hw_cfg.platform_name,
		 sizeof(g_sched_telem.hw_cfg.platform_name), "%s", p->name);
	snprintf(g_sched_telem.hw_cfg.arch_desc,
		 sizeof(g_sched_telem.hw_cfg.arch_desc), "%s", p->desc);
	g_sched_telem.hw_cfg.hw_ip_version = p->hw_ip_version;
	g_sched_telem.hw_cfg.num_cores =
		(p->num_cores <= TB_TELEM_MAX_CORES) ? p->num_cores : TB_TELEM_MAX_CORES;
	g_sched_telem.num_cores = g_sched_telem.hw_cfg.num_cores;
	g_sched_telem.hw_cfg.enabled_cores_mask =
		(1U << g_sched_telem.hw_cfg.num_cores) - 1U;
	g_sched_telem.hw_cfg.default_freq_hz = p->freq_hz;
	g_sched_telem.hw_cfg.hpsram_size_bytes = p->hpsram_size_bytes;
	g_sched_telem.hw_cfg.lpsram_size_bytes = p->lpsram_size_bytes;
	g_sched_telem.hw_cfg.dram_size_bytes = p->dram_size_bytes;
	g_sched_telem.hw_cfg.sram_bank_size = 65536U;
	g_sched_telem.hw_cfg.enforce_mem_limit =
		(strcasecmp(p->name, "default") != 0);

	for (c = 0; c < TB_TELEM_MAX_CORES; c++) {
		g_sched_telem.sim_cfg.core_freq_hz[c] = p->freq_hz;
		g_sched_telem.cores[c].freq_hz = p->freq_hz;
		g_sched_telem.cores[c].ll_budget_cycles = p->freq_hz / 1000U;
	}
	TELEM_UNLOCK();
	return 0;
}

int host_platform_set_core_count(uint32_t num_cores)
{
	if (num_cores == 0 || num_cores > TB_TELEM_MAX_CORES)
		return -1;

	TELEM_LOCK();
	ensure_sim_cfg_defaults();
	g_sched_telem.hw_cfg.num_cores = num_cores;
	g_sched_telem.num_cores = num_cores;
	g_sched_telem.hw_cfg.enabled_cores_mask = (1U << num_cores) - 1U;
	TELEM_UNLOCK();
	return 0;
}

uint32_t host_platform_get_core_count(void)
{
	ensure_sim_cfg_defaults();
	return g_sched_telem.hw_cfg.num_cores;
}

void host_platform_set_memory(size_t hpsram_bytes, size_t lpsram_bytes,
			      size_t dram_bytes, bool enforce_limit)
{
	TELEM_LOCK();
	ensure_sim_cfg_defaults();
	if (hpsram_bytes > 0)
		g_sched_telem.hw_cfg.hpsram_size_bytes = hpsram_bytes;
	if (lpsram_bytes > 0)
		g_sched_telem.hw_cfg.lpsram_size_bytes = lpsram_bytes;
	if (dram_bytes > 0)
		g_sched_telem.hw_cfg.dram_size_bytes = dram_bytes;
	g_sched_telem.hw_cfg.enforce_mem_limit = enforce_limit;
	TELEM_UNLOCK();
}

size_t host_platform_get_hpsram_size(void)
{
	ensure_sim_cfg_defaults();
	return g_sched_telem.hw_cfg.hpsram_size_bytes;
}

size_t host_platform_get_lpsram_size(void)
{
	ensure_sim_cfg_defaults();
	return g_sched_telem.hw_cfg.lpsram_size_bytes;
}

size_t host_platform_get_dram_size(void)
{
	ensure_sim_cfg_defaults();
	return g_sched_telem.hw_cfg.dram_size_bytes;
}

#define TB_ALLOC_TRACK_SLOTS	8192U

enum host_alloc_pool {
	HOST_ALLOC_POOL_NONE   = 0,
	HOST_ALLOC_POOL_HPSRAM = 1,
	HOST_ALLOC_POOL_LPSRAM = 2,
	HOST_ALLOC_POOL_DRAM   = 3,
};

struct host_alloc_record {
	const void *ptr;
	size_t bytes;
	uint8_t pool;
};

static struct host_alloc_record g_alloc_table[TB_ALLOC_TRACK_SLOTS];

static void record_tracked_alloc_locked(const void *ptr, size_t bytes, uint8_t pool)
{
	uint32_t h, i;

	if (!ptr || pool == HOST_ALLOC_POOL_NONE)
		return;

	h = (uint32_t)(((uintptr_t)ptr >> 4) ^ ((uintptr_t)ptr >> 16));
	for (i = 0; i < TB_ALLOC_TRACK_SLOTS; i++) {
		uint32_t idx = (h + i) & (TB_ALLOC_TRACK_SLOTS - 1U);

		if (!g_alloc_table[idx].ptr || g_alloc_table[idx].ptr == ptr) {
			g_alloc_table[idx].ptr = ptr;
			g_alloc_table[idx].bytes = bytes;
			g_alloc_table[idx].pool = pool;
			return;
		}
	}
}

static bool pop_tracked_alloc_locked(const void *ptr, size_t *out_bytes, uint8_t *out_pool)
{
	uint32_t h, i;

	if (!ptr)
		return false;

	h = (uint32_t)(((uintptr_t)ptr >> 4) ^ ((uintptr_t)ptr >> 16));
	for (i = 0; i < TB_ALLOC_TRACK_SLOTS; i++) {
		uint32_t idx = (h + i) & (TB_ALLOC_TRACK_SLOTS - 1U);

		if (g_alloc_table[idx].ptr == ptr) {
			*out_bytes = g_alloc_table[idx].bytes;
			*out_pool = g_alloc_table[idx].pool;
			g_alloc_table[idx].ptr = NULL;
			g_alloc_table[idx].bytes = 0;
			g_alloc_table[idx].pool = HOST_ALLOC_POOL_NONE;
			return true;
		}
	}
	return false;
}

void host_platform_reset_mem_stats(void)
{
	TELEM_LOCK();
	g_sched_telem.hw_cfg.hpsram_used_bytes = 0;
	g_sched_telem.hw_cfg.hpsram_peak_bytes = 0;
	g_sched_telem.hw_cfg.lpsram_used_bytes = 0;
	g_sched_telem.hw_cfg.lpsram_peak_bytes = 0;
	g_sched_telem.hw_cfg.dram_used_bytes = 0;
	g_sched_telem.hw_cfg.dram_peak_bytes = 0;
	g_sched_telem.hw_cfg.total_alloc_calls = 0;
	g_sched_telem.hw_cfg.total_free_calls = 0;
	g_sched_telem.hw_cfg.alloc_failures = 0;
	memset(g_alloc_table, 0, sizeof(g_alloc_table));
	TELEM_UNLOCK();
}

bool host_platform_account_alloc(const void *ptr, uint32_t flags, size_t bytes)
{
	uint8_t pool = HOST_ALLOC_POOL_NONE;
	bool ok = true;

	TELEM_LOCK();
	ensure_sim_cfg_defaults();

	if ((flags & SOF_MEM_FLAG_LOW_POWER) != 0) {
		if (g_sched_telem.hw_cfg.enforce_mem_limit &&
		    g_sched_telem.hw_cfg.lpsram_used_bytes + bytes >
		    g_sched_telem.hw_cfg.lpsram_size_bytes) {
			g_sched_telem.hw_cfg.alloc_failures++;
			ok = false;
		} else {
			g_sched_telem.hw_cfg.lpsram_used_bytes += bytes;
			if (g_sched_telem.hw_cfg.lpsram_used_bytes >
			    g_sched_telem.hw_cfg.lpsram_peak_bytes)
				g_sched_telem.hw_cfg.lpsram_peak_bytes =
					g_sched_telem.hw_cfg.lpsram_used_bytes;
			g_sched_telem.hw_cfg.total_alloc_calls++;
			pool = HOST_ALLOC_POOL_LPSRAM;
		}
	} else if ((flags & SOF_MEM_FLAG_L3) != 0 ||
		   g_sched_telem.sim_cfg.default_mem_domain == HOST_MEM_DOMAIN_DRAM) {
		if (g_sched_telem.hw_cfg.enforce_mem_limit &&
		    g_sched_telem.hw_cfg.dram_used_bytes + bytes >
		    g_sched_telem.hw_cfg.dram_size_bytes) {
			g_sched_telem.hw_cfg.alloc_failures++;
			ok = false;
		} else {
			g_sched_telem.hw_cfg.dram_used_bytes += bytes;
			if (g_sched_telem.hw_cfg.dram_used_bytes >
			    g_sched_telem.hw_cfg.dram_peak_bytes)
				g_sched_telem.hw_cfg.dram_peak_bytes =
					g_sched_telem.hw_cfg.dram_used_bytes;
			/* Also mirror in HPSRAM peak if running from SRAM/DRAM unified pool */
			g_sched_telem.hw_cfg.hpsram_used_bytes += bytes;
			if (g_sched_telem.hw_cfg.hpsram_used_bytes >
			    g_sched_telem.hw_cfg.hpsram_peak_bytes)
				g_sched_telem.hw_cfg.hpsram_peak_bytes =
					g_sched_telem.hw_cfg.hpsram_used_bytes;
			g_sched_telem.hw_cfg.total_alloc_calls++;
			pool = HOST_ALLOC_POOL_DRAM;
		}
	} else {
		if (g_sched_telem.hw_cfg.enforce_mem_limit &&
		    g_sched_telem.hw_cfg.hpsram_used_bytes + bytes >
		    g_sched_telem.hw_cfg.hpsram_size_bytes) {
			g_sched_telem.hw_cfg.alloc_failures++;
			ok = false;
		} else {
			g_sched_telem.hw_cfg.hpsram_used_bytes += bytes;
			if (g_sched_telem.hw_cfg.hpsram_used_bytes >
			    g_sched_telem.hw_cfg.hpsram_peak_bytes)
				g_sched_telem.hw_cfg.hpsram_peak_bytes =
					g_sched_telem.hw_cfg.hpsram_used_bytes;
			g_sched_telem.hw_cfg.total_alloc_calls++;
			pool = HOST_ALLOC_POOL_HPSRAM;
		}
	}

	if (ok)
		record_tracked_alloc_locked(ptr, bytes, pool);
	TELEM_UNLOCK();
	return ok;
}

void host_platform_account_free(const void *ptr, size_t bytes)
{
	size_t tracked_bytes = bytes;
	uint8_t pool = HOST_ALLOC_POOL_HPSRAM;

	TELEM_LOCK();
	(void)pop_tracked_alloc_locked(ptr, &tracked_bytes, &pool);

	if (pool == HOST_ALLOC_POOL_LPSRAM) {
		if (g_sched_telem.hw_cfg.lpsram_used_bytes >= tracked_bytes)
			g_sched_telem.hw_cfg.lpsram_used_bytes -= tracked_bytes;
		else
			g_sched_telem.hw_cfg.lpsram_used_bytes = 0;
	} else if (pool == HOST_ALLOC_POOL_DRAM) {
		if (g_sched_telem.hw_cfg.dram_used_bytes >= tracked_bytes)
			g_sched_telem.hw_cfg.dram_used_bytes -= tracked_bytes;
		else
			g_sched_telem.hw_cfg.dram_used_bytes = 0;

		if (g_sched_telem.hw_cfg.hpsram_used_bytes >= tracked_bytes)
			g_sched_telem.hw_cfg.hpsram_used_bytes -= tracked_bytes;
		else
			g_sched_telem.hw_cfg.hpsram_used_bytes = 0;
	} else {
		if (g_sched_telem.hw_cfg.hpsram_used_bytes >= tracked_bytes)
			g_sched_telem.hw_cfg.hpsram_used_bytes -= tracked_bytes;
		else
			g_sched_telem.hw_cfg.hpsram_used_bytes = 0;
	}

	g_sched_telem.hw_cfg.total_free_calls++;
	TELEM_UNLOCK();
}

int host_cpu_num_cores(void)
{
	ensure_sim_cfg_defaults();
	return (int)g_sched_telem.hw_cfg.num_cores;
}

int host_cpu_enable_core(int id)
{
	ensure_sim_cfg_defaults();
	if (id < 0 || (uint32_t)id >= g_sched_telem.hw_cfg.num_cores)
		return -1;
	g_sched_telem.hw_cfg.enabled_cores_mask |= (1U << id);
	return 0;
}

void host_cpu_disable_core(int id)
{
	ensure_sim_cfg_defaults();
	if (id <= 0 || (uint32_t)id >= g_sched_telem.hw_cfg.num_cores)
		return;
	g_sched_telem.hw_cfg.enabled_cores_mask &= ~(1U << id);
}

int host_cpu_is_core_enabled(int id)
{
	ensure_sim_cfg_defaults();
	if (id < 0 || (uint32_t)id >= g_sched_telem.hw_cfg.num_cores)
		return 0;
	return (g_sched_telem.hw_cfg.enabled_cores_mask & (1U << id)) ? 1 : 0;
}

int host_cpu_enabled_cores(void)
{
	ensure_sim_cfg_defaults();
	return (int)g_sched_telem.hw_cfg.enabled_cores_mask;
}

void host_telem_set_enabled(bool enabled)
{
	g_sched_telem.enabled = enabled;
}

struct host_sched_telemetry *host_telem_get(void)
{
	ensure_sim_cfg_defaults();
	return &g_sched_telem;
}

void host_workload_sim_set_mode(uint32_t mode)
{
	TELEM_LOCK();
	ensure_sim_cfg_defaults();
	if (mode > HOST_WORKLOAD_SIM_REALTIME)
		mode = HOST_WORKLOAD_SIM_REALTIME;
	g_sched_telem.sim_cfg.mode = mode;
	TELEM_UNLOCK();
}

uint32_t host_workload_sim_get_mode(void)
{
	ensure_sim_cfg_defaults();
	return g_sched_telem.sim_cfg.mode;
}

void host_workload_sim_set_core_freq(int core, uint32_t freq_hz)
{
	int c;

	if (freq_hz < 1000000U)
		freq_hz = (uint32_t)TB_DSP_NOMINAL_HZ;

	TELEM_LOCK();
	ensure_sim_cfg_defaults();
	if (core < 0) {
		g_sched_telem.hw_cfg.default_freq_hz = freq_hz;
		for (c = 0; c < TB_TELEM_MAX_CORES; c++) {
			g_sched_telem.sim_cfg.core_freq_hz[c] = freq_hz;
			g_sched_telem.cores[c].freq_hz = freq_hz;
			g_sched_telem.cores[c].ll_budget_cycles = freq_hz / 1000U;
		}
	} else if (core < TB_TELEM_MAX_CORES) {
		if (core == 0)
			g_sched_telem.hw_cfg.default_freq_hz = freq_hz;
		g_sched_telem.sim_cfg.core_freq_hz[core] = freq_hz;
		g_sched_telem.cores[core].freq_hz = freq_hz;
		g_sched_telem.cores[core].ll_budget_cycles = freq_hz / 1000U;
	}
	TELEM_UNLOCK();
}

uint32_t host_workload_sim_get_core_freq(int core)
{
	ensure_sim_cfg_defaults();
	if (core < 0 || core >= TB_TELEM_MAX_CORES)
		core = 0;
	return g_sched_telem.sim_cfg.core_freq_hz[core];
}

void host_workload_sim_set_mem_domain(uint32_t mem_domain, uint32_t dram_factor_x100,
				      uint32_t dram_cycles_per_word)
{
	TELEM_LOCK();
	ensure_sim_cfg_defaults();
	g_sched_telem.sim_cfg.default_mem_domain =
		(mem_domain == HOST_MEM_DOMAIN_DRAM) ? HOST_MEM_DOMAIN_DRAM : HOST_MEM_DOMAIN_SRAM;
	if (dram_factor_x100 >= 100U && dram_factor_x100 <= 1000U)
		g_sched_telem.sim_cfg.dram_factor_x100 = dram_factor_x100;
	if (dram_cycles_per_word <= 64U)
		g_sched_telem.sim_cfg.dram_cycles_per_word = dram_cycles_per_word;
	TELEM_UNLOCK();
}

int host_workload_sim_set_mod_override(uint32_t comp_id, uint32_t cpc_override,
				       uint32_t mem_domain_override)
{
	uint32_t i;
	int rc = -1;

	TELEM_LOCK();
	ensure_sim_cfg_defaults();
	for (i = 0; i < g_sched_telem.sim_cfg.num_overrides; i++) {
		if (g_sched_telem.sim_cfg.overrides[i].active &&
		    g_sched_telem.sim_cfg.overrides[i].comp_id == comp_id) {
			g_sched_telem.sim_cfg.overrides[i].cpc_override = cpc_override;
			g_sched_telem.sim_cfg.overrides[i].mem_domain_override =
				mem_domain_override;
			rc = 0;
			break;
		}
	}
	if (rc < 0 && g_sched_telem.sim_cfg.num_overrides < TB_TELEM_MAX_OVERRIDES) {
		struct host_workload_mod_override *ov =
			&g_sched_telem.sim_cfg.overrides[g_sched_telem.sim_cfg.num_overrides++];
		ov->active = true;
		ov->comp_id = comp_id;
		ov->cpc_override = cpc_override;
		ov->mem_domain_override = mem_domain_override;
		rc = 0;
	}
	TELEM_UNLOCK();
	return rc;
}

void host_workload_sim_clear_overrides(void)
{
	TELEM_LOCK();
	g_sched_telem.sim_cfg.num_overrides = 0;
	memset(g_sched_telem.sim_cfg.overrides, 0, sizeof(g_sched_telem.sim_cfg.overrides));
	TELEM_UNLOCK();
}

static uint32_t get_core_freq_hz(uint32_t core)
{
	ensure_sim_cfg_defaults();
	if (core >= TB_TELEM_MAX_CORES)
		core = 0;
	return g_sched_telem.sim_cfg.core_freq_hz[core];
}

static uint64_t cycles_to_ns(uint64_t cycles, uint32_t freq_hz)
{
	uint64_t f_khz = (freq_hz >= 1000U) ? (freq_hz / 1000U) : (TB_DSP_NOMINAL_HZ / 1000ULL);

	return (cycles * 1000000ULL) / f_khz;
}

static void pace_execution_ns(uint64_t delay_ns)
{
#if !defined(__XTENSA__) && !defined(CONFIG_XT_SIMULATOR)
	struct timespec t0, now, req;
	uint64_t start_ns, target_ns, cur_ns;

	if (delay_ns == 0)
		return;
	if (delay_ns > 50000000ULL)
		delay_ns = 50000000ULL; /* Cap single-step sleep at 50 ms */

	if (clock_gettime(CLOCK_MONOTONIC, &t0) != 0)
		return;
	start_ns = (uint64_t)t0.tv_sec * 1000000000ULL + (uint64_t)t0.tv_nsec;
	target_ns = start_ns + delay_ns;

	if (delay_ns >= 80000ULL) {
		uint64_t sleep_ns = delay_ns - 40000ULL;

		req.tv_sec = (time_t)(sleep_ns / 1000000000ULL);
		req.tv_nsec = (long)(sleep_ns % 1000000000ULL);
		nanosleep(&req, NULL);
	}

	do {
		if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
			break;
		cur_ns = (uint64_t)now.tv_sec * 1000000000ULL + (uint64_t)now.tv_nsec;
	} while (cur_ns < target_ns);
#else
	(void)delay_ns;
#endif
}

uint64_t host_telem_timestamp_cycles(void)
{
#if defined(__XTENSA__)
	uint32_t ccount = 0;

	__asm__ __volatile__("rsr.ccount %0" : "=a"(ccount));
	return (uint64_t)ccount;
#elif defined(CONFIG_XT_SIMULATOR)
	clock_t c = clock();

	return (uint64_t)c * ((TB_DSP_NOMINAL_HZ / (uint64_t)CLOCKS_PER_SEC));
#else
	struct timespec ts;
	uint64_t ns;

	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0)
		clock_gettime(CLOCK_MONOTONIC, &ts);

	ns = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
	/* Convert nanoseconds to nominal 400 MHz DSP cycles (0.4 cycles/ns) */
	return (ns * (TB_DSP_NOMINAL_HZ / 1000000ULL)) / 1000ULL;
#endif
}

static uint64_t host_telem_wall_ns(void)
{
#if !defined(__XTENSA__) && !defined(CONFIG_XT_SIMULATOR)
	struct timespec ts;
	uint64_t now_ns;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		return 0;
	now_ns = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
	if (g_sched_telem.run_start_wall_ns == 0)
		g_sched_telem.run_start_wall_ns = now_ns;
	return (now_ns >= g_sched_telem.run_start_wall_ns) ?
	       (now_ns - g_sched_telem.run_start_wall_ns) : 0;
#else
	return 0;
#endif
}

void host_telem_ll_tick_begin(void)
{
	int c;

	if (!g_sched_telem.enabled)
		return;

	TELEM_LOCK();
	if (g_sched_telem.run_start_wall_ns == 0)
		(void)host_telem_wall_ns();
	g_sched_telem.current_ll_tick++;
	for (c = 0; c < TB_TELEM_MAX_CORES; c++)
		g_sched_telem.cores[c]._tick_ll_offset_cycles = 0;
	TELEM_UNLOCK();
}

void host_telem_trace_clear(void)
{
	TELEM_LOCK();
	g_sched_telem.trace_head = 0;
	g_sched_telem.trace_count = 0;
	g_sched_telem.trace_total_events = 0;
	memset(g_sched_telem.trace_events, 0, sizeof(g_sched_telem.trace_events));
	TELEM_UNLOCK();
}

uint32_t host_telem_trace_read(struct host_sched_trace_event *out_events,
			       uint32_t max_events, uint32_t since_seq_id,
			       int core_filter, int pipe_filter,
			       int comp_filter, int domain_filter)
{
	uint32_t count, start_idx, i, copied = 0;

	if (!out_events || max_events == 0)
		return 0;

	TELEM_LOCK();
	count = g_sched_telem.trace_count;
	if (count > TB_TELEM_TRACE_CAPACITY)
		count = TB_TELEM_TRACE_CAPACITY;
	start_idx = (g_sched_telem.trace_head + TB_TELEM_TRACE_CAPACITY - count) %
		    TB_TELEM_TRACE_CAPACITY;

	for (i = 0; i < count && copied < max_events; i++) {
		const struct host_sched_trace_event *ev =
			&g_sched_telem.trace_events[(start_idx + i) % TB_TELEM_TRACE_CAPACITY];

		if (since_seq_id > 0 && ev->seq_id <= since_seq_id)
			continue;
		if (core_filter >= 0 && ev->core_id != (uint8_t)core_filter)
			continue;
		if (pipe_filter >= 0 && ev->pipeline_id != (uint16_t)pipe_filter)
			continue;
		if (comp_filter >= 0 && ev->comp_id != (uint32_t)comp_filter)
			continue;
		if (domain_filter >= 0 && ev->event_type != (uint8_t)domain_filter)
			continue;

		out_events[copied++] = *ev;
	}
	TELEM_UNLOCK();

	return copied;
}

static void record_mod_sched_event(struct host_mod_telem *m, uint32_t core_id,
				   uint32_t freq_hz, uint8_t event_type,
				   uint32_t mod_iter_nr, uint64_t delta_cycles,
				   uint32_t cpc_budget, uint32_t bytes_in,
				   uint32_t bytes_out, uint64_t wall_start_ns,
				   uint64_t wall_complete_ns, int ret_status)
{
	struct host_core_telem *c;
	struct host_sched_trace_event *ev;
	uint32_t tick_nr;
	uint64_t tick_budget_cyc, tick_base_cyc, tick_base_ns;
	uint64_t virt_start_cyc, virt_complete_cyc, virt_start_ns, virt_complete_ns, deadline_ns;
	uint64_t exec_ns;

	if (core_id >= TB_TELEM_MAX_CORES)
		core_id = 0;

	TELEM_LOCK();
	c = &g_sched_telem.cores[core_id];
	tick_nr = g_sched_telem.current_ll_tick ? g_sched_telem.current_ll_tick : 1U;
	tick_budget_cyc = (freq_hz >= 1000U) ? (freq_hz / 1000U) : 400000ULL;
	tick_base_cyc = (uint64_t)(tick_nr - 1U) * tick_budget_cyc;
	tick_base_ns = (uint64_t)(tick_nr - 1U) * 1000000ULL;
	exec_ns = cycles_to_ns(delta_cycles, freq_hz);

	if (event_type != HOST_SCHED_EVT_DP_RUN) {
		virt_start_cyc = tick_base_cyc + c->_tick_ll_offset_cycles;
		virt_complete_cyc = virt_start_cyc + delta_cycles;
		c->_tick_ll_offset_cycles += delta_cycles;
		virt_start_ns = tick_base_ns +
				cycles_to_ns(virt_start_cyc - tick_base_cyc, freq_hz);
		virt_complete_ns = virt_start_ns + exec_ns;
		deadline_ns = (uint64_t)tick_nr * 1000000ULL;
	} else {
		uint64_t ll_end_cyc = tick_base_cyc + c->_tick_ll_offset_cycles;
		uint64_t dp_period_ns = (uint64_t)(m->period_us ? m->period_us : 1000U) * 1000ULL;

		if (c->_dp_cursor_cycles < ll_end_cyc)
			c->_dp_cursor_cycles = ll_end_cyc;
		virt_start_cyc = c->_dp_cursor_cycles;
		virt_complete_cyc = virt_start_cyc + delta_cycles;
		c->_dp_cursor_cycles = virt_complete_cyc;
		virt_start_ns = cycles_to_ns(virt_start_cyc, freq_hz);
		virt_complete_ns = virt_start_ns + exec_ns;
		deadline_ns = tick_base_ns + dp_period_ns;
	}

	if (event_type != HOST_SCHED_EVT_DP_SYNC) {
		m->last_start_cycles = virt_start_cyc;
		m->last_complete_cycles = virt_complete_cyc;
		m->last_start_ns = virt_start_ns;
		m->last_complete_ns = virt_complete_ns;
		m->last_deadline_ns = deadline_ns;
		m->last_wall_start_ns = wall_start_ns;
		m->last_wall_complete_ns = wall_complete_ns;
	}

	ev = &g_sched_telem.trace_events[g_sched_telem.trace_head];
	ev->seq_id = ++g_sched_telem.trace_total_events;
	ev->ll_tick_nr = tick_nr;
	ev->mod_iter_nr = mod_iter_nr;
	ev->comp_id = m->comp_id;
	ev->pipeline_id = (uint16_t)m->pipeline_id;
	ev->core_id = (uint8_t)core_id;
	ev->event_type = event_type;
	ev->mem_domain = (uint8_t)m->mem_domain;
	ev->cpc_violation = (cpc_budget > 0 && delta_cycles > cpc_budget) ? 1U : 0U;
	ev->period_us = (uint16_t)(m->period_us ? m->period_us : 1000U);
	ev->start_cycles = virt_start_cyc;
	ev->complete_cycles = virt_complete_cyc;
	ev->duration_cycles = (uint32_t)delta_cycles;
	ev->cpc_budget = cpc_budget;
	ev->start_ns = virt_start_ns;
	ev->complete_ns = virt_complete_ns;
	ev->deadline_ns = deadline_ns;
	ev->wall_start_ns = wall_start_ns;
	ev->wall_complete_ns = wall_complete_ns;
	ev->bytes_in = bytes_in;
	ev->bytes_out = bytes_out;
	ev->ret_status = ret_status;

	g_sched_telem.trace_head = (g_sched_telem.trace_head + 1U) % TB_TELEM_TRACE_CAPACITY;
	if (g_sched_telem.trace_count < TB_TELEM_TRACE_CAPACITY)
		g_sched_telem.trace_count++;

	TELEM_UNLOCK();
}

/*
 * Fast integer log2 in Q8 fixed-point (256 * log2(val)) for val > 0.
 * Zero floating-point or libm dependency for Xtensa simulator compatibility.
 */
static int32_t fast_log2_q8(uint64_t val)
{
	int k = 0;
	uint64_t tmp = val;
	uint32_t m, frac_q8;

	if (val == 0)
		return 0;

	while (tmp >>= 1)
		k++;

	if (k >= 8)
		m = (uint32_t)((val >> (k - 8)) & 0xFFU);
	else
		m = (uint32_t)((val << (8 - k)) & 0xFFU);

	/* Polynomial approximation log2(1 + f) ~= f + f*(1-f)*3/8 for f = m/256 */
	frac_q8 = m + ((m * (256U - m)) >> 9) + ((m * (256U - m)) >> 10);
	return (k << 8) + (int32_t)frac_q8;
}

/* Convert 16-bit peak amplitude [0..32768] to 0.01 dBFS [-12000..0] */
static int16_t peak_s16_to_dbfs_x100(uint32_t peak)
{
	int32_t log2_q8, db_x100;

	if (peak == 0)
		return -12000;
	if (peak >= 32768U)
		return 0;

	/* 20 * 100 * log10(2) ~= 602.06 */
	log2_q8 = fast_log2_q8(peak) - (15 << 8);
	db_x100 = (log2_q8 * 602) / 256;
	if (db_x100 < -12000)
		db_x100 = -12000;
	if (db_x100 > 0)
		db_x100 = 0;
	return (int16_t)db_x100;
}

/* Convert 16-bit mean square [0..2^30] to 0.01 dBFS [-12000..0] */
static int16_t ms_s16_to_dbfs_x100(uint64_t mean_sq)
{
	int32_t log2_q8, db_x100;

	if (mean_sq == 0)
		return -12000;
	if (mean_sq >= (1ULL << 30))
		return 0;

	/* 10 * 100 * log10(2) ~= 301.03 */
	log2_q8 = fast_log2_q8(mean_sq) - (30 << 8);
	db_x100 = (log2_q8 * 301) / 256;
	if (db_x100 < -12000)
		db_x100 = -12000;
	if (db_x100 > 0)
		db_x100 = 0;
	return (int16_t)db_x100;
}

/*
 * Capture up to TB_MOD_TAP_SAMPLES normalized S16 samples from a circular buffer
 * and compute RMS and Peak in 0.01 dBFS.
 */
static void capture_circular_samples(const void *base_addr, size_t buf_size,
				     size_t start_offset, size_t avail_bytes,
				     uint16_t frame_fmt,
				     int16_t *out_samples, uint16_t *out_num_samples,
				     int16_t *out_rms_dbfs_x100,
				     int16_t *out_peak_dbfs_x100)
{
	const uint8_t *buf = (const uint8_t *)base_addr;
	size_t bps = 2;
	size_t n_avail, n_tap, i;
	uint32_t peak = 0;
	uint64_t sum_sq = 0;

	if (!buf || buf_size == 0 || avail_bytes == 0) {
		*out_num_samples = 0;
		*out_rms_dbfs_x100 = -12000;
		*out_peak_dbfs_x100 = -12000;
		return;
	}

	switch (frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		bps = 2;
		break;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
	case SOF_IPC_FRAME_FLOAT:
		bps = 4;
		break;
	case SOF_IPC_FRAME_S24_3LE:
		bps = 3;
		break;
	default:
		bps = 2;
		break;
	}

	if (avail_bytes > buf_size)
		avail_bytes = buf_size;

	n_avail = avail_bytes / bps;
	n_tap = (n_avail < TB_MOD_TAP_SAMPLES) ? n_avail : TB_MOD_TAP_SAMPLES;
	if (n_tap == 0) {
		*out_num_samples = 0;
		*out_rms_dbfs_x100 = -12000;
		*out_peak_dbfs_x100 = -12000;
		return;
	}

	for (i = 0; i < n_tap; i++) {
		size_t off = (start_offset + i * bps) % buf_size;
		int16_t s = 0;
		uint32_t abs_s;

		if (bps == 2 && off + 2 <= buf_size) {
			s = (int16_t)((uint16_t)buf[off] | ((uint16_t)buf[off + 1] << 8));
		} else if (bps == 4 && off + 4 <= buf_size) {
			int32_t v = (int32_t)((uint32_t)buf[off] |
					      ((uint32_t)buf[off + 1] << 8) |
					      ((uint32_t)buf[off + 2] << 16) |
					      ((uint32_t)buf[off + 3] << 24));
			if (frame_fmt == SOF_IPC_FRAME_S24_4LE) {
				v = (v << 8) >> 8;
				s = (int16_t)(v >> 8);
			} else {
				s = (int16_t)(v >> 16);
			}
		} else if (bps == 3 && off + 3 <= buf_size) {
			int32_t v = (int32_t)buf[off] |
				    ((int32_t)buf[off + 1] << 8) |
				    ((int32_t)(int8_t)buf[off + 2] << 16);
			s = (int16_t)(v >> 8);
		}

		out_samples[i] = s;
		abs_s = (s < 0) ? (uint32_t)(-(int32_t)s) : (uint32_t)s;
		if (abs_s > peak)
			peak = abs_s;
		sum_sq += (uint64_t)abs_s * (uint64_t)abs_s;
	}

	*out_num_samples = (uint16_t)n_tap;
	*out_peak_dbfs_x100 = peak_s16_to_dbfs_x100(peak);
	*out_rms_dbfs_x100 = ms_s16_to_dbfs_x100(sum_sq / n_tap);
}

static void update_core_util(int core, struct host_core_telem *c)
{
	uint32_t freq_hz = get_core_freq_hz((uint32_t)core);
	uint64_t total_kcps = (uint64_t)c->ll_avg_kcps + (uint64_t)c->dp_avg_kcps;
	uint64_t max_kcps = freq_hz / 1000ULL;

	c->freq_hz = freq_hz;
	c->ll_budget_cycles = freq_hz / 1000U;

	if (max_kcps > 0) {
		uint64_t u = (total_kcps * 10000ULL) / max_kcps;

		if (u == 0 && (c->ll_tick_count > 0 || c->dp_run_count > 0))
			u = 1;
		if (u > 10000ULL)
			u = 10000ULL;
		c->total_util_x100 = (uint32_t)u;
	}
}

void host_telem_core_ll_tick(int core, uint32_t tasks_run, uint64_t cycles)
{
	struct host_core_telem *c;
	uint32_t freq_hz;
	uint64_t exec_ns;

	if (!g_sched_telem.enabled || core < 0 || core >= TB_TELEM_MAX_CORES)
		return;
	if (tasks_run == 0)
		return;

	TELEM_LOCK();
	c = &g_sched_telem.cores[core];
	if (g_sched_telem.sim_cfg.mode != HOST_WORKLOAD_SIM_OFF && c->_sim_ll_tick_cycles > 0) {
		cycles = c->_sim_ll_tick_cycles;
		c->_sim_ll_tick_cycles = 0;
	}

	if (cycles == 0)
		cycles = 1;

	freq_hz = get_core_freq_hz((uint32_t)core);
	exec_ns = cycles_to_ns(cycles, freq_hz);

	c->active = true;
	c->freq_hz = freq_hz;
	c->ll_budget_cycles = freq_hz / 1000U;
	c->ll_tick_count++;
	c->ll_active_tasks = tasks_run;
	c->ll_last_cycles = cycles;
	c->ll_total_cycles += cycles;
	c->ll_avg_cycles = c->ll_total_cycles / c->ll_tick_count;
	if (cycles > c->ll_peak_cycles)
		c->ll_peak_cycles = cycles;

	c->ll_last_exec_ns = exec_ns;
	c->ll_total_exec_ns += exec_ns;
	c->ll_avg_exec_ns = c->ll_total_exec_ns / c->ll_tick_count;
	if (exec_ns > c->ll_peak_exec_ns)
		c->ll_peak_exec_ns = exec_ns;

	if (c->ll_budget_cycles > 0 && cycles > c->ll_budget_cycles)
		c->ll_deadline_violations++;

	/* Assume 1 ms nominal LL tick period -> 1000 ticks/sec -> KCPS = cycles */
	c->ll_avg_kcps = (uint32_t)c->ll_avg_cycles;
	c->ll_peak_kcps = (uint32_t)c->ll_peak_cycles;
	update_core_util(core, c);
	TELEM_UNLOCK();
}

void host_telem_core_dp_run(int core, uint32_t active_dp_threads, uint64_t cycles)
{
	struct host_core_telem *c;
	uint32_t freq_hz, i, dp_avg_k = 0, dp_peak_k = 0;
	uint64_t exec_ns;

	if (!g_sched_telem.enabled || core < 0 || core >= TB_TELEM_MAX_CORES)
		return;

	TELEM_LOCK();
	c = &g_sched_telem.cores[core];
	if (g_sched_telem.sim_cfg.mode != HOST_WORKLOAD_SIM_OFF && c->_sim_dp_run_cycles > 0) {
		cycles = c->_sim_dp_run_cycles;
		c->_sim_dp_run_cycles = 0;
	}

	if (cycles == 0)
		cycles = 1;

	freq_hz = get_core_freq_hz((uint32_t)core);
	exec_ns = cycles_to_ns(cycles, freq_hz);

	c->active = true;
	c->freq_hz = freq_hz;
	c->ll_budget_cycles = freq_hz / 1000U;
	c->dp_run_count++;
	if (active_dp_threads > 0)
		c->dp_active_threads = active_dp_threads;
	c->dp_last_cycles = cycles;
	c->dp_total_cycles += cycles;
	c->dp_avg_cycles = c->dp_total_cycles / c->dp_run_count;
	if (cycles > c->dp_peak_cycles)
		c->dp_peak_cycles = cycles;

	c->dp_last_exec_ns = exec_ns;
	c->dp_total_exec_ns += exec_ns;
	c->dp_avg_exec_ns = c->dp_total_exec_ns / c->dp_run_count;
	if (exec_ns > c->dp_peak_exec_ns)
		c->dp_peak_exec_ns = exec_ns;

	/* Sum amortized KCPS across active DP modules on this core */
	for (i = 0; i < g_sched_telem.num_mods; i++) {
		if (g_sched_telem.mods[i].active &&
		    g_sched_telem.mods[i].core_id == (uint32_t)core &&
		    g_sched_telem.mods[i].proc_domain == COMP_PROCESSING_DOMAIN_DP) {
			dp_avg_k += g_sched_telem.mods[i].avg_kcps;
			dp_peak_k += g_sched_telem.mods[i].peak_kcps;
		}
	}
	c->dp_avg_kcps = dp_avg_k ? dp_avg_k : (uint32_t)c->dp_avg_cycles;
	c->dp_peak_kcps = dp_peak_k ? dp_peak_k : (uint32_t)c->dp_peak_cycles;
	update_core_util(core, c);
	TELEM_UNLOCK();
}

static struct host_pipe_telem *find_or_alloc_pipe(uint32_t pipeline_id)
{
	uint32_t i;
	struct host_pipe_telem *p = NULL;

	for (i = 0; i < g_sched_telem.num_pipes; i++) {
		if (g_sched_telem.pipes[i].active &&
		    g_sched_telem.pipes[i].pipeline_id == pipeline_id)
			return &g_sched_telem.pipes[i];
	}

	TELEM_LOCK();
	for (i = 0; i < g_sched_telem.num_pipes; i++) {
		if (g_sched_telem.pipes[i].active &&
		    g_sched_telem.pipes[i].pipeline_id == pipeline_id) {
			p = &g_sched_telem.pipes[i];
			break;
		}
	}
	if (!p && g_sched_telem.num_pipes < TB_TELEM_MAX_PIPES) {
		p = &g_sched_telem.pipes[g_sched_telem.num_pipes++];
		memset(p, 0, sizeof(*p));
		p->pipeline_id = pipeline_id;
		p->active = true;
	}
	TELEM_UNLOCK();

	return p;
}

static struct host_mod_telem *find_or_alloc_mod(uint32_t comp_id)
{
	uint32_t i;
	struct host_mod_telem *m = NULL;

	for (i = 0; i < g_sched_telem.num_mods; i++) {
		if (g_sched_telem.mods[i].active &&
		    g_sched_telem.mods[i].comp_id == comp_id)
			return &g_sched_telem.mods[i];
	}

	TELEM_LOCK();
	for (i = 0; i < g_sched_telem.num_mods; i++) {
		if (g_sched_telem.mods[i].active &&
		    g_sched_telem.mods[i].comp_id == comp_id) {
			m = &g_sched_telem.mods[i];
			break;
		}
	}
	if (!m && g_sched_telem.num_mods < TB_TELEM_MAX_MODS) {
		m = &g_sched_telem.mods[g_sched_telem.num_mods++];
		memset(m, 0, sizeof(*m));
		m->comp_id = comp_id;
		m->active = true;
	}
	TELEM_UNLOCK();

	return m;
}

static uint32_t default_cpc_for_comp_type(uint32_t comp_type, uint32_t proc_domain,
					  uint32_t period_us, uint32_t ibs, uint32_t obs)
{
	uint32_t base_cpc;
	uint32_t chunk_bytes = ibs ? ibs : obs;

	switch (comp_type) {
	case SOF_COMP_HOST:
	case SOF_COMP_DAI:
	case SOF_COMP_SG_HOST:
	case SOF_COMP_SG_DAI:
	case SOF_COMP_FILEREAD:
	case SOF_COMP_FILEWRITE:
		base_cpc = 1647;
		break;
	case SOF_COMP_VOLUME:
		base_cpc = 10183;
		break;
	case SOF_COMP_MIXER:
		base_cpc = 2950;
		break;
	case SOF_COMP_MUX:
	case SOF_COMP_DEMUX:
	case SOF_COMP_SELECTOR:
		base_cpc = 2900;
		break;
	case SOF_COMP_SRC:
		base_cpc = 24000;
		break;
	case SOF_COMP_ASRC:
		base_cpc = 32000;
		break;
	case SOF_COMP_TONE:
		base_cpc = 4200;
		break;
	case SOF_COMP_BUFFER:
		base_cpc = 400;
		break;
	case SOF_COMP_EQ_IIR:
		base_cpc = 18500;
		break;
	case SOF_COMP_EQ_FIR:
		base_cpc = 26000;
		break;
	case SOF_COMP_KEYWORD_DETECT:
		base_cpc = 45000;
		break;
	case SOF_COMP_KPB:
		base_cpc = 3500;
		break;
	case SOF_COMP_DCBLOCK:
		base_cpc = 6400;
		break;
	case SOF_COMP_SMART_AMP:
		base_cpc = 38000;
		break;
	default:
		base_cpc = (proc_domain == COMP_PROCESSING_DOMAIN_DP) ? 48000 : 12000;
		break;
	}

	if (proc_domain == COMP_PROCESSING_DOMAIN_DP && period_us > 1000)
		base_cpc = base_cpc * (period_us / 1000U);
	else if (chunk_bytes > 384U)
		base_cpc = (uint32_t)(((uint64_t)base_cpc * chunk_bytes) / 384ULL);

	return base_cpc;
}

static void resolve_mod_sim_params(struct host_mod_telem *m)
{
	uint32_t i;
	uint32_t cpc_ov = 0;
	uint32_t mem_ov = HOST_MEM_DOMAIN_AUTO;

	ensure_sim_cfg_defaults();

	for (i = 0; i < g_sched_telem.sim_cfg.num_overrides; i++) {
		const struct host_workload_mod_override *ov =
			&g_sched_telem.sim_cfg.overrides[i];

		if (!ov->active)
			continue;
		if (ov->comp_id == m->comp_id ||
		    ((ov->comp_id & 0xFFFF0000U) == 0xFFFF0000U &&
		     (ov->comp_id & 0xFFFFU) == (m->comp_id & 0xFFFFU))) {
			if (ov->cpc_override > 0)
				cpc_ov = ov->cpc_override;
			if (ov->mem_domain_override != HOST_MEM_DOMAIN_AUTO)
				mem_ov = ov->mem_domain_override;
			break;
		}
	}

	if (mem_ov != HOST_MEM_DOMAIN_AUTO)
		m->mem_domain = (mem_ov == HOST_MEM_DOMAIN_DRAM) ?
				HOST_MEM_DOMAIN_DRAM : HOST_MEM_DOMAIN_SRAM;
	else
		m->mem_domain = g_sched_telem.sim_cfg.default_mem_domain;

	if (cpc_ov > 0) {
		m->effective_cpc = cpc_ov;
	} else if (m->cpc > 0) {
		m->effective_cpc = m->cpc;
	} else {
		m->effective_cpc = default_cpc_for_comp_type(m->type, m->proc_domain,
							     m->period_us, m->ibs, m->obs);
	}
	if (m->cpc == 0)
		m->cpc = m->effective_cpc;
}

static uint64_t compute_simulated_mod_cycles(const struct host_mod_telem *m,
					     uint32_t bytes_in, uint32_t bytes_out)
{
	uint64_t base = m->effective_cpc ? m->effective_cpc : 2000ULL;
	uint64_t cycles;

	/* If module has connected pins but transferred 0 bytes on this tick, charge idle check */
	if (bytes_in == 0 && bytes_out == 0 && (m->num_in_pins > 0 || m->num_out_pins > 0))
		base = 64ULL;

	if (m->mem_domain == HOST_MEM_DOMAIN_DRAM) {
		uint32_t factor = g_sched_telem.sim_cfg.dram_factor_x100 ?
				  g_sched_telem.sim_cfg.dram_factor_x100 : 160U;
		uint32_t c_per_word = g_sched_telem.sim_cfg.dram_cycles_per_word;
		uint64_t xfer_words = (uint64_t)(bytes_in + bytes_out) / 4ULL;

		cycles = (base * (uint64_t)factor) / 100ULL + xfer_words * (uint64_t)c_per_word;
	} else {
		uint32_t factor = g_sched_telem.sim_cfg.sram_factor_x100 ?
				  g_sched_telem.sim_cfg.sram_factor_x100 : 100U;

		cycles = (base * (uint64_t)factor) / 100ULL;
	}

	return cycles ? cycles : 1ULL;
}

uint64_t host_telem_pipe_begin(struct pipeline *p)
{
	struct host_pipe_telem *pt;
	uint32_t core;

	if (!g_sched_telem.enabled || !p)
		return 0;

	pt = find_or_alloc_pipe(p->pipeline_id);
	if (pt) {
		uint64_t overhead;

		TELEM_LOCK();
		/* Base LL pipeline dispatch overhead (120 cycles in SRAM, scaled in DRAM) */
		uint32_t factor = (g_sched_telem.sim_cfg.default_mem_domain ==
				   HOST_MEM_DOMAIN_DRAM) ?
				  g_sched_telem.sim_cfg.dram_factor_x100 :
				  g_sched_telem.sim_cfg.sram_factor_x100;
		if (!factor)
			factor = 100U;
		overhead = (120ULL * factor) / 100ULL;
		pt->_sim_tick_cycles = overhead;
		core = (p->core < TB_TELEM_MAX_CORES) ? p->core : 0;
		g_sched_telem.cores[core]._sim_ll_tick_cycles += overhead;
		g_sched_telem.cores[core]._tick_ll_offset_cycles += overhead;
		TELEM_UNLOCK();
	}

	return host_telem_timestamp_cycles();
}

void host_telem_pipe_end(struct pipeline *p, uint64_t start_cycles, int ret)
{
	struct host_pipe_telem *pt;
	uint64_t end_cycles, delta, exec_ns;
	uint32_t period_us, freq_hz, i, ll_cnt = 0, dp_cnt = 0;

	if (!g_sched_telem.enabled || !p)
		return;

	end_cycles = host_telem_timestamp_cycles();
	delta = (end_cycles > start_cycles) ? (end_cycles - start_cycles) : 1;

	pt = find_or_alloc_pipe(p->pipeline_id);
	if (!pt)
		return;

	TELEM_LOCK();
	if (g_sched_telem.sim_cfg.mode != HOST_WORKLOAD_SIM_OFF && pt->_sim_tick_cycles > 0) {
		delta = pt->_sim_tick_cycles;
		pt->_sim_tick_cycles = 0;
	}

	pt->comp_id = p->comp_id;
	pt->core_id = p->core;
	pt->priority = p->priority;
	pt->period_us = p->period ? p->period : 1000;
	pt->status = p->status;
	pt->mem_domain = g_sched_telem.sim_cfg.default_mem_domain;
	if (p->sched_comp)
		pt->direction = p->sched_comp->direction;

	freq_hz = get_core_freq_hz(pt->core_id);
	exec_ns = cycles_to_ns(delta, freq_hz);

	pt->copy_iterations++;
	pt->last_cycles = delta;
	pt->total_cycles += delta;
	pt->avg_cycles = pt->total_cycles / pt->copy_iterations;
	if (delta > pt->peak_cycles)
		pt->peak_cycles = delta;

	pt->last_exec_ns = exec_ns;
	pt->total_exec_ns += exec_ns;
	pt->avg_exec_ns = pt->total_exec_ns / pt->copy_iterations;
	if (exec_ns > pt->peak_exec_ns)
		pt->peak_exec_ns = exec_ns;

	period_us = pt->period_us ? pt->period_us : 1000;
	pt->avg_kcps = (uint32_t)((pt->avg_cycles * 1000ULL) / period_us);
	pt->peak_kcps = (uint32_t)((pt->peak_cycles * 1000ULL) / period_us);

	if (ret < 0 || p->xrun_bytes != 0)
		pt->xrun_count++;

	for (i = 0; i < g_sched_telem.num_mods; i++) {
		if (g_sched_telem.mods[i].active &&
		    g_sched_telem.mods[i].pipeline_id == p->pipeline_id) {
			if (g_sched_telem.mods[i].proc_domain == COMP_PROCESSING_DOMAIN_DP)
				dp_cnt++;
			else
				ll_cnt++;
		}
	}
	pt->num_ll_modules = ll_cnt;
	pt->num_dp_modules = dp_cnt;
	TELEM_UNLOCK();
}

uint64_t host_telem_mod_entry(struct comp_dev *dev, bool is_dp_thread)
{
	struct host_mod_telem *m;
	struct comp_buffer *buffer;
	uint32_t idx;

	if (!g_sched_telem.enabled || !dev)
		return 0;

	m = find_or_alloc_mod(dev->ipc_config.id);
	if (!m)
		return host_telem_timestamp_cycles();

	m->pipeline_id = dev->ipc_config.pipeline_id;
	m->core_id = dev->ipc_config.core;
	m->proc_domain = dev->ipc_config.proc_domain;
	m->type = dev->ipc_config.type;
	m->period_us = dev->period ? dev->period :
		       (dev->pipeline && dev->pipeline->period ? dev->pipeline->period : 1000);
	m->ibs = (uint32_t)dev->ibs;
	m->obs = (uint32_t)dev->obs;
	m->cpc = (uint32_t)dev->cpc;

#if CONFIG_IPC_MAJOR_4
	if (dev->mod) {
		if (!m->ibs)
			m->ibs = dev->mod->priv.cfg.base_cfg.ibs;
		if (!m->obs)
			m->obs = dev->mod->priv.cfg.base_cfg.obs;
		if (!m->cpc)
			m->cpc = dev->mod->priv.cfg.base_cfg.cpc;
	}
#endif

	if (!is_dp_thread) {
		m->_ll_entry_wall_ns = host_telem_wall_ns();
		/* LL tick path (comp_copy) */
		idx = 0;
		comp_dev_for_each_producer(dev, buffer) {
			struct host_mod_pin_telem *pin;
			struct comp_dev *prod;
			uintptr_t base, rptr;
			uint32_t avail_b, free_b;

			if (idx >= TB_TELEM_MAX_PINS)
				break;
			pin = &m->in_pins[idx++];
			pin->active = true;
			pin->buf_id = buf_get_id(buffer);
			prod = comp_buffer_get_source_component(buffer);
			pin->peer_comp_id = prod ? prod->ipc_config.id : 0xFFFFFFFFU;
			pin->buf_size = (uint32_t)audio_stream_get_size(&buffer->stream);
			pin->rate = audio_stream_get_rate(&buffer->stream);
			pin->channels = audio_stream_get_channels(&buffer->stream);
			pin->frame_fmt = audio_stream_get_frm_fmt(&buffer->stream);

			avail_b = (uint32_t)audio_stream_get_avail_bytes(&buffer->stream);
			free_b = (uint32_t)audio_stream_get_free_bytes(&buffer->stream);
			pin->_entry_avail = avail_b;

			base = (uintptr_t)audio_stream_get_addr(&buffer->stream);
			rptr = (uintptr_t)audio_stream_get_rptr(&buffer->stream);
			pin->_entry_ptr = rptr;

			if (avail_b > 0 || pin->total_bytes_transferred == 0) {
				pin->entry_avail_bytes = avail_b;
				pin->entry_free_bytes = free_b;
				if (dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_LL &&
				    pin->buf_size > 0 && rptr >= base) {
					capture_circular_samples((const void *)base, pin->buf_size,
								 (size_t)(rptr - base),
								 avail_b,
								 pin->frame_fmt,
								 pin->entry_samples,
								 &pin->entry_num_samples,
								 &pin->entry_rms_dbfs_x100,
								 &pin->entry_peak_dbfs_x100);
				}
			}

#if CONFIG_PIPELINE_2_0
			if (buffer->audio_buffer.secondary_buffer_source) {
				struct sof_audio_buffer *sec =
					buffer->audio_buffer.secondary_buffer_source;
				uint32_t avail = (uint32_t)source_get_data_available(
					audio_buffer_get_source(sec));
				uint32_t free_b = (uint32_t)sink_get_free_size(
					audio_buffer_get_sink(sec));
				pin->dp_ring_entry_avail = avail;
				pin->dp_ring_entry_free = free_b;
				pin->dp_ring_size = avail + free_b;
			}
#endif
		}
		if (idx > m->num_in_pins)
			m->num_in_pins = idx;

		idx = 0;
		comp_dev_for_each_consumer(dev, buffer) {
			struct host_mod_pin_telem *pin;
			struct comp_dev *cons;
			uintptr_t base, rptr, wptr;
			uint32_t avail_b, free_b;

			if (idx >= TB_TELEM_MAX_PINS)
				break;
			pin = &m->out_pins[idx++];
			pin->active = true;
			pin->buf_id = buf_get_id(buffer);
			cons = comp_buffer_get_sink_component(buffer);
			pin->peer_comp_id = cons ? cons->ipc_config.id : 0xFFFFFFFFU;
			pin->buf_size = (uint32_t)audio_stream_get_size(&buffer->stream);
			pin->rate = audio_stream_get_rate(&buffer->stream);
			pin->channels = audio_stream_get_channels(&buffer->stream);
			pin->frame_fmt = audio_stream_get_frm_fmt(&buffer->stream);

			avail_b = (uint32_t)audio_stream_get_avail_bytes(&buffer->stream);
			free_b = (uint32_t)audio_stream_get_free_bytes(&buffer->stream);
			pin->_entry_avail = avail_b;

			base = (uintptr_t)audio_stream_get_addr(&buffer->stream);
			rptr = (uintptr_t)audio_stream_get_rptr(&buffer->stream);
			wptr = (uintptr_t)audio_stream_get_wptr(&buffer->stream);
			pin->_entry_ptr = wptr;

			if (avail_b > 0 || pin->total_bytes_transferred == 0) {
				pin->entry_avail_bytes = avail_b;
				pin->entry_free_bytes = free_b;
				if (dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_LL &&
				    pin->buf_size > 0 && rptr >= base) {
					capture_circular_samples((const void *)base, pin->buf_size,
								 (size_t)(rptr - base),
								 avail_b,
								 pin->frame_fmt,
								 pin->entry_samples,
								 &pin->entry_num_samples,
								 &pin->entry_rms_dbfs_x100,
								 &pin->entry_peak_dbfs_x100);
				}
			}

#if CONFIG_PIPELINE_2_0
			if (buffer->audio_buffer.secondary_buffer_sink) {
				struct sof_audio_buffer *sec =
					buffer->audio_buffer.secondary_buffer_sink;
				uint32_t avail = (uint32_t)source_get_data_available(
					audio_buffer_get_source(sec));
				uint32_t free_b = (uint32_t)sink_get_free_size(
					audio_buffer_get_sink(sec));
				pin->dp_ring_entry_avail = avail;
				pin->dp_ring_entry_free = free_b;
				pin->dp_ring_size = avail + free_b;
			}
#endif
		}
		if (idx > m->num_out_pins)
			m->num_out_pins = idx;
	} else if (dev->mod) {
		/* DP worker thread path (dp_task_run) */
		struct processing_module *mod = dev->mod;

		m->_dp_entry_wall_ns = host_telem_wall_ns();

		for (idx = 0; idx < mod->num_of_sources && idx < TB_TELEM_MAX_PINS; idx++) {
			struct sof_source *src = mod->sources[idx];
			struct sof_audio_buffer *ab;
			struct ring_buffer *rb;
			struct host_mod_pin_telem *pin = &m->in_pins[idx];
			size_t roff;

			if (!src)
				continue;
			ab = sof_audio_buffer_from_source(src);
			rb = container_of(ab, struct ring_buffer, audio_buffer);
			pin->active = true;
			pin->rate = source_get_rate(src);
			pin->channels = source_get_channels(src);
			pin->frame_fmt = source_get_frm_fmt(src);
			pin->dp_ring_size = (uint32_t)rb->data_buffer_size;
			pin->dp_ring_entry_avail = (uint32_t)source_get_data_available(src);
			pin->dp_ring_entry_free =
				(pin->dp_ring_size >= pin->dp_ring_entry_avail) ?
				(pin->dp_ring_size - pin->dp_ring_entry_avail) : 0;

			roff = (rb->data_buffer_size > 0) ?
			       (rb->_read_offset % rb->data_buffer_size) : 0;
			capture_circular_samples((__sparse_force const void *)rb->_data_buffer,
						 rb->data_buffer_size, roff,
						 pin->dp_ring_entry_avail,
						 pin->frame_fmt,
						 pin->entry_samples,
						 &pin->entry_num_samples,
						 &pin->entry_rms_dbfs_x100,
						 &pin->entry_peak_dbfs_x100);
		}
		if (idx > m->num_in_pins)
			m->num_in_pins = idx;

		for (idx = 0; idx < mod->num_of_sinks && idx < TB_TELEM_MAX_PINS; idx++) {
			struct sof_sink *snk = mod->sinks[idx];
			struct sof_audio_buffer *ab;
			struct ring_buffer *rb;
			struct host_mod_pin_telem *pin = &m->out_pins[idx];
			size_t roff, woff;

			if (!snk)
				continue;
			ab = sof_audio_buffer_from_sink(snk);
			rb = container_of(ab, struct ring_buffer, audio_buffer);
			pin->active = true;
			pin->rate = sink_get_rate(snk);
			pin->channels = sink_get_channels(snk);
			pin->frame_fmt = sink_get_frm_fmt(snk);
			pin->dp_ring_size = (uint32_t)rb->data_buffer_size;
			pin->dp_ring_entry_free = (uint32_t)sink_get_free_size(snk);
			pin->dp_ring_entry_avail =
				(pin->dp_ring_size >= pin->dp_ring_entry_free) ?
				(pin->dp_ring_size - pin->dp_ring_entry_free) : 0;

			woff = (rb->data_buffer_size > 0) ?
			       (rb->_write_offset % rb->data_buffer_size) : 0;
			roff = (rb->data_buffer_size > 0) ?
			       (rb->_read_offset % rb->data_buffer_size) : 0;
			pin->_dp_entry_woff = (uint32_t)woff;

			capture_circular_samples((__sparse_force const void *)rb->_data_buffer,
						 rb->data_buffer_size, roff,
						 pin->dp_ring_entry_avail,
						 pin->frame_fmt,
						 pin->entry_samples,
						 &pin->entry_num_samples,
						 &pin->entry_rms_dbfs_x100,
						 &pin->entry_peak_dbfs_x100);
		}
		if (idx > m->num_out_pins)
			m->num_out_pins = idx;
	}

	return host_telem_timestamp_cycles();
}

void host_telem_mod_exit(struct comp_dev *dev, bool is_dp_thread, uint64_t start_cycles, int ret)
{
	struct host_mod_telem *m;
	struct host_pipe_telem *pt;
	struct comp_buffer *buffer;
	uint64_t end_cycles, delta, exec_ns;
	uint32_t idx, period_us, freq_hz, core_id;
	uint32_t total_in_bytes = 0, total_out_bytes = 0;

	if (!g_sched_telem.enabled || !dev)
		return;

	end_cycles = host_telem_timestamp_cycles();
	delta = (end_cycles > start_cycles) ? (end_cycles - start_cycles) : 1;

	m = find_or_alloc_mod(dev->ipc_config.id);
	if (!m)
		return;

	resolve_mod_sim_params(m);
	core_id = (m->core_id < TB_TELEM_MAX_CORES) ? m->core_id : 0;

	if (ret < 0)
		m->error_count++;

	if (!is_dp_thread && dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP) {
		uint32_t sync_iter;

		/* DP module during LL tick: only synced secondary ring buffers */
		pt = find_or_alloc_pipe(m->pipeline_id);
		TELEM_LOCK();
		if (g_sched_telem.sim_cfg.mode != HOST_WORKLOAD_SIM_OFF) {
			uint32_t factor = (m->mem_domain == HOST_MEM_DOMAIN_DRAM) ?
					  g_sched_telem.sim_cfg.dram_factor_x100 :
					  g_sched_telem.sim_cfg.sram_factor_x100;
			if (!factor)
				factor = 100U;
			delta = (160ULL * factor) / 100ULL;
			if (pt)
				pt->_sim_tick_cycles += delta;
			g_sched_telem.cores[core_id]._sim_ll_tick_cycles += delta;
		}
		m->ll_ring_sync_iterations++;
		sync_iter = (uint32_t)m->ll_ring_sync_iterations;
		m->ll_ring_sync_last_cycles = delta;
		m->ll_ring_sync_total_cycles += delta;
		TELEM_UNLOCK();

		idx = 0;
		comp_dev_for_each_producer(dev, buffer) {
			if (idx >= TB_TELEM_MAX_PINS)
				break;
			m->in_pins[idx].exit_avail_bytes =
				(uint32_t)audio_stream_get_avail_bytes(&buffer->stream);
			m->in_pins[idx].exit_free_bytes =
				(uint32_t)audio_stream_get_free_bytes(&buffer->stream);
			idx++;
		}

		idx = 0;
		comp_dev_for_each_consumer(dev, buffer) {
			if (idx >= TB_TELEM_MAX_PINS)
				break;
			m->out_pins[idx].exit_avail_bytes =
				(uint32_t)audio_stream_get_avail_bytes(&buffer->stream);
			m->out_pins[idx].exit_free_bytes =
				(uint32_t)audio_stream_get_free_bytes(&buffer->stream);
			idx++;
		}

		freq_hz = get_core_freq_hz(core_id);
		record_mod_sched_event(m, core_id, freq_hz, HOST_SCHED_EVT_DP_SYNC,
				       sync_iter, delta, 0,
				       0, 0, m->_ll_entry_wall_ns,
				       host_telem_wall_ns(), ret);
		return;
	}

	if (!is_dp_thread) {
		/* LL module exit: inspect comp_buffer pointers and capture output samples */
		idx = 0;
		comp_dev_for_each_producer(dev, buffer) {
			struct host_mod_pin_telem *pin;
			uintptr_t base, exit_r;
			uint32_t sz, exit_avail, exit_free, consumed = 0;

			if (idx >= TB_TELEM_MAX_PINS)
				break;
			pin = &m->in_pins[idx++];
			exit_avail = (uint32_t)audio_stream_get_avail_bytes(&buffer->stream);
			exit_free = (uint32_t)audio_stream_get_free_bytes(&buffer->stream);

			base = (uintptr_t)audio_stream_get_addr(&buffer->stream);
			exit_r = (uintptr_t)audio_stream_get_rptr(&buffer->stream);
			sz = pin->buf_size;

			if (sz > 0 && pin->_entry_ptr >= base && exit_r >= base) {
				consumed = (uint32_t)((exit_r + sz - pin->_entry_ptr) % sz);
				if (consumed == 0 && pin->_entry_avail > exit_avail)
					consumed = pin->_entry_avail - exit_avail;
			}
			total_in_bytes += consumed;

			if (consumed > 0 || pin->total_bytes_transferred == 0) {
				pin->exit_avail_bytes = exit_avail;
				pin->exit_free_bytes = exit_free;
				pin->last_bytes_transferred = consumed;
				pin->total_bytes_transferred += consumed;

				if (sz > 0 && exit_r >= base) {
					size_t tap_off = (exit_avail > 0) ?
							 (size_t)(exit_r - base) :
							 (size_t)(pin->_entry_ptr - base);
					size_t tap_bytes = (exit_avail > 0) ?
							   exit_avail : consumed;
					capture_circular_samples((const void *)base, sz,
								 tap_off, tap_bytes,
								 pin->frame_fmt,
								 pin->exit_samples,
								 &pin->exit_num_samples,
								 &pin->exit_rms_dbfs_x100,
								 &pin->exit_peak_dbfs_x100);
				}
			}
		}

		idx = 0;
		comp_dev_for_each_consumer(dev, buffer) {
			struct host_mod_pin_telem *pin;
			uintptr_t base, exit_w, rptr;
			uint32_t sz, exit_avail, exit_free, produced = 0;

			if (idx >= TB_TELEM_MAX_PINS)
				break;
			pin = &m->out_pins[idx++];
			exit_avail = (uint32_t)audio_stream_get_avail_bytes(&buffer->stream);
			exit_free = (uint32_t)audio_stream_get_free_bytes(&buffer->stream);

			base = (uintptr_t)audio_stream_get_addr(&buffer->stream);
			exit_w = (uintptr_t)audio_stream_get_wptr(&buffer->stream);
			rptr = (uintptr_t)audio_stream_get_rptr(&buffer->stream);
			sz = pin->buf_size;

			if (sz > 0 && pin->_entry_ptr >= base && exit_w >= base) {
				produced = (uint32_t)((exit_w + sz - pin->_entry_ptr) % sz);
				if (produced == 0 && exit_avail > pin->_entry_avail)
					produced = exit_avail - pin->_entry_avail;
			}
			total_out_bytes += produced;

			if (produced > 0 || pin->total_bytes_transferred == 0) {
				pin->entry_avail_bytes = pin->_entry_avail;
				pin->entry_free_bytes = (sz >= pin->_entry_avail) ?
							(sz - pin->_entry_avail) : 0;
				pin->exit_avail_bytes = exit_avail;
				pin->exit_free_bytes = exit_free;
				pin->last_bytes_transferred = produced;
				pin->total_bytes_transferred += produced;

				if (sz > 0 && pin->_entry_ptr >= base) {
					size_t tap_off = (produced > 0) ?
							 (size_t)(pin->_entry_ptr - base) :
							 (size_t)(rptr - base);
					size_t tap_bytes = (produced > 0) ?
							   produced : exit_avail;
					capture_circular_samples((const void *)base, sz,
								 tap_off, tap_bytes,
								 pin->frame_fmt,
								 pin->exit_samples,
								 &pin->exit_num_samples,
								 &pin->exit_rms_dbfs_x100,
								 &pin->exit_peak_dbfs_x100);
				}
			}
		}
	} else if (dev->mod) {
		/* DP worker thread exit: inspect ring_buffer processed bytes and output samples */
		struct processing_module *mod = dev->mod;

		for (idx = 0; idx < mod->num_of_sources && idx < TB_TELEM_MAX_PINS; idx++) {
			struct sof_source *src = mod->sources[idx];
			struct sof_audio_buffer *ab;
			struct ring_buffer *rb;
			struct host_mod_pin_telem *pin = &m->in_pins[idx];
			uint32_t consumed;
			size_t roff;

			if (!src)
				continue;
			ab = sof_audio_buffer_from_source(src);
			rb = container_of(ab, struct ring_buffer, audio_buffer);
			consumed = (uint32_t)source_get_num_of_processed_bytes(src);
			total_in_bytes += consumed;
			pin->last_bytes_transferred = consumed;
			pin->total_bytes_transferred += consumed;
			pin->dp_ring_exit_avail = (uint32_t)source_get_data_available(src);
			pin->dp_ring_exit_free =
				(pin->dp_ring_size >= pin->dp_ring_exit_avail) ?
				(pin->dp_ring_size - pin->dp_ring_exit_avail) : 0;

			roff = (rb->data_buffer_size > 0) ?
			       (rb->_read_offset % rb->data_buffer_size) : 0;
			capture_circular_samples((__sparse_force const void *)rb->_data_buffer,
						 rb->data_buffer_size, roff,
						 pin->dp_ring_exit_avail ?
						 pin->dp_ring_exit_avail : consumed,
						 pin->frame_fmt,
						 pin->exit_samples,
						 &pin->exit_num_samples,
						 &pin->exit_rms_dbfs_x100,
						 &pin->exit_peak_dbfs_x100);
		}

		for (idx = 0; idx < mod->num_of_sinks && idx < TB_TELEM_MAX_PINS; idx++) {
			struct sof_sink *snk = mod->sinks[idx];
			struct sof_audio_buffer *ab;
			struct ring_buffer *rb;
			struct host_mod_pin_telem *pin = &m->out_pins[idx];
			uint32_t produced;

			if (!snk)
				continue;
			ab = sof_audio_buffer_from_sink(snk);
			rb = container_of(ab, struct ring_buffer, audio_buffer);
			produced = (uint32_t)sink_get_num_of_processed_bytes(snk);
			total_out_bytes += produced;
			pin->last_bytes_transferred = produced;
			pin->total_bytes_transferred += produced;
			pin->dp_ring_exit_free = (uint32_t)sink_get_free_size(snk);
			pin->dp_ring_exit_avail =
				(pin->dp_ring_size >= pin->dp_ring_exit_free) ?
				(pin->dp_ring_size - pin->dp_ring_exit_free) : 0;

			capture_circular_samples((__sparse_force const void *)rb->_data_buffer,
						 rb->data_buffer_size,
						 pin->_dp_entry_woff,
						 produced ? produced : pin->dp_ring_exit_avail,
						 pin->frame_fmt,
						 pin->exit_samples,
						 &pin->exit_num_samples,
						 &pin->exit_rms_dbfs_x100,
						 &pin->exit_peak_dbfs_x100);
		}
	}

	/* Apply accurate workload execution simulation when enabled */
	pt = !is_dp_thread ? find_or_alloc_pipe(m->pipeline_id) : NULL;
	if (g_sched_telem.sim_cfg.mode != HOST_WORKLOAD_SIM_OFF)
		delta = compute_simulated_mod_cycles(m, total_in_bytes, total_out_bytes);

	freq_hz = get_core_freq_hz(core_id);
	exec_ns = cycles_to_ns(delta, freq_hz);

	if (g_sched_telem.sim_cfg.mode == HOST_WORKLOAD_SIM_REALTIME)
		pace_execution_ns(exec_ns);

	TELEM_LOCK();
	if (g_sched_telem.sim_cfg.mode != HOST_WORKLOAD_SIM_OFF) {
		if (!is_dp_thread) {
			if (pt)
				pt->_sim_tick_cycles += delta;
			g_sched_telem.cores[core_id]._sim_ll_tick_cycles += delta;
		} else {
			g_sched_telem.cores[core_id]._sim_dp_run_cycles += delta;
		}
	}

	/* Record module processing execution cycles & simulated execution time */
	m->iterations++;
	uint32_t mod_iter_nr = (uint32_t)m->iterations;

	m->last_cycles = delta;
	m->total_cycles += delta;
	m->avg_cycles = m->total_cycles / m->iterations;
	if (delta > m->peak_cycles)
		m->peak_cycles = delta;

	m->last_exec_ns = exec_ns;
	m->total_exec_ns += exec_ns;
	m->avg_exec_ns = m->total_exec_ns / m->iterations;
	if (exec_ns > m->peak_exec_ns)
		m->peak_exec_ns = exec_ns;

	period_us = m->period_us ? m->period_us : 1000;
	m->avg_kcps = (uint32_t)((m->avg_cycles * 1000ULL) / period_us);
	m->peak_kcps = (uint32_t)((m->peak_cycles * 1000ULL) / period_us);

	if (m->effective_cpc > 0 && delta > m->effective_cpc)
		m->cpc_violations++;
	TELEM_UNLOCK();

	record_mod_sched_event(m, core_id, freq_hz,
			       is_dp_thread ? HOST_SCHED_EVT_DP_RUN : HOST_SCHED_EVT_LL_COPY,
			       mod_iter_nr, delta, m->effective_cpc,
			       total_in_bytes, total_out_bytes,
			       is_dp_thread ? m->_dp_entry_wall_ns : m->_ll_entry_wall_ns,
			       host_telem_wall_ns(), ret);
}
