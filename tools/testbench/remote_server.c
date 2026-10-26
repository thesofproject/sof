// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.
//
// Network Server Interface (sof-remote TCP protocol) for SOF Testbench

#if !defined(__XTENSA__) && !defined(CONFIG_XT_SIMULATOR)

#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <math.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <sof/audio/buffer.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/driver.h>
#include <sof/lib/cpu.h>
#include <sof/list.h>
#include <platform/lib/ll_schedule.h>
#include <platform/lib/telemetry_sched.h>
#include <ipc4/header.h>
#include <ipc4/module.h>
#include <ipc4/pipeline.h>
#include <sof_versions.h>

#include "testbench/sof_remote_proto.h"
#include "testbench/remote_server.h"
#include "testbench/topology_ipc4.h"
#include "testbench/trace.h"
#include "testbench/file.h"
#include "testbench/file_ipc4.h"
#include "testbench/utils.h"
#include "../../src/audio/copier/copier.h"

/* IPC4 BaseFW Large Config Parameter IDs */
#define TB_IPC4_FW_CONFIG			0
#define TB_IPC4_HW_CONFIG_GET			1
#define TB_IPC4_MODULES_INFO_GET		9
#define TB_IPC4_PIPELINE_LIST_INFO_GET		12
#define TB_IPC4_MEMORY_STATE_INFO_GET		14
#define TB_IPC4_POWER_STATE_INFO_GET		15
#define TB_IPC4_PERF_MEASUREMENTS_STATE		17
#define TB_IPC4_GLOBAL_PERF_DATA		18
#define TB_IPC4_EXTENDED_GLOBAL_PERF_DATA	30

/* PCM Capture Ring Buffer Capacity (in S16_LE samples: 48000 * 2ch * 2 sec = 192000 samples) */
#define TB_PCM_CAPTURE_RING_SAMPLES		192000

struct system_tick_info_wire {
	uint32_t count;
	uint32_t last_time_elapsed;
	uint32_t max_time_elapsed;
	uint32_t last_ccount;
	uint32_t avg_utilization;
	uint32_t peak_utilization;
	uint32_t peak_utilization_4k;
	uint32_t peak_utilization_8k;
} __attribute__((packed));

struct perf_data_item_comp_wire {
	uint32_t resource_id;
	uint32_t power_mode : 1;
	uint32_t is_removed : 1;
	uint32_t rsvd       : 30;
	uint32_t peak_kcps;
	uint32_t avg_kcps;
	uint64_t total_iteration_count;
	uint64_t total_cycles_consumed;
} __attribute__((packed));

static pthread_mutex_t g_tb_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_tb_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t g_pcm_ring_lock = PTHREAD_MUTEX_INITIALIZER;

static struct testbench_prm *g_tp;
static int g_srv_fd = -1;
static pthread_t g_accept_thread;
static volatile bool g_srv_running;
static volatile bool g_quit_requested;
static volatile bool g_dsp_paused;
static volatile int g_step_ticks_remaining;
static __thread bool g_in_remote_rpc;
static uint32_t g_perf_meas_state = 2; /* 2 = STARTED by default */

/* Per-core telemetry state */
static struct system_tick_info_wire g_core_ticks[CONFIG_CORE_COUNT];
static uint64_t g_total_ticks;

/* Emulated SRAM Windows */
static uint8_t g_win0_fw_regs[4096];
static uint8_t g_win1_outbox[4096];
static uint8_t g_win2_debug[65536];
static uint8_t g_win3_perf[4096];
static char g_last_shell_output[4096];
static uint32_t g_shell_seq;

/* Tone generator & PCM capture state */
static volatile bool g_tone_running;
static char g_pcm_device[32] = "hw:0,0";
static uint32_t g_tone_freq_hz = 1000;
static uint32_t g_pcm_rate = 48000;
static uint32_t g_pcm_channels = 2;
static uint32_t g_pcm_api_used = SOF_AUDIO_API_ALSA;
static uint32_t g_pcm_format = SOF_PCM_FMT_S16_LE;
static uint32_t g_pcm_xruns;
static uint64_t g_pcm_frames_processed;
static uint32_t g_pcm_last_rms;
static uint32_t g_pcm_last_peak;
static int16_t g_pcm_capture_ring[TB_PCM_CAPTURE_RING_SAMPLES];
static size_t g_pcm_ring_write_idx;
static size_t g_pcm_ring_avail;

/* Cached Kcontrol state for SOF_NET_OP_KCTL_LIST / GET / SET */
#define TB_REMOTE_MAX_KCTLS 64
struct tb_kctl_shadow {
	bool initialized;
	int64_t values[8];
	uint32_t bytes_len;
	uint8_t bytes_data[512];
};
static struct tb_kctl_shadow g_kctl_shadow[TB_REMOTE_MAX_KCTLS];

/* Dynamically loaded LLEXT / shared-library modules */
#define TB_MAX_LOADED_LLEXTS 16
struct tb_loaded_llext {
	bool active;
	uint32_t lib_id;
	uint32_t data_size;
	char name[16];
	char path[128];
	void *dl_handle;
};
static struct tb_loaded_llext g_loaded_llexts[TB_MAX_LOADED_LLEXTS];

static int read_exact(int fd, void *buf, size_t len)
{
	uint8_t *p = (uint8_t *)buf;
	size_t rem = len;

	while (rem > 0) {
		ssize_t n = recv(fd, p, rem, 0);

		if (n <= 0) {
			if (n < 0 && errno == EINTR)
				continue;
			return -1;
		}
		p += n;
		rem -= (size_t)n;
	}
	return 0;
}

static int write_exact(int fd, const void *buf, size_t len)
{
	const uint8_t *p = (const uint8_t *)buf;
	size_t rem = len;

	while (rem > 0) {
		ssize_t n = send(fd, p, rem, MSG_NOSIGNAL);

		if (n <= 0) {
			if (n < 0 && errno == EINTR)
				continue;
			return -1;
		}
		p += n;
		rem -= (size_t)n;
	}
	return 0;
}

static int send_reply(int sock, uint16_t opcode, uint32_t req_id,
		      int32_t status, const void *payload, uint32_t payload_len)
{
	struct sof_net_hdr hdr = {
		.magic       = SOF_NET_MAGIC,
		.version     = SOF_NET_VERSION,
		.opcode      = opcode,
		.req_id      = req_id,
		.payload_len = payload ? payload_len : 0,
		.status      = status,
	};

	if (write_exact(sock, &hdr, sizeof(hdr)) < 0)
		return -1;
	if (payload && payload_len > 0) {
		if (write_exact(sock, payload, payload_len) < 0)
			return -1;
	}
	return 0;
}

/* -------------------------------------------------------------------------
 * PCM Output Recording & Tone State Hooks (called from file.c)
 * -------------------------------------------------------------------------
 */

bool tb_remote_is_tone_active(uint32_t *freq_hz)
{
	if (!g_tone_running)
		return false;
	if (freq_hz)
		*freq_hz = g_tone_freq_hz ? g_tone_freq_hz : 1000;
	return true;
}

void tb_remote_record_output_samples_s16(const int16_t *samples, size_t count,
					 int channels, int rate)
{
	double sum_sq = 0.0;
	int16_t peak = 0;
	size_t i;

	if (!samples || count == 0)
		return;

	pthread_mutex_lock(&g_pcm_ring_lock);
	if (channels > 0)
		g_pcm_channels = (uint32_t)channels;
	if (rate > 0)
		g_pcm_rate = (uint32_t)rate;

	for (i = 0; i < count; i++) {
		int16_t v = samples[i];
		int16_t av = v < 0 ? -v : v;

		if (av > peak)
			peak = av;
		sum_sq += (double)v * (double)v;

		g_pcm_capture_ring[g_pcm_ring_write_idx] = v;
		g_pcm_ring_write_idx = (g_pcm_ring_write_idx + 1) % TB_PCM_CAPTURE_RING_SAMPLES;
		if (g_pcm_ring_avail < TB_PCM_CAPTURE_RING_SAMPLES)
			g_pcm_ring_avail++;
	}

	g_pcm_last_rms = (uint32_t)sqrt(sum_sq / (double)count);
	g_pcm_last_peak = (uint32_t)peak;
	if (g_pcm_channels > 0)
		g_pcm_frames_processed += count / g_pcm_channels;
	pthread_mutex_unlock(&g_pcm_ring_lock);
}

void tb_remote_record_output_samples_s32(const int32_t *samples, size_t count,
					 int channels, int rate, int shift_to_s16)
{
	double sum_sq = 0.0;
	int16_t peak = 0;
	size_t i;

	if (!samples || count == 0)
		return;

	pthread_mutex_lock(&g_pcm_ring_lock);
	if (channels > 0)
		g_pcm_channels = (uint32_t)channels;
	if (rate > 0)
		g_pcm_rate = (uint32_t)rate;

	for (i = 0; i < count; i++) {
		int32_t s32 = samples[i] >> shift_to_s16;
		int16_t v;
		int16_t av;

		if (s32 > 32767)
			s32 = 32767;
		else if (s32 < -32768)
			s32 = -32768;
		v = (int16_t)s32;
		av = v == -32768 ? 32767 : (v < 0 ? -v : v);
		if (av > peak)
			peak = av;
		sum_sq += (double)v * (double)v;

		g_pcm_capture_ring[g_pcm_ring_write_idx] = v;
		g_pcm_ring_write_idx = (g_pcm_ring_write_idx + 1) % TB_PCM_CAPTURE_RING_SAMPLES;
		if (g_pcm_ring_avail < TB_PCM_CAPTURE_RING_SAMPLES)
			g_pcm_ring_avail++;
	}

	g_pcm_last_rms = (uint32_t)sqrt(sum_sq / (double)count);
	g_pcm_last_peak = (uint32_t)peak;
	if (g_pcm_channels > 0)
		g_pcm_frames_processed += count / g_pcm_channels;
	pthread_mutex_unlock(&g_pcm_ring_lock);
}

/* -------------------------------------------------------------------------
 * Emulated SRAM Windows, Device Info & Telemetry Synthesis
 * -------------------------------------------------------------------------
 */

static struct tplg_comp_info *find_comp_info_by_ipc_id(struct testbench_prm *tp, uint32_t ipc_id)
{
	struct list_item *item;
	int i;

	if (!tp)
		return NULL;

	if (tp->widget_list.next) {
		list_for_item(item, &tp->widget_list) {
			struct tplg_comp_info *ci = container_of(item, struct tplg_comp_info, item);
			uint32_t cid = (uint32_t)IPC4_COMP_ID(ci->module_id, ci->instance_id);

			if (cid == ipc_id)
				return ci;
		}
		list_for_item(item, &tp->widget_list) {
			struct tplg_comp_info *ci = container_of(item, struct tplg_comp_info, item);

			if ((uint32_t)ci->id == ipc_id)
				return ci;
		}
	}

	if (!tp->info)
		return NULL;

	for (i = 0; i < tp->info_elems; i++) {
		uint32_t cid = (uint32_t)IPC4_COMP_ID(tp->info[i].module_id,
						      tp->info[i].instance_id);

		if (cid == ipc_id)
			return &tp->info[i];
	}
	for (i = 0; i < tp->info_elems; i++) {
		if ((uint32_t)tp->info[i].id == ipc_id)
			return &tp->info[i];
	}
	return NULL;
}

/* Cached widget names so post-run sched-trace / sched-telem queries resolve names */
struct tb_cached_widget_name {
	uint32_t comp_id;
	char name[32];
};
static struct tb_cached_widget_name g_cached_widget_names[128];
static uint32_t g_num_cached_widget_names;

static void cache_active_widget_names(struct testbench_prm *tp)
{
	struct list_item *item;
	int i;

	if (!tp)
		return;

	if (tp->widget_list.next && !list_is_empty(&tp->widget_list)) {
		g_num_cached_widget_names = 0;
		list_for_item(item, &tp->widget_list) {
			struct tplg_comp_info *ci = container_of(item, struct tplg_comp_info, item);
			uint32_t cid = (uint32_t)IPC4_COMP_ID(ci->module_id, ci->instance_id);

			if (!ci->name || g_num_cached_widget_names >= 128)
				continue;
			g_cached_widget_names[g_num_cached_widget_names].comp_id = cid;
			snprintf(g_cached_widget_names[g_num_cached_widget_names].name,
				 sizeof(g_cached_widget_names[0].name), "%s", ci->name);
			g_num_cached_widget_names++;
		}
	} else if (tp->info && tp->info_elems > 0) {
		g_num_cached_widget_names = 0;
		for (i = 0; i < tp->info_elems && g_num_cached_widget_names < 128; i++) {
			uint32_t cid = (uint32_t)IPC4_COMP_ID(tp->info[i].module_id,
							      tp->info[i].instance_id);
			if (!tp->info[i].name)
				continue;
			g_cached_widget_names[g_num_cached_widget_names].comp_id = cid;
			snprintf(g_cached_widget_names[g_num_cached_widget_names].name,
				 sizeof(g_cached_widget_names[0].name), "%s", tp->info[i].name);
			g_num_cached_widget_names++;
		}
	}
}

static const char *resolve_comp_widget_name(struct testbench_prm *tp, uint32_t ipc_id)
{
	struct tplg_comp_info *ci = find_comp_info_by_ipc_id(tp, ipc_id);
	uint32_t i;

	if (ci && ci->name)
		return ci->name;
	for (i = 0; i < g_num_cached_widget_names; i++) {
		if (g_cached_widget_names[i].comp_id == ipc_id &&
		    g_cached_widget_names[i].name[0])
			return g_cached_widget_names[i].name;
	}
	return "comp";
}

static void populate_cdev_info(struct testbench_prm *tp, struct sof_cdev_info *info)
{
	struct host_sched_telemetry *st = host_telem_get();
	const char *base_tplg = "";
	int i;

	memset(info, 0, sizeof(*info));
	info->abi_version = SOF_CDEV_ABI_VERSION;
#if CONFIG_IPC_MAJOR_4
	info->ipc_type = 1;
#else
	info->ipc_type = 0;
#endif
	info->fw_state = 7; /* SOF_FW_BOOT_COMPLETE */
	info->dsp_power_state = g_dsp_paused ? 4 : 1; /* 1=D0, 4=D3 */
	info->pm_runtime_active = g_dsp_paused ? 0 : 1;
	info->num_cores = st ? st->hw_cfg.num_cores : CONFIG_CORE_COUNT;
	info->enabled_cores_mask = arch_cpu_enabled_cores();
	info->max_payload_size = SOF_IPC_MSG_MAX_SIZE;
	info->fw_ver_major = SOF_MAJOR;
	info->fw_ver_minor = SOF_MINOR;
	info->fw_ver_micro = SOF_MICRO;
	info->fw_ver_build = SOF_BUILD;
	info->hw_ip_version = (st && st->hw_cfg.hw_ip_version) ? st->hw_cfg.hw_ip_version : 4;
	snprintf(info->platform, sizeof(info->platform), "%s",
		 (st && st->hw_cfg.platform_name[0]) ? st->hw_cfg.platform_name : "sof-testbench4");

	if (tp && tp->tplg_file) {
		const char *slash = strrchr(tp->tplg_file, '/');

		base_tplg = slash ? (slash + 1) : tp->tplg_file;
	}
	snprintf(info->fw_filename, sizeof(info->fw_filename), "%s", base_tplg);

	/* 4 SRAM windows */
	info->windows[0].win_id = SOF_CDEV_WIN_FW_REGS;
	info->windows[0].bar_offset = 0x000000;
	info->windows[0].size = sizeof(g_win0_fw_regs);
	info->windows[0].flags = 1;

	info->windows[1].win_id = SOF_CDEV_WIN_OUTBOX;
	info->windows[1].bar_offset = 0x001000;
	info->windows[1].size = sizeof(g_win1_outbox);
	info->windows[1].flags = 1;

	info->windows[2].win_id = SOF_CDEV_WIN_DEBUG;
	info->windows[2].bar_offset = 0x010000;
	info->windows[2].size = sizeof(g_win2_debug);
	info->windows[2].flags = 1;

	info->windows[3].win_id = SOF_CDEV_WIN_PERF;
	info->windows[3].bar_offset = 0x020000;
	info->windows[3].size = sizeof(g_win3_perf);
	info->windows[3].flags = 1;

	/* Window 2 Debug Slots */
	for (i = 0; i < SOF_CDEV_MAX_DEBUG_SLOTS; i++) {
		info->debug_slots[i].slot_index = i;
		info->debug_slots[i].win_offset = (i + 1) * 4096;
		info->debug_slots[i].bar_offset = 0x010000 + (i + 1) * 4096;
		info->debug_slots[i].size = 4096;
	}

	info->debug_slots[0].res_id = 0x00000001;
	info->debug_slots[0].type = SOF_CDEV_SLOT_TELEMETRY;
	info->debug_slots[0].vma = 0xA0011000;

	info->debug_slots[1].res_id = 0x00000002;
	info->debug_slots[1].type = SOF_CDEV_SLOT_SHELL;
	info->debug_slots[1].vma = 0xA0012000;

	info->debug_slots[2].res_id = 0x00000003;
	info->debug_slots[2].type = SOF_CDEV_SLOT_DEBUG_LOG;
	info->debug_slots[2].vma = 0xA0013000;
}

static uint32_t build_modules_info_tlv(struct testbench_prm *tp, uint8_t *out, uint32_t cap)
{
	/*
	 * Matches TelemetryParser.parse_modules_info_tlv in tools/softop/backend.py:
	 *   uint32_t mod_count
	 *   followed by mod_count * 116-byte sof_man_module entries:
	 *     bytes 0..1: uint16_t module_id
	 *     bytes 2..3: uint16_t state_flags
	 *     bytes 4..11: char name[8]
	 */
	uint32_t seen_mod_ids[64];
	char seen_names[64][8];
	uint32_t count = 0;
	int i;

	if (cap < 4)
		return 0;

	memset(out, 0, cap);

	/* Always include module_id 0 = basefw */
	seen_mod_ids[count] = 0;
	memset(seen_names[count], 0, 8);
	memcpy(seen_names[count], "basefw", 6);
	count++;

	if (tp && tp->widget_list.next) {
		struct list_item *item;

		list_for_item(item, &tp->widget_list) {
			struct tplg_comp_info *ci = container_of(item, struct tplg_comp_info, item);
			uint32_t mid = (uint32_t)ci->module_id;
			bool dup = false;
			uint32_t j;

			if (count >= 64)
				break;
			if (mid == 0 || !ci->name)
				continue;

			for (j = 0; j < count; j++) {
				if (seen_mod_ids[j] == mid) {
					dup = true;
					break;
				}
			}
			if (dup)
				continue;

			seen_mod_ids[count] = mid;
			memset(seen_names[count], 0, 8);
			const char *dot = strchr(ci->name, '.');
			size_t nlen = dot ? (size_t)(dot - ci->name) : strlen(ci->name);

			if (nlen > 7)
				nlen = 7;
			memcpy(seen_names[count], ci->name, nlen);
			count++;
		}
	} else if (tp && tp->info) {
		for (i = 0; i < tp->info_elems && count < 64; i++) {
			struct tplg_comp_info *ci = &tp->info[i];
			uint32_t mid = (uint32_t)ci->module_id;
			bool dup = false;
			uint32_t j;

			if (mid == 0 || !ci->name)
				continue;

			for (j = 0; j < count; j++) {
				if (seen_mod_ids[j] == mid) {
					dup = true;
					break;
				}
			}
			if (dup)
				continue;

			seen_mod_ids[count] = mid;
			memset(seen_names[count], 0, 8);
			/* Extract prefix before '.' from widget name (e.g. "gain.1.1" -> "gain") */
			const char *dot = strchr(ci->name, '.');
			size_t nlen = dot ? (size_t)(dot - ci->name) : strlen(ci->name);

			if (nlen > 7)
				nlen = 7;
			memcpy(seen_names[count], ci->name, nlen);
			count++;
		}
	}

	/* Include any dynamically loaded LLEXT modules */
	for (i = 0; i < TB_MAX_LOADED_LLEXTS && count < 64; i++) {
		if (!g_loaded_llexts[i].active)
			continue;
		seen_mod_ids[count] = 0x100 + g_loaded_llexts[i].lib_id;
		memset(seen_names[count], 0, 8);
		strncpy(seen_names[count], g_loaded_llexts[i].name, 7);
		count++;
	}

	uint32_t total_len = 4 + count * 116;

	if (total_len > cap) {
		count = (cap - 4) / 116;
		total_len = 4 + count * 116;
	}

	memcpy(out, &count, sizeof(uint32_t));
	for (uint32_t idx = 0; idx < count; idx++) {
		uint8_t *ent = out + 4 + idx * 116;
		uint16_t mid16 = (uint16_t)seen_mod_ids[idx];
		uint16_t flg16 = 1;

		memcpy(ent + 0, &mid16, 2);
		memcpy(ent + 2, &flg16, 2);
		memcpy(ent + 4, seen_names[idx], 8);
	}

	return total_len;
}

static uint32_t build_power_state_tlv(uint8_t *out, uint32_t cap)
{
	/*
	 * TLV 0: IPC4_ACTIVE_CORES_MASK (uint32_t)
	 * TLV 1: IPC4_CORE_KCPS (uint32_t[num_cores])
	 */
	struct host_sched_telemetry *st = host_telem_get();
	uint32_t num_cores = (st && st->hw_cfg.num_cores <= CONFIG_CORE_COUNT) ?
			     st->hw_cfg.num_cores : CONFIG_CORE_COUNT;
	uint32_t core_kcps[CONFIG_CORE_COUNT];
	uint32_t mask = arch_cpu_enabled_cores();
	uint32_t off = 0;
	uint32_t c;

	if (cap < 8 + 4 + 8 + num_cores * sizeof(uint32_t))
		return 0;

	for (c = 0; c < num_cores; c++)
		core_kcps[c] = g_core_ticks[c].avg_utilization;

	/* Tuple 0: type=0, len=4, val=mask */
	uint32_t t0_type = 0, t0_len = 4;

	memcpy(out + off, &t0_type, 4); off += 4;
	memcpy(out + off, &t0_len, 4);  off += 4;
	memcpy(out + off, &mask, 4);    off += 4;

	/* Tuple 1: type=1, len=num_cores*4, val=core_kcps */
	uint32_t t1_type = 1, t1_len = num_cores * sizeof(uint32_t);

	memcpy(out + off, &t1_type, 4);       off += 4;
	memcpy(out + off, &t1_len, 4);        off += 4;
	memcpy(out + off, core_kcps, t1_len); off += t1_len;

	return off;
}

static uint32_t build_memory_state_tlv(struct testbench_prm *tp, uint8_t *out, uint32_t cap)
{
	/*
	 * Matches TelemetryParser.parse_memory_state_tlv in tools/softop/backend.py:
	 *   TLV type 1 (HPSRAM) and TLV type 0 (LPSRAM)
	 *   Payload layout:
	 *     uint32_t free_phys_pages
	 *     uint32_t ebb_dwords (1 or 3 depending on bank_count)
	 *     uint32_t pgctl_masks[ebb_dwords] (bit=0 powered ON, bit=1 gated)
	 *     uint32_t bank_count (configured HPSRAM/LPSRAM 64 KB EBB banks)
	 *     uint16_t bank_alloc_mask[bank_count]
	 */
	struct host_sched_telemetry *st = host_telem_get();
	struct tb_heap_usage_record recs[64];
	int rec_count = 0;
	size_t total_heap_bytes = 0;
	int i;

	if (tp) {
		memset(recs, 0, sizeof(recs));
		tb_collect_heap_usage(tp, recs, &rec_count);
		for (i = 0; i < rec_count && i < 64; i++)
			total_heap_bytes += recs[i].heap_max;
	}
	if (st && st->hw_cfg.hpsram_used_bytes > total_heap_bytes)
		total_heap_bytes = st->hw_cfg.hpsram_used_bytes;

	uint32_t off = 0;
	size_t hp_total_bytes = (st && st->hw_cfg.hpsram_size_bytes >= 65536UL) ?
				st->hw_cfg.hpsram_size_bytes : (16UL * 65536UL);
	uint32_t hp_banks = (uint32_t)((hp_total_bytes + 65535UL) / 65536UL);
	uint32_t hp_used_banks = (uint32_t)((total_heap_bytes + 65535UL) / 65536UL);

	if (hp_banks > 96)
		hp_banks = 96;
	if (hp_used_banks == 0 && total_heap_bytes > 0)
		hp_used_banks = 1;
	if (hp_used_banks < 1)
		hp_used_banks = 1;
	if (hp_used_banks > hp_banks)
		hp_used_banks = hp_banks;

	uint32_t hp_ebb_dwords = (hp_banks + 31U) / 32U;
	uint32_t hp_free_pages = (hp_banks - hp_used_banks) * 16U;
	uint32_t hp_val_len = 12U + hp_ebb_dwords * 4U + hp_banks * sizeof(uint16_t);

	if (off + 8U + hp_val_len <= cap) {
		uint32_t ttype = 1;

		memcpy(out + off, &ttype, 4);          off += 4;
		memcpy(out + off, &hp_val_len, 4);     off += 4;
		memcpy(out + off, &hp_free_pages, 4);  off += 4;
		memcpy(out + off, &hp_ebb_dwords, 4);  off += 4;
		for (uint32_t d = 0; d < hp_ebb_dwords; d++) {
			uint32_t pgctl = 0xFFFFFFFFU;

			for (uint32_t bit = 0; bit < 32U; bit++) {
				if (d * 32U + bit < hp_used_banks)
					pgctl &= ~(1U << bit);
			}
			memcpy(out + off, &pgctl, 4);
			off += 4;
		}
		memcpy(out + off, &hp_banks, 4);       off += 4;
		for (uint32_t b = 0; b < hp_banks; b++) {
			uint16_t amask = (b < hp_used_banks) ? 0xFFFF : 0x0000;

			memcpy(out + off, &amask, 2);
			off += 2;
		}
	}

	/* Build LPSRAM (ttype = 0) */
	size_t lp_total_bytes = (st && st->hw_cfg.lpsram_size_bytes >= 65536UL) ?
				st->hw_cfg.lpsram_size_bytes : 65536UL;
	size_t lp_used_bytes = st ? st->hw_cfg.lpsram_used_bytes : 0;
	uint32_t lp_banks = (uint32_t)((lp_total_bytes + 65535UL) / 65536UL);
	uint32_t lp_used_banks = (uint32_t)((lp_used_bytes + 65535UL) / 65536UL);

	if (lp_banks > 32)
		lp_banks = 32;
	if (lp_used_banks > lp_banks)
		lp_used_banks = lp_banks;

	uint32_t lp_pgctl = (lp_used_banks >= 32) ? 0U : ~((1U << lp_used_banks) - 1U);
	uint32_t lp_free_pages = (lp_banks - lp_used_banks) * 16U;
	uint32_t lp_ebb_dwords = 1;
	uint32_t lp_val_len = 16 + lp_banks * sizeof(uint16_t);

	if (off + 8 + lp_val_len <= cap) {
		uint32_t ttype = 0;

		memcpy(out + off, &ttype, 4);          off += 4;
		memcpy(out + off, &lp_val_len, 4);     off += 4;
		memcpy(out + off, &lp_free_pages, 4);  off += 4;
		memcpy(out + off, &lp_ebb_dwords, 4);  off += 4;
		memcpy(out + off, &lp_pgctl, 4);       off += 4;
		memcpy(out + off, &lp_banks, 4);       off += 4;
		for (uint32_t b = 0; b < lp_banks; b++) {
			uint16_t amask = (b < lp_used_banks) ? 0xFFFF : 0x0000;

			memcpy(out + off, &amask, 2);
			off += 2;
		}
	}

	return off;
}

static uint32_t build_hw_config_tlv(uint8_t *out, uint32_t cap)
{
	/*
	 * IPC4_HW_CONFIG_GET (param_id = 1) TLVs:
	 *   0: VER_HW_CFG          (uint32_t)
	 *   1: DSP_CORES           (uint32_t)
	 *   2: MEM_PAGE_BYTES      (uint32_t = 4096)
	 *   3: TOTAL_PHYS_MEM_PAGES(uint32_t)
	 *   6: EBB_SIZE_BYTES      (uint32_t = 65536)
	 *   7: EBB_TOTAL_COUNT     (uint32_t = HPSRAM banks)
	 *   9: HP_EBB_COUNT        (uint32_t = HPSRAM banks)
	 *  10: LP_EBB_COUNT        (uint32_t = LPSRAM banks)
	 *  11: DSP_CLOCK_HZ        (uint32_t = core 0 freq Hz)
	 */
	struct host_sched_telemetry *st = host_telem_get();
	uint32_t hw_ver = st ? st->hw_cfg.hw_ip_version : 4;
	uint32_t cores = st ? st->hw_cfg.num_cores : CONFIG_CORE_COUNT;
	uint32_t page_sz = 4096;
	uint32_t ebb_sz = 65536;
	uint32_t hp_ebbs = st ? (uint32_t)(st->hw_cfg.hpsram_size_bytes / 65536UL) : 47;
	uint32_t lp_ebbs = st ? (uint32_t)(st->hw_cfg.lpsram_size_bytes / 65536UL) : 1;
	uint32_t tot_pages = (hp_ebbs + lp_ebbs) * 16U;
	uint32_t clk_hz = st ? st->sim_cfg.core_freq_hz[0] : (uint32_t)TB_DSP_NOMINAL_HZ;
	const uint32_t tuples[][2] = {
		{ 0, hw_ver },
		{ 1, cores },
		{ 2, page_sz },
		{ 3, tot_pages },
		{ 6, ebb_sz },
		{ 7, hp_ebbs },
		{ 9, hp_ebbs },
		{ 10, lp_ebbs },
		{ 11, clk_hz },
	};
	uint32_t off = 0;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(tuples); i++) {
		uint32_t t = tuples[i][0];
		uint32_t l = 4;
		uint32_t v = tuples[i][1];

		if (off + 12 > cap)
			break;
		memcpy(out + off, &t, 4); off += 4;
		memcpy(out + off, &l, 4); off += 4;
		memcpy(out + off, &v, 4); off += 4;
	}
	return off;
}

static uint32_t build_ext_perf_tlv(struct testbench_prm *tp, uint8_t *out, uint32_t cap)
{
	/*
	 * Builds uint32_t count + struct perf_data_item_comp_wire[count] (32-byte stride)
	 * Also populates g_win3_perf for Window 3 reads.
	 */
	struct ipc *ipc = sof_get() ? sof_get()->ipc : NULL;
	struct list_item *clist;
	uint32_t count = 0;
	uint32_t max_items = (cap > 4) ? ((cap - 4) / sizeof(struct perf_data_item_comp_wire)) : 0;
	struct perf_data_item_comp_wire *items = (struct perf_data_item_comp_wire *)(out + 4);
	struct perf_data_item_comp_wire *win3_items = (struct perf_data_item_comp_wire *)g_win3_perf;
	uint32_t win3_count = 0;
	uint32_t enabled_mask = arch_cpu_enabled_cores();
	int c;

	if (max_items == 0)
		return 0;

	memset(out, 0, cap);
	memset(g_win3_perf, 0, sizeof(g_win3_perf));

	/* 1. Per-core summary items (module_id == 0, instance_id == core_id) */
	struct host_sched_telemetry *st = host_telem_get();

	for (c = 0; c < CONFIG_CORE_COUNT && count < max_items; c++) {
		if (!(enabled_mask & BIT(c)) && !(st && st->cores[c].active))
			continue;
		struct perf_data_item_comp_wire *it = &items[count++];

		it->resource_id = IPC4_COMP_ID(0, c);
		it->power_mode = 0;
		it->is_removed = 0;
		if (st && st->cores[c].active) {
			it->avg_kcps = st->cores[c].ll_avg_kcps + st->cores[c].dp_avg_kcps;
			it->peak_kcps = st->cores[c].ll_peak_kcps + st->cores[c].dp_peak_kcps;
			it->total_iteration_count = st->cores[c].ll_tick_count +
						    st->cores[c].dp_run_count;
			it->total_cycles_consumed = st->cores[c].ll_total_cycles +
						    st->cores[c].dp_total_cycles;
		} else {
			it->avg_kcps = g_core_ticks[c].avg_utilization;
			it->peak_kcps = g_core_ticks[c].peak_utilization;
			it->total_iteration_count = g_core_ticks[c].count;
			it->total_cycles_consumed = (uint64_t)g_core_ticks[c].count *
						    g_core_ticks[c].last_time_elapsed;
		}
	}

	/* 2. Per-component items from active SOF components & real telemetry */
	if (ipc) {
		list_for_item(clist, &ipc->comp_list) {
			struct ipc_comp_dev *icd = container_of(clist, struct ipc_comp_dev, list);

			if (icd->type != COMP_TYPE_COMPONENT || !icd->cd)
				continue;
			if (count >= max_items)
				break;

			struct perf_data_item_comp_wire *it = &items[count++];
			struct host_mod_telem *mt = NULL;
			uint32_t i;

			if (st) {
				for (i = 0; i < st->num_mods; i++) {
					if (st->mods[i].active &&
					    st->mods[i].comp_id == icd->id) {
						mt = &st->mods[i];
						break;
					}
				}
			}

			it->resource_id = icd->id;
			it->power_mode = (icd->cd->ipc_config.proc_domain ==
					  COMP_PROCESSING_DOMAIN_DP) ? 1 : 0;
			it->is_removed = 0;
			if (mt && mt->iterations > 0) {
				it->avg_kcps = mt->avg_kcps;
				it->peak_kcps = mt->peak_kcps;
				it->total_iteration_count = mt->iterations;
				it->total_cycles_consumed = mt->total_cycles;
			} else {
				uint32_t core = icd->core < CONFIG_CORE_COUNT ? icd->core : 0;
				uint32_t iters = g_core_ticks[core].count;

				it->avg_kcps = 100;
				it->peak_kcps = 200;
				it->total_iteration_count = iters;
				it->total_cycles_consumed = (uint64_t)iters * it->avg_kcps;
			}

			if ((win3_count + 1) * sizeof(*win3_items) <= sizeof(g_win3_perf))
				win3_items[win3_count++] = *it;
		}
	}
	(void)tp;

	memcpy(out, &count, sizeof(uint32_t));
	return 4 + count * sizeof(struct perf_data_item_comp_wire);
}

static void refresh_emulated_windows(struct testbench_prm *tp)
{
	/* Window 0: FW Regs */
	uint32_t *r32 = (uint32_t *)g_win0_fw_regs;

	r32[0] = 0x00000007U; /* fw_status = BOOT_COMPLETE */
	r32[1] = 0x00000000U; /* lec */
	r32[2] = g_dsp_paused ? 0x00000004U : 0x00000001U; /* fps */
	r32[3] = 0x00000000U; /* lnec */
	r32[4] = (uint32_t)g_total_ticks; /* ltr */

	/* Window 2 Slot 0 (offset 4096): Telemetry Slot */
	uint8_t *tslot = g_win2_debug + 4096;
	uint32_t sep = 0x0000C0DEU;

	memcpy(tslot, &sep, 4);
	memcpy(tslot + 4, g_core_ticks, sizeof(g_core_ticks));

	/* Window 3: Perf window */
	uint8_t tmp_perf[4096];

	build_ext_perf_tlv(tp, tmp_perf, sizeof(tmp_perf));
}

/* -------------------------------------------------------------------------
 * Tick-Boundary Synchronization Hooks (called from testbench.c)
 * -------------------------------------------------------------------------
 */

void tb_remote_tick_begin(struct testbench_prm *tp)
{
	if (!g_srv_running || g_in_remote_rpc)
		return;

	pthread_mutex_lock(&g_tb_lock);
	g_tp = tp;

	while ((g_dsp_paused || (!tp->tplg_file && sof_get() && sof_get()->ipc &&
				 list_is_empty(&sof_get()->ipc->comp_list))) &&
	       g_step_ticks_remaining <= 0 && !g_quit_requested && g_srv_running)
		pthread_cond_wait(&g_tb_cond, &g_tb_lock);

	if (g_step_ticks_remaining > 0)
		g_step_ticks_remaining--;
}

void tb_remote_tick_end(struct testbench_prm *tp, uint64_t delta_cycles, uint64_t delta_ns)
{
	uint32_t enabled_mask;
	uint32_t sim_cycles;
	uint32_t kcps;
	int c;

	if (!g_srv_running)
		return;

	g_total_ticks++;
	enabled_mask = arch_cpu_enabled_cores();

	/* Convert host elapsed ns (or Xtensa cycles) into simulated 400 MHz DSP cycles */
	if (delta_cycles > 0)
		sim_cycles = (uint32_t)delta_cycles;
	else if (delta_ns > 0)
		sim_cycles = (uint32_t)((delta_ns * 400ULL) / 1000ULL);
	else
		sim_cycles = 12000;

	if (sim_cycles < 4000)
		sim_cycles = 4000;
	if (sim_cycles > 380000)
		sim_cycles = 380000;

	kcps = sim_cycles; /* 1 ms tick -> cycles per tick == kcps */

	for (c = 0; c < CONFIG_CORE_COUNT; c++) {
		struct host_sched_telemetry *st = host_telem_get();

		if (g_perf_meas_state == 2 /* STARTED */) {
			if (st && (st->num_pipes > 0 || st->num_mods > 0)) {
				if (st->cores[c].active) {
					struct host_core_telem *ct = &st->cores[c];
					uint32_t last_cyc = (uint32_t)(ct->ll_last_cycles +
								       ct->dp_last_cycles);
					uint32_t peak_cyc = (uint32_t)(ct->ll_peak_cycles +
								       ct->dp_peak_cycles);
					uint32_t avg_k = ct->ll_avg_kcps + ct->dp_avg_kcps;
					uint32_t peak_k = ct->ll_peak_kcps + ct->dp_peak_kcps;

					g_core_ticks[c].count = ct->ll_tick_count +
								ct->dp_run_count;
					g_core_ticks[c].last_time_elapsed = last_cyc;
					g_core_ticks[c].max_time_elapsed = peak_cyc;
					g_core_ticks[c].last_ccount =
						(uint32_t)(ct->ll_total_cycles +
							   ct->dp_total_cycles);
					g_core_ticks[c].avg_utilization = avg_k;
					g_core_ticks[c].peak_utilization = peak_k;
					g_core_ticks[c].peak_utilization_4k = peak_k;
					g_core_ticks[c].peak_utilization_8k = peak_k;
				}
			} else if (enabled_mask & BIT(c)) {
				g_core_ticks[c].count++;
				g_core_ticks[c].last_time_elapsed = sim_cycles;
				if (sim_cycles > g_core_ticks[c].max_time_elapsed)
					g_core_ticks[c].max_time_elapsed = sim_cycles;
				g_core_ticks[c].last_ccount += sim_cycles;
				g_core_ticks[c].avg_utilization =
					g_core_ticks[c].avg_utilization ?
					((g_core_ticks[c].avg_utilization * 7 + kcps) / 8) : kcps;
				if (kcps > g_core_ticks[c].peak_utilization)
					g_core_ticks[c].peak_utilization = kcps;
				g_core_ticks[c].peak_utilization_4k =
					g_core_ticks[c].peak_utilization;
				g_core_ticks[c].peak_utilization_8k =
					g_core_ticks[c].peak_utilization;
			}
		}
	}

	refresh_emulated_windows(tp);
	if (g_in_remote_rpc)
		return;

	pthread_mutex_unlock(&g_tb_lock);

	/* When running in interactive server mode (-W or -S), pace slightly so network clients
	 * can acquire g_tb_lock without starvation and observe real-time telemetry.
	 */
	if (tp->server_wait || g_tone_running)
		usleep(500);
}

bool tb_remote_should_continue(struct testbench_prm *tp, bool file_eof)
{
	if (g_quit_requested)
		return false;

	if (!g_srv_running || g_in_remote_rpc)
		return !file_eof;

	if (g_tone_running)
		return true;

	if (tp && tp->server_wait) {
		if (file_eof) {
			/* Rewind all input files so the pipeline keeps running in server mode */
			int i;

			pthread_mutex_lock(&g_tb_lock);
			for (i = 0; i < tp->input_file_num; i++) {
				if (tp->fr[i].state && tp->fr[i].state->rfh) {
					rewind(tp->fr[i].state->rfh);
					tp->fr[i].state->reached_eof = false;
					tp->fr[i].state->copy_timeout = false;
				}
			}
			for (i = 0; i < tp->output_file_num; i++) {
				if (tp->fw[i].state) {
					tp->fw[i].state->reached_eof = false;
					tp->fw[i].state->copy_timeout = false;
				}
			}
			pthread_mutex_unlock(&g_tb_lock);
			return !g_quit_requested;
		}
		return true;
	}

	return !file_eof;
}

/* -------------------------------------------------------------------------
 * Interactive Testbench Shell Handler (SOF_NET_OP_SHELL_EXEC)
 * -------------------------------------------------------------------------
 */

static void remote_set_csv_files(char **arr, int *num_out, int max_num, const char *csv)
{
	char *copy, *save = NULL, *tok;
	int i;

	for (i = 0; i < *num_out && i < max_num; i++) {
		free(arr[i]);
		arr[i] = NULL;
	}
	*num_out = 0;
	if (!csv || !*csv)
		return;

	copy = strdup(csv);
	if (!copy)
		return;
	tok = strtok_r(copy, ",", &save);
	while (tok && *num_out < max_num) {
		arr[(*num_out)++] = strdup(tok);
		tok = strtok_r(NULL, ",", &save);
	}
	free(copy);
}

static void remote_set_csv_pipelines(struct testbench_prm *tp, const char *csv)
{
	char *copy, *save = NULL, *tok;
	int idx = 0;

	if (!tp || !csv || !*csv)
		return;

	copy = strdup(csv);
	if (!copy)
		return;
	tok = strtok_r(copy, ",", &save);
	while (tok && idx < TB_MAX_PIPELINES_NUM) {
		tp->pipelines[idx++] = atoi(tok);
		tok = strtok_r(NULL, ",", &save);
	}
	if (idx > 0)
		tp->pipeline_num = idx;
	free(copy);
}

static void remote_unload_active_topology(struct testbench_prm *tp)
{
	if (!tp)
		return;

	if (tp->tplg_file ||
	    (sof_get() && sof_get()->ipc && !list_is_empty(&sof_get()->ipc->comp_list))) {
		cache_active_widget_names(tp);
		tb_set_reset_state(tp);
		tb_free_all_pipelines(tp);
		tb_free_topology(tp);
	}
	if (tp->tplg_file) {
		free(tp->tplg_file);
		tp->tplg_file = NULL;
	}
	if (tp->control_fh) {
		fclose(tp->control_fh);
		tp->control_fh = NULL;
	}
	if (tp->control_file) {
		free(tp->control_file);
		tp->control_file = NULL;
	}
	memset(g_kctl_shadow, 0, sizeof(g_kctl_shadow));
}

static int tokenize_shell_args(char *buf, char **argv, int max_args)
{
	int argc = 0;
	char *p = buf;

	while (*p && argc < max_args) {
		while (*p == ' ' || *p == '\t')
			p++;
		if (!*p)
			break;
		if (*p == '"' || *p == '\'') {
			char q = *p++;

			argv[argc++] = p;
			while (*p && *p != q)
				p++;
			if (*p == q)
				*p++ = '\0';
		} else {
			argv[argc++] = p;
			while (*p && *p != ' ' && *p != '\t')
				p++;
			if (*p)
				*p++ = '\0';
		}
	}
	return argc;
}

static uint32_t remote_parse_freq_hz(const char *str)
{
	char *endp = NULL;
	double val;

	if (!str || !*str)
		return (uint32_t)TB_DSP_NOMINAL_HZ;
	val = strtod(str, &endp);
	if (val <= 0.0)
		return (uint32_t)TB_DSP_NOMINAL_HZ;
	if (endp && (*endp == 'k' || *endp == 'K'))
		return (uint32_t)(val * 1000.0 + 0.5);
	if (endp && (*endp == 'm' || *endp == 'M'))
		return (uint32_t)(val * 1000000.0 + 0.5);
	if (val < 100000.0)
		return (uint32_t)(val * 1000000.0 + 0.5);
	return (uint32_t)(val + 0.5);
}

static void remote_apply_freq_arg(const char *arg)
{
	char *copy, *saveptr = NULL, *tok;

	if (!arg)
		return;
	copy = strdup(arg);
	if (!copy)
		return;
	tok = strtok_r(copy, ",", &saveptr);
	while (tok) {
		char *colon = strchr(tok, ':');

		if (colon) {
			int core = atoi(tok);
			uint32_t hz = remote_parse_freq_hz(colon + 1);

			host_workload_sim_set_core_freq(core, hz);
		} else {
			uint32_t hz = remote_parse_freq_hz(tok);

			host_workload_sim_set_core_freq(-1, hz);
		}
		tok = strtok_r(NULL, ",", &saveptr);
	}
	free(copy);
}

static size_t remote_parse_mem_size_token(const char *str)
{
	char *endp = NULL;
	double val;

	if (!str || !*str)
		return 0;
	val = strtod(str, &endp);
	if (val <= 0.0)
		return 0;
	if (endp && (*endp == 'm' || *endp == 'M'))
		return (size_t)(val * 1024.0 * 1024.0 + 0.5);
	if (endp && (*endp == 'k' || *endp == 'K'))
		return (size_t)(val * 1024.0 + 0.5);
	if (endp && (*endp == 'b' || *endp == 'B'))
		return (size_t)(val + 0.5);
	if (val <= 65536.0)
		return (size_t)(val * 1024.0 + 0.5);
	return (size_t)(val + 0.5);
}

static int remote_apply_mem_cfg_arg(const char *arg)
{
	size_t hpsram = 0, lpsram = 0, dram = 0;
	bool enforce = true;
	char *copy, *saveptr = NULL, *tok;
	int pos = 0;

	if (!arg || !*arg)
		return -EINVAL;
	copy = strdup(arg);
	if (!copy)
		return -ENOMEM;

	tok = strtok_r(copy, ",", &saveptr);
	while (tok) {
		char *eq = strchr(tok, '=');

		if (eq) {
			*eq = '\0';
			const char *val = eq + 1;

			if (strcasecmp(tok, "hpsram") == 0 || strcasecmp(tok, "sram") == 0 ||
			    strcasecmp(tok, "hp") == 0) {
				hpsram = remote_parse_mem_size_token(val);
			} else if (strcasecmp(tok, "lpsram") == 0 || strcasecmp(tok, "lp") == 0) {
				lpsram = remote_parse_mem_size_token(val);
			} else if (strcasecmp(tok, "dram") == 0 || strcasecmp(tok, "imr") == 0 ||
				   strcasecmp(tok, "l3") == 0) {
				dram = remote_parse_mem_size_token(val);
			} else if (strcasecmp(tok, "strict") == 0 ||
				   strcasecmp(tok, "enforce") == 0) {
				enforce = (atoi(val) != 0 && strcasecmp(val, "off") != 0 &&
					   strcasecmp(val, "false") != 0);
			}
		} else if (strcasecmp(tok, "nostrict") == 0) {
			enforce = false;
		} else if (strcasecmp(tok, "strict") == 0) {
			enforce = true;
		} else {
			size_t sz = remote_parse_mem_size_token(tok);

			if (pos == 0)
				hpsram = sz;
			else if (pos == 1)
				lpsram = sz;
			else if (pos == 2)
				dram = sz;
			pos++;
		}
		tok = strtok_r(NULL, ",", &saveptr);
	}
	free(copy);

	if (hpsram == 0 && lpsram == 0 && dram == 0)
		return -EINVAL;
	host_platform_set_memory(hpsram, lpsram, dram, enforce);
	return 0;
}

static void remote_apply_mem_arg(const char *arg)
{
	uint32_t domain = HOST_MEM_DOMAIN_SRAM;
	uint32_t factor_x100 = 0;
	uint32_t cyc_per_word = 0;
	const char *colon;

	if (!arg)
		return;
	if (strncasecmp(arg, "dram", 4) == 0 || strcmp(arg, "1") == 0)
		domain = HOST_MEM_DOMAIN_DRAM;

	colon = strchr(arg, ':');
	if (colon) {
		factor_x100 = (uint32_t)strtoul(colon + 1, NULL, 10);
		colon = strchr(colon + 1, ':');
		if (colon)
			cyc_per_word = (uint32_t)strtoul(colon + 1, NULL, 10);
	}
	host_workload_sim_set_mem_domain(domain, factor_x100, cyc_per_word);
}

static size_t format_remote_test_summary(struct testbench_prm *tp, long long delta_t,
					 struct tb_heap_usage_record *heap_records,
					 int heap_records_count, char *out_buf,
					 size_t off, size_t out_cap)
{
	long long file_cycles = 0;
	int n_in = 0, n_out = 0, frames_out;
	int i;
	uint32_t u;
	struct host_sched_telemetry *st = host_telem_get();

	for (i = 0; i < tp->input_file_num; i++) {
		if (tp->fr[i].id < 0 || !tp->fr[i].state)
			continue;
		n_in += tp->fr[i].state->n;
		file_cycles += tp->fr[i].state->cycles_count;
	}
	for (i = 0; i < tp->output_file_num; i++) {
		if (tp->fw[i].id < 0 || !tp->fw[i].state)
			continue;
		n_out += tp->fw[i].state->n;
		file_cycles += tp->fw[i].state->cycles_count;
	}

	off += snprintf(out_buf + off, out_cap - off,
			"==========================================================\n"
			"		           Test Summary 1\n"
			"==========================================================\n");

	for (i = 0; i < tp->pipeline_num && off + 256 < out_cap; i++) {
		int pid = tp->pipelines[i];
		int j;

		off += snprintf(out_buf + off, out_cap - off, "pipeline %d\n", pid);
		for (j = 0; j < tp->input_file_num && off + 160 < out_cap; j++) {
			if (tp->fr[j].pipeline_id == pid && tp->fr[j].state) {
				off += snprintf(out_buf + off, out_cap - off,
						"file read %d:pipeline %d: used file %s: %d samples %d copies\n",
						tp->fr[j].id, pid, tp->fr[j].state->fn,
						tp->fr[j].state->n, tp->fr[j].state->copy_count);
			}
		}
		for (j = 0; j < tp->output_file_num && off + 160 < out_cap; j++) {
			if (tp->fw[j].pipeline_id == pid && tp->fw[j].state) {
				off += snprintf(out_buf + off, out_cap - off,
						"file write %d:pipeline %d: used file %s: %d samples %d copies\n",
						tp->fw[j].id, pid, tp->fw[j].state->fn,
						tp->fw[j].state->n, tp->fw[j].state->copy_count);
			}
		}
	}

	frames_out = tp->channels_out > 0 ? (n_out / tp->channels_out) : 0;
	off += snprintf(out_buf + off, out_cap - off,
			"Input bit format: %s\n"
			"Input sample rate: %d\n"
			"Output sample rate: %d\n"
			"Input sample (frame) count: %d (%d)\n"
			"Output sample (frame) count: %d (%d)\n",
			tp->bits_in ? tp->bits_in : "S32_LE",
			tp->fs_in, tp->fs_out,
			n_in, tp->channels_in > 0 ? (n_in / tp->channels_in) : 0,
			n_out, frames_out);

	for (i = 0; i < heap_records_count && off + 128 < out_cap; i++) {
		off += snprintf(out_buf + off, out_cap - off,
				"Heap usage for module %s: %u bytes\n",
				heap_records[i].module_name,
				(uint32_t)heap_records[i].heap_max);
	}

	if (delta_t <= 0)
		delta_t = 1;
	off += snprintf(out_buf + off, out_cap - off,
			"\nTotal execution time: %lld us, %.2f x realtime\n\n",
			delta_t,
			tp->fs_out > 0 ? ((float)frames_out / tp->fs_out * 1000000.0f / delta_t) : 0.0f);

	if (st && (st->num_pipes > 0 || st->num_mods > 0) && off + 512 < out_cap) {
		const char *mode_str =
			(st->sim_cfg.mode == HOST_WORKLOAD_SIM_REALTIME) ? "REALTIME-PACED" :
			(st->sim_cfg.mode == HOST_WORKLOAD_SIM_VIRTUAL)  ? "VIRTUAL-SIM" :
									   "HOST-MEASURED";
		const char *mem_str =
			(st->sim_cfg.default_mem_domain == HOST_MEM_DOMAIN_DRAM) ? "DRAM" : "SRAM";

		off += snprintf(out_buf + off, out_cap - off,
				"--- Per-Core LL & DP Scheduler Telemetry [Platform=%s Cores=%u HPSRAM=%zu/%zuKB LPSRAM=%zu/%zuKB Mode=%s Mem=%s DRAM_Factor=%u.%02ux] ---\n",
				st->hw_cfg.platform_name,
				st->hw_cfg.num_cores,
				st->hw_cfg.hpsram_peak_bytes / 1024UL,
				st->hw_cfg.hpsram_size_bytes / 1024UL,
				st->hw_cfg.lpsram_peak_bytes / 1024UL,
				st->hw_cfg.lpsram_size_bytes / 1024UL,
				mode_str, mem_str,
				st->sim_cfg.dram_factor_x100 / 100,
				st->sim_cfg.dram_factor_x100 % 100);
		for (u = 0; u < st->hw_cfg.num_cores && u < TB_TELEM_MAX_CORES && off + 256 < out_cap; u++) {
			struct host_core_telem *ct = &st->cores[u];

			if (!ct->active)
				continue;
			off += snprintf(out_buf + off, out_cap - off,
					"  Core %u (%u.%03u MHz): util=%u.%02u%% overruns=%u | LL: ticks=%u tasks=%u avg=%llu peak=%llu tot=%llu cyc (%u/%u kcps) exec=%llu.%03llu/%llu.%03lluus | DP: runs=%u thr=%u avg=%llu peak=%llu tot=%llu cyc (%u/%u kcps) exec=%llu.%03llu/%llu.%03lluus\n",
					u, ct->freq_hz / 1000000U,
					(ct->freq_hz % 1000000U) / 1000U,
					ct->total_util_x100 / 100, ct->total_util_x100 % 100,
					ct->ll_deadline_violations,
					ct->ll_tick_count, ct->ll_active_tasks,
					(unsigned long long)ct->ll_avg_cycles,
					(unsigned long long)ct->ll_peak_cycles,
					(unsigned long long)ct->ll_total_cycles,
					ct->ll_avg_kcps, ct->ll_peak_kcps,
					(unsigned long long)(ct->ll_avg_exec_ns / 1000ULL),
					(unsigned long long)(ct->ll_avg_exec_ns % 1000ULL),
					(unsigned long long)(ct->ll_peak_exec_ns / 1000ULL),
					(unsigned long long)(ct->ll_peak_exec_ns % 1000ULL),
					ct->dp_run_count, ct->dp_active_threads,
					(unsigned long long)ct->dp_avg_cycles,
					(unsigned long long)ct->dp_peak_cycles,
					(unsigned long long)ct->dp_total_cycles,
					ct->dp_avg_kcps, ct->dp_peak_kcps,
					(unsigned long long)(ct->dp_avg_exec_ns / 1000ULL),
					(unsigned long long)(ct->dp_avg_exec_ns % 1000ULL),
					(unsigned long long)(ct->dp_peak_exec_ns / 1000ULL),
					(unsigned long long)(ct->dp_peak_exec_ns % 1000ULL));
		}

		off += snprintf(out_buf + off, out_cap - off,
				"\n--- Per-Pipeline Scheduling Telemetry (%u active) ---\n",
				st->num_pipes);
		for (u = 0; u < st->num_pipes && off + 224 < out_cap; u++) {
			struct host_pipe_telem *pt = &st->pipes[u];

			if (!pt->active)
				continue;
			off += snprintf(out_buf + off, out_cap - off,
					"  Pipe %2u: core=%u prio=%u per=%uus mem=%s mods=%uLL/%uDP copies=%llu avg=%llu peak=%llu tot=%llu cyc (%u/%u kcps) exec=%llu.%03llu/%llu.%03lluus xruns=%u\n",
					pt->pipeline_id, pt->core_id, pt->priority, pt->period_us,
					pt->mem_domain == HOST_MEM_DOMAIN_DRAM ? "DRAM" : "SRAM",
					pt->num_ll_modules, pt->num_dp_modules,
					(unsigned long long)pt->copy_iterations,
					(unsigned long long)pt->avg_cycles,
					(unsigned long long)pt->peak_cycles,
					(unsigned long long)pt->total_cycles,
					pt->avg_kcps, pt->peak_kcps,
					(unsigned long long)(pt->avg_exec_ns / 1000ULL),
					(unsigned long long)(pt->avg_exec_ns % 1000ULL),
					(unsigned long long)(pt->peak_exec_ns / 1000ULL),
					(unsigned long long)(pt->peak_exec_ns % 1000ULL),
					pt->xrun_count);
		}

		off += snprintf(out_buf + off, out_cap - off,
				"\n--- Per-Module LL & DP Scheduling + Entry/Exit Buffer Telemetry (%u active) ---\n",
				st->num_mods);
		for (u = 0; u < st->num_mods && off + 256 < out_cap; u++) {
			struct host_mod_telem *mt = &st->mods[u];
			const char *wname;

			if (!mt->active)
				continue;
			wname = resolve_comp_widget_name(tp, mt->comp_id);

			off += snprintf(out_buf + off, out_cap - off,
					"  [0x%06x %-24s] pipe=%u core=%u %s mem=%s iters=%llu avg=%llu peak=%llu tot=%llu cyc (%u/%u kcps) exec=%llu.%03llu/%llu.%03lluus last=%llu.%03llu->%llu.%03lluus(dl=%llu.%03lluus) ibs=%u obs=%u cpc=%u(viol=%u)\n",
					mt->comp_id, wname, mt->pipeline_id, mt->core_id,
					mt->proc_domain == COMP_PROCESSING_DOMAIN_DP ? "DP" : "LL",
					mt->mem_domain == HOST_MEM_DOMAIN_DRAM ? "DRAM" : "SRAM",
					(unsigned long long)mt->iterations,
					(unsigned long long)mt->avg_cycles,
					(unsigned long long)mt->peak_cycles,
					(unsigned long long)mt->total_cycles,
					mt->avg_kcps, mt->peak_kcps,
					(unsigned long long)(mt->avg_exec_ns / 1000ULL),
					(unsigned long long)(mt->avg_exec_ns % 1000ULL),
					(unsigned long long)(mt->peak_exec_ns / 1000ULL),
					(unsigned long long)(mt->peak_exec_ns % 1000ULL),
					(unsigned long long)(mt->last_start_ns / 1000ULL),
					(unsigned long long)(mt->last_start_ns % 1000ULL),
					(unsigned long long)(mt->last_complete_ns / 1000ULL),
					(unsigned long long)(mt->last_complete_ns % 1000ULL),
					(unsigned long long)(mt->last_deadline_ns / 1000ULL),
					(unsigned long long)(mt->last_deadline_ns % 1000ULL),
					mt->ibs, mt->obs, mt->effective_cpc, mt->cpc_violations);
		}

		if (tp->sched_trace_limit > 0 && st->trace_count > 0 && off + 512 < out_cap) {
			uint32_t max_ev = (uint32_t)tp->sched_trace_limit;
			struct host_sched_trace_event *ev_buf;
			uint64_t total_ev = st->trace_total_events;
			uint32_t avail_ev = st->trace_count;
			uint32_t n_ev, e;

			if (max_ev > TB_TELEM_TRACE_CAPACITY)
				max_ev = TB_TELEM_TRACE_CAPACITY;
			ev_buf = calloc(max_ev, sizeof(*ev_buf));
			if (ev_buf) {
				n_ev = host_telem_trace_read(ev_buf, max_ev, 0, -1, -1, -1, -1);
				off += snprintf(out_buf + off, out_cap - off,
						"\n--- Per-Iteration LL & DP Module Scheduling Timeline (showing %u of %llu events, ring=%u) ---\n"
						"  %-5s %-5s %-5s %-4s %-4s %-7s %-32s %12s %12s %10s %12s %21s %13s %11s\n",
						n_ev, (unsigned long long)total_ev, avail_ev,
						"Seq", "Tick", "Iter", "Core", "Pipe", "Domain",
						"Module", "Start(us)", "Complete(us)", "Dur(us)",
						"Deadline(us)", "Start->End(cyc)", "Cyc/Budget",
						"In->Out(B)");
				for (e = 0; e < n_ev && off + 224 < out_cap; e++) {
					struct host_sched_trace_event *ev = &ev_buf[e];
					const char *ename = resolve_comp_widget_name(tp, ev->comp_id);
					const char *dom =
						(ev->event_type == HOST_SCHED_EVT_DP_RUN) ? "DP-RUN" :
						(ev->event_type == HOST_SCHED_EVT_DP_SYNC) ? "DP-SYNC" :
											     "LL-COPY";
					uint64_t dur_ns = (ev->complete_ns >= ev->start_ns) ?
							  (ev->complete_ns - ev->start_ns) : 0;
					char mod_col[48];
					char cyc_col[48];
					char bud_col[32];
					char io_col[32];

					snprintf(mod_col, sizeof(mod_col), "0x%06x %.22s",
						 ev->comp_id, ename);
					snprintf(cyc_col, sizeof(cyc_col), "%llu->%llu",
						 (unsigned long long)ev->start_cycles,
						 (unsigned long long)ev->complete_cycles);
					snprintf(bud_col, sizeof(bud_col), "%u/%u%s",
						 ev->duration_cycles, ev->cpc_budget,
						 ev->cpc_violation ? "!" : "");
					snprintf(io_col, sizeof(io_col), "%u->%u",
						 ev->bytes_in, ev->bytes_out);

					off += snprintf(out_buf + off, out_cap - off,
							"  %-5u %-5u %-5u C%-3u P%-3u %-7s %-32s %8llu.%03llu %8llu.%03llu %6llu.%03llu %8llu.%03llu %21s %13s %11s\n",
							ev->seq_id, ev->ll_tick_nr, ev->mod_iter_nr,
							ev->core_id, ev->pipeline_id, dom, mod_col,
							(unsigned long long)(ev->start_ns / 1000ULL),
							(unsigned long long)(ev->start_ns % 1000ULL),
							(unsigned long long)(ev->complete_ns / 1000ULL),
							(unsigned long long)(ev->complete_ns % 1000ULL),
							(unsigned long long)(dur_ns / 1000ULL),
							(unsigned long long)(dur_ns % 1000ULL),
							(unsigned long long)(ev->deadline_ns / 1000ULL),
							(unsigned long long)(ev->deadline_ns % 1000ULL),
							cyc_col, bud_col, io_col);
				}
				free(ev_buf);
			}
		}
	}
	return off;
}

static size_t handle_remote_io_cfg(struct testbench_prm *tp, const char *args_str,
				   char *out_buf, size_t off, size_t out_cap)
{
	char *copy = strdup(args_str ? args_str : "");
	char *argv[64];
	int argc, i;

	if (!copy || !tp) {
		free(copy);
		return off + snprintf(out_buf + off, out_cap - off, "error: out of memory\n");
	}

	argc = tokenize_shell_args(copy, argv, 64);
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
			remote_set_csv_files(tp->input_file, &tp->input_file_num,
					     TB_MAX_INPUT_FILE_NUM, argv[++i]);
		} else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
			remote_set_csv_files(tp->output_file, &tp->output_file_num,
					     TB_MAX_OUTPUT_FILE_NUM, argv[++i]);
		} else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
			remote_set_csv_pipelines(tp, argv[++i]);
		} else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
			tp->fs_in = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-R") == 0 && i + 1 < argc) {
			tp->fs_out = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
			tp->channels_in = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
			tp->channels_out = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
			free(tp->bits_in);
			tp->bits_in = strdup(argv[++i]);
			tp->frame_fmt = tplg_find_format(tp->bits_in);
		} else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
			free(tp->control_file);
			tp->control_file = strdup(argv[++i]);
		} else if (strcmp(argv[i], "-C") == 0 && i + 1 < argc) {
			tp->copy_iterations = atoi(argv[++i]);
			tp->copy_check = true;
		}
	}
	free(copy);

	if (!tp->fs_in)
		tp->fs_in = 48000;
	if (!tp->fs_out)
		tp->fs_out = tp->fs_in;
	if (!tp->channels_in)
		tp->channels_in = 2;
	if (!tp->channels_out)
		tp->channels_out = tp->channels_in;
	tp->input_file_index = 0;
	tp->output_file_index = 0;
	pthread_mutex_lock(&g_pcm_ring_lock);
	g_pcm_ring_write_idx = 0;
	g_pcm_ring_avail = 0;
	g_pcm_frames_processed = 0;
	pthread_mutex_unlock(&g_pcm_ring_lock);
	tb_configure_audio_params(tp);

	off += snprintf(out_buf + off, out_cap - off,
			"io-cfg: in=%d out=%d pipes=%d rate=%d->%d ch=%d->%d fmt=%s\n",
			tp->input_file_num, tp->output_file_num, tp->pipeline_num,
			tp->fs_in, tp->fs_out, tp->channels_in, tp->channels_out,
			tp->bits_in ? tp->bits_in : "S32_LE");
	return off;
}

static bool remote_is_any_file_at_eof(struct testbench_prm *tp)
{
	int i;

	if (!tp)
		return true;
	for (i = 0; i < tp->input_file_num && i < TB_MAX_INPUT_FILE_NUM; i++) {
		if (!tp->fr[i].state)
			continue;
		if (tp->fr[i].state->reached_eof || tp->fr[i].state->copy_timeout)
			return true;
	}
	for (i = 0; i < tp->output_file_num && i < TB_MAX_OUTPUT_FILE_NUM; i++) {
		if (!tp->fw[i].state)
			continue;
		if (tp->fw[i].state->reached_eof || tp->fw[i].state->copy_timeout ||
		    tp->fw[i].state->write_failed)
			return true;
	}
	return false;
}

static size_t handle_remote_run_ipc(struct testbench_prm *tp, const char *args_str,
				    char *out_buf, size_t off, size_t out_cap)
{
	struct tb_heap_usage_record heap_records[TB_NUM_WIDGETS_SUPPORTED];
	struct timespec td0 = {0}, td1 = {0}, t0 = {0}, t1 = {0};
	long long delta_t = 0;
	int heap_records_count = 0;
	int max_ticks = 20000;
	int ticks_run = 0;
	int i;

	if (!tp)
		return off + snprintf(out_buf + off, out_cap - off,
				      "error: testbench not initialized\n");
	if (args_str && *args_str) {
		int v = atoi(args_str);

		if (v > 0)
			max_ticks = v;
	}

	off += snprintf(out_buf + off, out_cap - off,
			"==========================================================\n"
			"		           Test Start 0\n"
			"==========================================================\n");

	host_telem_reset();
	tb_gettime(&td0);
	while (ticks_run < max_ticks) {
		uint64_t c0, c1, delta_ns;

		tb_gettime(&t0);
		tb_getcycles(&c0);
		if (sof_get() && sof_get()->ipc &&
		    !list_is_empty(&sof_get()->ipc->comp_list))
			schedule_ll_run_tasks();
		tb_getcycles(&c1);
		tb_gettime(&t1);
		tp->total_cycles += c1 - c0;
		delta_ns = (uint64_t)(t1.tv_sec - t0.tv_sec) * 1000000000ULL +
			   (uint64_t)(t1.tv_nsec - t0.tv_nsec);
		g_in_remote_rpc = true;
		tb_remote_tick_end(tp, c1 - c0, delta_ns);
		g_in_remote_rpc = false;
		ticks_run++;

		if (remote_is_any_file_at_eof(tp)) {
			schedule_ll_run_tasks();
			break;
		}
	}
	tb_gettime(&td1);

	for (i = 0; i < tp->output_file_num && i < TB_MAX_OUTPUT_FILE_NUM; i++) {
		if (tp->fw[i].state && tp->fw[i].state->wfh)
			fflush(tp->fw[i].state->wfh);
	}

	delta_t = (td1.tv_sec - td0.tv_sec) * 1000000LL;
	delta_t += (td1.tv_nsec - td0.tv_nsec) / 1000LL;

	memset(heap_records, 0, sizeof(heap_records));
	tb_collect_heap_usage(tp, heap_records, &heap_records_count);

	return format_remote_test_summary(tp, delta_t, heap_records, heap_records_count,
					  out_buf, off, out_cap);
}

static size_t handle_remote_run_test(struct testbench_prm *tp, const char *args_str,
				     char *out_buf, size_t off, size_t out_cap)
{
	struct tb_heap_usage_record heap_records[TB_NUM_WIDGETS_SUPPORTED];
	struct host_sched_telemetry *st = host_telem_get();
	struct host_workload_sim_cfg saved_sim_cfg;
	struct host_platform_hw_cfg saved_hw_cfg;
	struct timespec td0 = {0}, td1 = {0};
	long long delta_t = 0;
	int64_t next_control_ns = 0;
	int64_t time_ns;
	int heap_records_count = 0;
	bool saved_server_wait;
	bool w_explicit_off = false;
	const char *opt_preset = NULL;
	const char *opt_cores = NULL;
	const char *opt_mem = NULL;
	const char *opt_freq = NULL;
	char *copy;
	char *argv[64];
	int argc, i, err = 0;

	if (!tp)
		return off + snprintf(out_buf + off, out_cap - off, "error: testbench not initialized\n");

	copy = strdup(args_str ? args_str : "");
	if (!copy)
		return off + snprintf(out_buf + off, out_cap - off, "error: out of memory\n");

	/* Ensure any prior topology or dynamic pipeline is cleanly torn down */
	remote_unload_active_topology(tp);

	saved_sim_cfg = st->sim_cfg;
	saved_hw_cfg = st->hw_cfg;
	saved_server_wait = tp->server_wait;
	tp->server_wait = false;
	host_platform_reset_mem_stats();

	/* Reset per-run defaults before parsing run-test flags */
	tp->fs_in = 48000;
	tp->fs_out = 0;
	tp->channels_in = 2;
	tp->channels_out = 0;
	free(tp->bits_in);
	tp->bits_in = strdup("S32_LE");
	tp->frame_fmt = tplg_find_format(tp->bits_in);
	tp->copy_check = false;
	tp->copy_iterations = 1;
	tp->sched_trace_limit = 0;
	tp->pipelines[0] = 1;
	tp->pipeline_num = 1;

	argc = tokenize_shell_args(copy, argv, 64);
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
			free(tp->tplg_file);
			tp->tplg_file = strdup(argv[++i]);
		} else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
			remote_set_csv_pipelines(tp, argv[++i]);
		} else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
			remote_set_csv_files(tp->input_file, &tp->input_file_num,
					     TB_MAX_INPUT_FILE_NUM, argv[++i]);
		} else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
			remote_set_csv_files(tp->output_file, &tp->output_file_num,
					     TB_MAX_OUTPUT_FILE_NUM, argv[++i]);
		} else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
			tp->fs_in = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-R") == 0 && i + 1 < argc) {
			tp->fs_out = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
			tp->channels_in = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
			tp->channels_out = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
			free(tp->bits_in);
			tp->bits_in = strdup(argv[++i]);
			tp->frame_fmt = tplg_find_format(tp->bits_in);
		} else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
			free(tp->control_file);
			tp->control_file = strdup(argv[++i]);
		} else if (strcmp(argv[i], "-C") == 0 && i + 1 < argc) {
			tp->copy_iterations = atoi(argv[++i]);
			tp->copy_check = true;
		} else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
			tp->trace_level = atoi(argv[++i]);
			tb_enable_trace(tp->trace_level);
		} else if (strcmp(argv[i], "-A") == 0 && i + 1 < argc) {
			opt_preset = argv[++i];
		} else if (strcmp(argv[i], "-N") == 0 && i + 1 < argc) {
			opt_cores = argv[++i];
		} else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
			opt_mem = argv[++i];
		} else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
			const char *warg = argv[++i];

			if (strcmp(warg, "0") == 0 || strcasecmp(warg, "off") == 0 ||
			    strcasecmp(warg, "disable") == 0) {
				host_workload_sim_set_mode(HOST_WORKLOAD_SIM_OFF);
				w_explicit_off = true;
			} else if (strcmp(warg, "2") == 0 || strcasecmp(warg, "realtime") == 0 ||
				   strcasecmp(warg, "pace") == 0) {
				host_workload_sim_set_mode(HOST_WORKLOAD_SIM_REALTIME);
				w_explicit_off = false;
			} else {
				host_workload_sim_set_mode(HOST_WORKLOAD_SIM_VIRTUAL);
				w_explicit_off = false;
			}
		} else if (strcmp(argv[i], "-F") == 0 && i + 1 < argc) {
			opt_freq = argv[++i];
		} else if (strcmp(argv[i], "-M") == 0 && i + 1 < argc) {
			remote_apply_mem_arg(argv[++i]);
			if (!w_explicit_off &&
			    host_workload_sim_get_mode() == HOST_WORKLOAD_SIM_OFF)
				host_workload_sim_set_mode(HOST_WORKLOAD_SIM_VIRTUAL);
		} else if (strcmp(argv[i], "-T") == 0 && i + 1 < argc) {
			tp->sched_trace_limit = atoi(argv[++i]);
			if (tp->sched_trace_limit <= 0)
				tp->sched_trace_limit = 64;
		}
	}

	if (opt_preset) {
		if (host_platform_apply_preset(opt_preset) < 0) {
			free(copy);
			st->sim_cfg = saved_sim_cfg;
			st->hw_cfg = saved_hw_cfg;
			tp->server_wait = saved_server_wait;
			return off + snprintf(out_buf + off, out_cap - off,
					      "error: unknown platform preset '%s'\n",
					      opt_preset);
		}
		if (!w_explicit_off && host_workload_sim_get_mode() == HOST_WORKLOAD_SIM_OFF)
			host_workload_sim_set_mode(HOST_WORKLOAD_SIM_VIRTUAL);
	}
	if (opt_cores) {
		int nc = atoi(opt_cores);

		if (nc < 1 || nc > TB_TELEM_MAX_CORES ||
		    host_platform_set_core_count((uint32_t)nc) < 0) {
			free(copy);
			st->sim_cfg = saved_sim_cfg;
			st->hw_cfg = saved_hw_cfg;
			tp->server_wait = saved_server_wait;
			return off + snprintf(out_buf + off, out_cap - off,
					      "error: invalid core count '%s' (expected 1..%d)\n",
					      opt_cores, TB_TELEM_MAX_CORES);
		}
	}
	if (opt_mem) {
		if (remote_apply_mem_cfg_arg(opt_mem) < 0) {
			free(copy);
			st->sim_cfg = saved_sim_cfg;
			st->hw_cfg = saved_hw_cfg;
			tp->server_wait = saved_server_wait;
			return off + snprintf(out_buf + off, out_cap - off,
					      "error: invalid memory config '%s'\n", opt_mem);
		}
	}
	if (opt_freq) {
		remote_apply_freq_arg(opt_freq);
		if (!w_explicit_off && host_workload_sim_get_mode() == HOST_WORKLOAD_SIM_OFF)
			host_workload_sim_set_mode(HOST_WORKLOAD_SIM_VIRTUAL);
	}
	free(copy);

	if (!tp->fs_out)
		tp->fs_out = tp->fs_in;
	if (!tp->channels_out)
		tp->channels_out = tp->channels_in;

	if (!tp->tplg_file) {
		st->sim_cfg = saved_sim_cfg;
		st->hw_cfg = saved_hw_cfg;
		st->num_cores = saved_hw_cfg.num_cores;
		tp->server_wait = saved_server_wait;
		return off + snprintf(out_buf + off, out_cap - off,
				      "error: run-test requires -t <file.tplg>\n");
	}

	off += snprintf(out_buf + off, out_cap - off,
			"==========================================================\n"
			"		           Test Start 0\n"
			"==========================================================\n");

	host_telem_reset();

	err = tb_load_topology(tp);
	if (err < 0) {
		off += snprintf(out_buf + off, out_cap - off,
				"error: topology load 0 failed %d\n", err);
		goto cleanup;
	}
	cache_active_widget_names(tp);

	err = tb_set_up_all_pipelines(tp);
	if (err < 0) {
		off += snprintf(out_buf + off, out_cap - off,
				"error: pipelines set up 0 failed %d\n", err);
		goto cleanup;
	}

	err = tb_set_running_state(tp);
	if (err < 0) {
		off += snprintf(out_buf + off, out_cap - off,
				"error: pipelines state set 0 failed %d\n", err);
		goto cleanup;
	}

	err = tb_find_file_components(tp);
	if (err < 0) {
		off += snprintf(out_buf + off, out_cap - off,
				"error: file component find failed %d\n", err);
		goto cleanup;
	}

	/* Record active secondary core LL worker and DP module thread creation lines */
	for (i = 1; i < CONFIG_CORE_COUNT && off + 128 < out_cap; i++) {
		if (arch_cpu_enabled_cores() & BIT(i))
			off += snprintf(out_buf + off, out_cap - off,
					"LL worker thread created for DSP core %d (host CPU %d)\n",
					i, i);
	}
	if (tp->widget_list.next) {
		struct list_item *item;

		list_for_item(item, &tp->widget_list) {
			struct tplg_comp_info *ci = container_of(item, struct tplg_comp_info, item);

			if (off + 160 >= out_cap)
				break;
			if (ci->proc_domain == 1 && tb_is_pipeline_enabled(tp, ci->pipeline_id))
				off += snprintf(out_buf + off, out_cap - off,
						"DP thread initialized for module %s (0x%x) on DSP core %d (host CPU %d)\n",
						ci->name ? ci->name : "dp_mod",
						(uint32_t)IPC4_COMP_ID(ci->module_id, ci->instance_id),
						ci->core_id, ci->core_id);
		}
	}

	if (!tp->fw[0].state) {
		off += snprintf(out_buf + off, out_cap - off,
				"error: no active output file component found\n");
		goto cleanup;
	}
	tp->has_output_file = true;
	tp->last_output_time_ns = 0;

	if (tp->control_file) {
		FILE *cfh = fopen(tp->control_file, "r");

		if (!cfh) {
			off += snprintf(out_buf + off, out_cap - off,
					"error: opening script %s (%s).\n",
					tp->control_file, strerror(errno));
			goto cleanup;
		}
		char cline[512];

		while (fgets(cline, sizeof(cline), cfh) && off + 192 < out_cap) {
			char *nl = strpbrk(cline, "\r\n");

			if (nl)
				*nl = '\0';
			if (cline[0] != '#' && cline[0] != '\0')
				off += snprintf(out_buf + off, out_cap - off,
						"Info: Setting control name from '%s'\n", cline);
		}
		rewind(cfh);
		tp->control_fh = cfh;
	}

	err = tb_read_controls(tp, &next_control_ns);
	if (err) {
		off += snprintf(out_buf + off, out_cap - off,
				"error: failed to read control commands.\n");
		goto cleanup;
	}

	tb_gettime(&td0);
	while (true) {
		if (tp->copy_check) {
			if (tp->copy_iterations-- <= 0)
				break;
		}
		if (tb_schedule_pipeline_check_state(tp))
			break;
		if (next_control_ns && tp->has_output_file) {
			time_ns = tp->last_output_time_ns;
			if (time_ns >= next_control_ns) {
				err = tb_read_controls(tp, &next_control_ns);
				if (err) {
					off += snprintf(out_buf + off, out_cap - off,
							"error: failed to read control commands.\n");
					goto cleanup;
				}
				if (next_control_ns)
					next_control_ns += time_ns;
			}
		}
	}
	tb_schedule_pipeline_check_state(tp);
	tb_gettime(&td1);

	delta_t = (td1.tv_sec - td0.tv_sec) * 1000000LL;
	delta_t += (td1.tv_nsec - td0.tv_nsec) / 1000LL;

	memset(heap_records, 0, sizeof(heap_records));
	tb_collect_heap_usage(tp, heap_records, &heap_records_count);

	off = format_remote_test_summary(tp, delta_t, heap_records, heap_records_count,
					 out_buf, off, out_cap);

	err = tb_set_reset_state(tp);
	if (err < 0) {
		off += snprintf(out_buf + off, out_cap - off,
				"error: pipeline reset 0 failed %d\n", err);
		goto cleanup;
	}

cleanup:
	remote_unload_active_topology(tp);
	st->sim_cfg = saved_sim_cfg;
	st->hw_cfg = saved_hw_cfg;
	st->num_cores = saved_hw_cfg.num_cores;
	tp->server_wait = saved_server_wait;
	return off;
}

static int execute_tb_shell_cmd(const char *cmd, char *out_buf, size_t out_cap)
{
	size_t off = 0;

	if (strncmp(cmd, "host:", 5) == 0) {
		FILE *fp = popen(cmd + 5, "r");

		if (!fp)
			return -errno;
		while (off + 1 < out_cap) {
			size_t n = fread(out_buf + off, 1, out_cap - 1 - off, fp);

			if (n == 0)
				break;
			off += n;
		}
		out_buf[off] = '\0';
		pclose(fp);
		return (int)off;
	}

	pthread_mutex_lock(&g_tb_lock);

	while (*cmd == ' ' || *cmd == '\t')
		cmd++;

	if (strcmp(cmd, "help") == 0) {
		off += snprintf(out_buf + off, out_cap - off,
				"SOF Testbench Remote Shell Commands:\n"
				"  help                         Show this help\n"
				"  version | info               Show testbench version, platform, topology & core state\n"
				"  platform-cfg [status|...]    Configure/query platform preset, core count, memory & freq\n"
				"  pipelines | widgets          List active IPC pipelines, components & widgets\n"
				"  modules                      List loaded built-in & dynamic LLEXT modules\n"
				"  io-cfg [-i .. -o .. -r ..]   Configure input/output files, rates, channels & format\n"
				"  tplg-load <file> [p1,p2,...] Dynamically load topology and start pipelines\n"
				"  tplg-unload                  Stop, reset and free active topology & pipelines\n"
				"  run-test -t <file> [...]     Run full topology test to EOF and return summary\n"
				"  ppl-create <id> [prio] [core] Dynamically create an IPC4 pipeline\n"
				"  ppl-state <id> <run|pause|reset> Set IPC4 pipeline state\n"
				"  ppl-delete <id>              Delete an IPC4 pipeline\n"
				"  mod-init <m> <i> <ppl> [c] [d] Initialize an IPC4 module instance\n"
				"  mod-bind <sm> <si> <sq> <dm> <di> <dq>   Bind two module instances\n"
				"  mod-unbind <sm> <si> <sq> <dm> <di> <dq> Unbind two module instances\n"
				"  mod-delete <mod_id> <inst_id> Delete a module instance\n"
				"  controls                     List topology ALSA kcontrols\n"
				"  amixer cset name=\"...\" <val> Set mixer or enum control live\n"
				"  sof-ctl -c name=\"...\" -s <f> Apply bytes control blob live\n"
				"  stats | heap                 Show file I/O stats, platform memory & module heap usage\n"
				"  sched-telem                  Show per-core, per-pipeline & per-module LL/DP telemetry\n"
				"  sched-telem-reset            Reset scheduling telemetry counters\n"
				"  workload-sim [status|...]    Configure/query CPC, core freq & SRAM/DRAM workload sim\n"
				"  mod-buffers [id|name]        Show module entry/exit buffer levels, dBFS & sample taps\n"
				"  kernel cycles                Show simulated DSP core tick & cycle counters\n"
				"  kernel threads               Show active LL core & DP module worker threads\n"
				"  pause | resume | step [N]    Pause, resume, or step N pipeline ticks\n"
				"  loglevel <0..4>              Set trace print verbosity level\n"
				"  quit                         Stop testbench simulation\n");
	} else if (strcmp(cmd, "version") == 0 || strcmp(cmd, "info") == 0) {
		struct host_sched_telemetry *st = host_telem_get();

		off += snprintf(out_buf + off, out_cap - off,
				"SOF Testbench v%d.%d.%d.%d (IPC4)\n"
				"Platform: %s (%s)\n"
				"Topology: %s\n"
				"Enabled DSP Cores Mask: 0x%x (%u configured cores, max %d, C0=%u.%03u MHz)\n"
				"Memory: HPSRAM=%zu/%zu KB (peak %zu KB), LPSRAM=%zu/%zu KB, DRAM=%zu/%zu KB\n"
				"State: %s, Total Ticks: %llu, Tone Generator: %s (%u Hz)\n",
				SOF_MAJOR, SOF_MINOR, SOF_MICRO, SOF_BUILD,
				st->hw_cfg.platform_name[0] ? st->hw_cfg.platform_name : "default",
				st->hw_cfg.arch_desc[0] ? st->hw_cfg.arch_desc : "Generic Host Testbench",
				(g_tp && g_tp->tplg_file) ? g_tp->tplg_file : "none",
				arch_cpu_enabled_cores(), st->hw_cfg.num_cores, CONFIG_CORE_COUNT,
				st->sim_cfg.core_freq_hz[0] / 1000000U,
				(st->sim_cfg.core_freq_hz[0] % 1000000U) / 1000U,
				st->hw_cfg.hpsram_used_bytes / 1024UL,
				st->hw_cfg.hpsram_size_bytes / 1024UL,
				st->hw_cfg.hpsram_peak_bytes / 1024UL,
				st->hw_cfg.lpsram_used_bytes / 1024UL,
				st->hw_cfg.lpsram_size_bytes / 1024UL,
				st->hw_cfg.dram_used_bytes / 1024UL,
				st->hw_cfg.dram_size_bytes / 1024UL,
				g_dsp_paused ? "PAUSED (D3)" : "RUNNING (D0)",
				(unsigned long long)g_total_ticks,
				g_tone_running ? "ON" : "OFF", g_tone_freq_hz);
	} else if (strncmp(cmd, "platform-cfg", 12) == 0 || strncmp(cmd, "hw-cfg", 6) == 0) {
		struct host_sched_telemetry *st = host_telem_get();
		const char *sub = cmd + (strncmp(cmd, "platform-cfg", 12) == 0 ? 12 : 6);

		while (*sub == ' ' || *sub == '\t')
			sub++;

		if (*sub == '\0' || strcmp(sub, "status") == 0) {
			uint32_t c;

			off += snprintf(out_buf + off, out_cap - off,
					"=== Platform Hardware Configuration ===\n"
					"  Platform Preset:   %s (%s)\n"
					"  HW IP Version:     0x%x\n"
					"  Configured Cores:  %u (enabled mask: 0x%x, max supported: %d)\n"
					"  HPSRAM Capacity:   %zu KB (%zu bytes) | used=%zu B peak=%zu B (%u EBB banks)\n"
					"  LPSRAM Capacity:   %zu KB (%zu bytes) | used=%zu B peak=%zu B (%u EBB banks)\n"
					"  DRAM/IMR Capacity: %zu KB (%zu bytes) | used=%zu B peak=%zu B\n"
					"  Heap Allocations:  %llu total (%llu frees), %u OOM failures\n",
					st->hw_cfg.platform_name[0] ? st->hw_cfg.platform_name : "default",
					st->hw_cfg.arch_desc[0] ? st->hw_cfg.arch_desc : "Generic Host Testbench",
					st->hw_cfg.hw_ip_version,
					st->hw_cfg.num_cores, arch_cpu_enabled_cores(), CONFIG_CORE_COUNT,
					st->hw_cfg.hpsram_size_bytes / 1024UL, st->hw_cfg.hpsram_size_bytes,
					st->hw_cfg.hpsram_used_bytes, st->hw_cfg.hpsram_peak_bytes,
					(uint32_t)(st->hw_cfg.hpsram_size_bytes / 65536UL),
					st->hw_cfg.lpsram_size_bytes / 1024UL, st->hw_cfg.lpsram_size_bytes,
					st->hw_cfg.lpsram_used_bytes, st->hw_cfg.lpsram_peak_bytes,
					(uint32_t)(st->hw_cfg.lpsram_size_bytes / 65536UL),
					st->hw_cfg.dram_size_bytes / 1024UL, st->hw_cfg.dram_size_bytes,
					st->hw_cfg.dram_used_bytes, st->hw_cfg.dram_peak_bytes,
					(unsigned long long)st->hw_cfg.total_alloc_calls,
					(unsigned long long)st->hw_cfg.total_free_calls,
					st->hw_cfg.alloc_failures);
			for (c = 0; c < st->hw_cfg.num_cores && c < TB_TELEM_MAX_CORES && off + 128 < out_cap; c++) {
				off += snprintf(out_buf + off, out_cap - off,
						"  [Core %u] enabled=%u freq=%u.%03u MHz (%u Hz, budget=%u cyc/ms)\n",
						c, (st->hw_cfg.enabled_cores_mask & BIT(c)) ? 1 : 0,
						st->sim_cfg.core_freq_hz[c] / 1000000U,
						(st->sim_cfg.core_freq_hz[c] % 1000000U) / 1000U,
						st->sim_cfg.core_freq_hz[c],
						st->sim_cfg.core_freq_hz[c] / 1000U);
			}
		} else if (strcmp(sub, "presets") == 0 || strcmp(sub, "list") == 0) {
			size_t count = 0, i;
			const struct host_platform_preset *presets = host_platform_get_presets(&count);

			off += snprintf(out_buf + off, out_cap - off,
					"=== Supported SOF Hardware Platform Presets ===\n"
					"  %-8s %-14s %-5s %-11s %-10s %-10s %-10s %s\n",
					"Preset", "Aliases", "Cores", "Clock", "HPSRAM", "LPSRAM", "DRAM/IMR", "Description");
			for (i = 0; i < count && off + 192 < out_cap; i++) {
				off += snprintf(out_buf + off, out_cap - off,
						"  %-8s %-14s %5u %4u.%03uMHz %7zu KB %7zu KB %7zu MB %s\n",
						presets[i].name,
						presets[i].aliases ? presets[i].aliases : "-",
						presets[i].num_cores,
						presets[i].freq_hz / 1000000U,
						(presets[i].freq_hz % 1000000U) / 1000U,
						presets[i].hpsram_size_bytes / 1024UL,
						presets[i].lpsram_size_bytes / 1024UL,
						presets[i].dram_size_bytes / (1024UL * 1024UL),
						presets[i].desc);
			}
		} else if (strncmp(sub, "preset ", 7) == 0) {
			const char *pname = sub + 7;

			while (*pname == ' ' || *pname == '\t')
				pname++;
			if (host_platform_apply_preset(pname) < 0) {
				off += snprintf(out_buf + off, out_cap - off,
						"Unknown platform preset '%s' (try 'platform-cfg presets')\n",
						pname);
			} else {
				host_telem_reset();
				off += snprintf(out_buf + off, out_cap - off,
						"Applied platform preset '%s' (%s): %u cores @ %u.%03u MHz, HPSRAM=%zu KB, LPSRAM=%zu KB, DRAM=%zu KB\n",
						st->hw_cfg.platform_name, st->hw_cfg.arch_desc,
						st->hw_cfg.num_cores,
						st->sim_cfg.core_freq_hz[0] / 1000000U,
						(st->sim_cfg.core_freq_hz[0] % 1000000U) / 1000U,
						st->hw_cfg.hpsram_size_bytes / 1024UL,
						st->hw_cfg.lpsram_size_bytes / 1024UL,
						st->hw_cfg.dram_size_bytes / 1024UL);
			}
		} else if (strncmp(sub, "cores ", 6) == 0) {
			uint32_t ncores = (uint32_t)strtoul(sub + 6, NULL, 10);

			if (host_platform_set_core_count(ncores) < 0) {
				off += snprintf(out_buf + off, out_cap - off,
						"Invalid core count %u (supported: 1..%d)\n",
						ncores, TB_TELEM_MAX_CORES);
			} else {
				host_telem_reset();
				off += snprintf(out_buf + off, out_cap - off,
						"Configured DSP core count set to %u (enabled mask: 0x%x)\n",
						st->hw_cfg.num_cores, arch_cpu_enabled_cores());
			}
		} else if (strncmp(sub, "mem ", 4) == 0) {
			const char *marg = sub + 4;

			while (*marg == ' ' || *marg == '\t')
				marg++;
			if (remote_apply_mem_cfg_arg(marg) < 0) {
				off += snprintf(out_buf + off, out_cap - off,
						"Invalid memory spec '%s' (expected <hpsram[,lpsram[,dram]]> e.g. 2816K,64K,16M)\n",
						marg);
			} else {
				off += snprintf(out_buf + off, out_cap - off,
						"Configured memory limits: HPSRAM=%zu KB, LPSRAM=%zu KB, DRAM=%zu KB\n",
						st->hw_cfg.hpsram_size_bytes / 1024UL,
						st->hw_cfg.lpsram_size_bytes / 1024UL,
						st->hw_cfg.dram_size_bytes / 1024UL);
			}
		} else if (strncmp(sub, "freq ", 5) == 0) {
			const char *farg = sub + 5;

			while (*farg == ' ' || *farg == '\t')
				farg++;
			remote_apply_freq_arg(farg);
			host_telem_reset();
			off += snprintf(out_buf + off, out_cap - off,
					"Configured core frequencies: C0=%u.%03u MHz C1=%u.%03u MHz C2=%u.%03u MHz C3=%u.%03u MHz\n",
					st->sim_cfg.core_freq_hz[0] / 1000000U,
					(st->sim_cfg.core_freq_hz[0] % 1000000U) / 1000U,
					st->sim_cfg.core_freq_hz[1] / 1000000U,
					(st->sim_cfg.core_freq_hz[1] % 1000000U) / 1000U,
					st->sim_cfg.core_freq_hz[2] / 1000000U,
					(st->sim_cfg.core_freq_hz[2] % 1000000U) / 1000U,
					st->sim_cfg.core_freq_hz[3] / 1000000U,
					(st->sim_cfg.core_freq_hz[3] % 1000000U) / 1000U);
		} else {
			off += snprintf(out_buf + off, out_cap - off,
					"Usage:\n"
					"  platform-cfg status\n"
					"  platform-cfg presets\n"
					"  platform-cfg preset <tgl|tgl-h|mtl|arl|lnl|ptl|wcl|nvl|nvl-s|default>\n"
					"  platform-cfg cores <1..%d>\n"
					"  platform-cfg mem <hpsram[,lpsram[,dram]]> (e.g. 2816K,64K,16M)\n"
					"  platform-cfg freq <mhz_or_hz | core:freq,...> (e.g. 393.216M or 0:442.368M)\n",
					TB_TELEM_MAX_CORES);
		}
	} else if (strcmp(cmd, "pipelines") == 0 || strcmp(cmd, "widgets") == 0) {
		struct ipc *ipc = sof_get() ? sof_get()->ipc : NULL;

		if (ipc) {
			struct list_item *clist;

			off += snprintf(out_buf + off, out_cap - off,
					"=== Active IPC Pipelines & Components ===\n");
			list_for_item(clist, &ipc->comp_list) {
				struct ipc_comp_dev *icd =
					container_of(clist, struct ipc_comp_dev, list);

				if (off + 128 >= out_cap)
					break;
				if (icd->type == COMP_TYPE_PIPELINE && icd->pipeline) {
					off += snprintf(out_buf + off, out_cap - off,
							"  [PIPELINE] id=0x%06x pipe_id=%u core=%u prio=%u status=%u\n",
							icd->id, icd->pipeline->pipeline_id,
							icd->core, icd->pipeline->priority,
							icd->pipeline->status);
				} else if (icd->type == COMP_TYPE_COMPONENT && icd->cd) {
					struct tplg_comp_info *ci =
						find_comp_info_by_ipc_id(g_tp, icd->id);

					off += snprintf(out_buf + off, out_cap - off,
							"  [COMP    ] id=0x%06x mod=0x%02x inst=%2u core=%u %s state=%u name=%s\n",
							icd->id,
							IPC4_MOD_ID(icd->id),
							IPC4_INST_ID(icd->id),
							icd->core,
							icd->cd->ipc_config.proc_domain ==
								COMP_PROCESSING_DOMAIN_DP ? "DP" : "LL",
							icd->cd->state,
							(ci && ci->name) ? ci->name : "dynamic_mod");
				}
			}
		}
		off += snprintf(out_buf + off, out_cap - off,
				"=== Loaded Topology Widgets & Pipelines ===\n");
		if (g_tp && g_tp->widget_list.next) {
			struct list_item *item;

			list_for_item(item, &g_tp->widget_list) {
				struct tplg_comp_info *ci =
					container_of(item, struct tplg_comp_info, item);

				if (off + 128 >= out_cap)
					break;
				if (!tb_is_pipeline_enabled(g_tp, ci->pipeline_id))
					continue;
				off += snprintf(out_buf + off, out_cap - off,
						"  [pipe %2d | core %d | %s] id=0x%06x mod=0x%02x inst=%2d %-32s\n",
						ci->pipeline_id, ci->core_id,
						ci->proc_domain == 1 ? "DP" : "LL",
						(uint32_t)IPC4_COMP_ID(ci->module_id,
								       ci->instance_id),
						ci->module_id, ci->instance_id,
						ci->name ? ci->name : "unnamed");
			}
		} else if (g_tp && g_tp->info) {
			for (int i = 0; i < g_tp->info_elems && off + 128 < out_cap; i++) {
				struct tplg_comp_info *ci = &g_tp->info[i];

				if (!tb_is_pipeline_enabled(g_tp, ci->pipeline_id))
					continue;
				off += snprintf(out_buf + off, out_cap - off,
						"  [pipe %2d | core %d | %s] id=0x%06x mod=0x%02x inst=%2d %-32s\n",
						ci->pipeline_id, ci->core_id,
						ci->proc_domain == 1 ? "DP" : "LL",
						(uint32_t)IPC4_COMP_ID(ci->module_id,
								       ci->instance_id),
						ci->module_id, ci->instance_id,
						ci->name ? ci->name : "unnamed");
			}
		}
	} else if (strcmp(cmd, "modules") == 0) {
		off += snprintf(out_buf + off, out_cap - off,
				"=== Dynamic LLEXT / Shared Library Modules ===\n");
		int lcount = 0;

		for (int i = 0; i < TB_MAX_LOADED_LLEXTS && off + 128 < out_cap; i++) {
			if (!g_loaded_llexts[i].active)
				continue;
			lcount++;
			off += snprintf(out_buf + off, out_cap - off,
					"  [lib_id=%u] name=%s size=%u path=%s dl=%s\n",
					g_loaded_llexts[i].lib_id,
					g_loaded_llexts[i].name,
					g_loaded_llexts[i].data_size,
					g_loaded_llexts[i].path,
					g_loaded_llexts[i].dl_handle ? "loaded" : "registered");
		}
		if (lcount == 0)
			off += snprintf(out_buf + off, out_cap - off, "  (none loaded via llext-load)\n");
	} else if (strncmp(cmd, "io-cfg", 6) == 0) {
		off = handle_remote_io_cfg(g_tp, cmd + 6, out_buf, off, out_cap);
	} else if (strcmp(cmd, "tplg-unload") == 0) {
		remote_unload_active_topology(g_tp);
		off += snprintf(out_buf + off, out_cap - off,
				"tplg-unload -> OK (all pipelines and topology freed)\n");
	} else if (strncmp(cmd, "run-ipc", 7) == 0) {
		off = handle_remote_run_ipc(g_tp, cmd + 7, out_buf, off, out_cap);
	} else if (strncmp(cmd, "run-test", 8) == 0) {
		off = handle_remote_run_test(g_tp, cmd + 8, out_buf, off, out_cap);
	} else if (strncmp(cmd, "tplg-load ", 10) == 0) {
		char tpath[256] = {0};
		char plist[128] = {0};
		int n = sscanf(cmd + 10, "%255s %127s", tpath, plist);

		if (n < 1 || !g_tp) {
			off += snprintf(out_buf + off, out_cap - off,
					"Usage: tplg-load <file.tplg> [p1,p2,...]\n");
		} else {
			remote_unload_active_topology(g_tp);
			g_tp->tplg_file = strdup(tpath);
			if (n >= 2)
				remote_set_csv_pipelines(g_tp, plist);
			int err = tb_load_topology(g_tp);

			if (!err) {
				cache_active_widget_names(g_tp);
				err = tb_set_up_all_pipelines(g_tp);
			}
			if (!err)
				err = tb_set_running_state(g_tp);
			if (!err)
				err = tb_find_file_components(g_tp);
			if (!err)
				pthread_cond_broadcast(&g_tb_cond);
			off += snprintf(out_buf + off, out_cap - off,
					"tplg-load '%s' -> %s (err=%d)\n",
					tpath, err == 0 ? "OK" : "FAILED", err);
		}
	} else if (strncmp(cmd, "ppl-create ", 11) == 0) {
		uint32_t inst_id = 0, prio = 0, core = 0;
		int n = sscanf(cmd + 11, "%u %u %u", &inst_id, &prio, &core);

		if (n < 1 || !sof_get() || !sof_get()->ipc || !sof_get()->ipc->comp_data) {
			off += snprintf(out_buf + off, out_cap - off,
					"Usage: ppl-create <instance_id> [priority] [core_id]\n");
		} else {
			struct ipc4_pipeline_create msg = {{0}};
			uint32_t *mbox = (uint32_t *)sof_get()->ipc->comp_data;

			msg.primary.r.type = SOF_IPC4_GLB_CREATE_PIPELINE;
			msg.primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;
			msg.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
			msg.primary.r.instance_id = inst_id;
			msg.primary.r.ppl_priority = prio;
			msg.primary.r.ppl_mem_size = 16;
			msg.extension.r.core_id = core;

			if (core < (uint32_t)arch_cpu_num_cores())
				cpu_enable_core(core);

			memcpy(mbox, &msg, sizeof(msg));
			ipc_cmd((struct ipc_cmd_hdr *)mbox);
			uint32_t st = mbox[0] & 0x00FFFFFFU;

			pthread_cond_broadcast(&g_tb_cond);
			off += snprintf(out_buf + off, out_cap - off,
					"CREATE_PIPELINE(inst=%u, prio=%u, core=%u) -> status=%u (%s)\n",
					inst_id, prio, core, st, st == 0 ? "IPC4_SUCCESS" : "FAILED");
		}
	} else if (strncmp(cmd, "mod-init ", 9) == 0) {
		uint32_t mid = 0, iid = 0, ppl_id = 0, core = 0, dom = 0;
		int n = sscanf(cmd + 9, "%i %i %i %i %i", &mid, &iid, &ppl_id, &core, &dom);

		if (n < 3 || !sof_get() || !sof_get()->ipc || !sof_get()->ipc->comp_data) {
			off += snprintf(out_buf + off, out_cap - off,
					"Usage: mod-init <mod_id> <inst_id> <ppl_id> [core_id] [domain(0=LL,1=DP)]\n");
		} else {
			struct {
				struct ipc4_base_module_cfg base_cfg;
				struct sof_uuid uuid;
			} init_payload;
			struct ipc4_module_init_instance msg = {{0}};
			uint32_t *mbox = (uint32_t *)sof_get()->ipc->comp_data;
			/* Default to mixin UUID (39656eb2-3b71-4049-8d3f-f92cd5c43c09) or mixout (0x3) */
			static const struct sof_uuid mixin_uuid = {
				.a = 0x39656eb2, .b = 0x3b71, .c = 0x4049,
				.d = { 0x8d, 0x3f, 0xf9, 0x2c, 0xd5, 0xc4, 0x3c, 0x09 }
			};
			static const struct sof_uuid mixout_uuid = {
				.a = 0x3c56505a, .b = 0x24d7, .c = 0x418f,
				.d = { 0xbd, 0xdc, 0xc1, 0xf5, 0xa3, 0xac, 0x2a, 0xe0 }
			};

			if (core < (uint32_t)arch_cpu_num_cores())
				cpu_enable_core(core);

			memset(&init_payload, 0, sizeof(init_payload));
			init_payload.base_cfg.cpc = 1000;
			init_payload.base_cfg.ibs = 384;
			init_payload.base_cfg.obs = 384;
			init_payload.base_cfg.is_pages = 1;
			init_payload.base_cfg.audio_fmt.sampling_frequency = 48000;
			init_payload.base_cfg.audio_fmt.depth = 32;
			init_payload.base_cfg.audio_fmt.ch_map = 0xFFFFFF10U;
			init_payload.base_cfg.audio_fmt.ch_cfg = 1; /* STEREO */
			init_payload.base_cfg.audio_fmt.interleaving_style = 0;
			init_payload.base_cfg.audio_fmt.channels_count = 2;
			init_payload.base_cfg.audio_fmt.valid_bit_depth = 32;
			init_payload.base_cfg.audio_fmt.s_type = 0;
			init_payload.uuid = (mid == 3) ? mixout_uuid : mixin_uuid;

			if (g_tp && g_tp->widget_list.next) {
				struct list_item *it;

				list_for_item(it, &g_tp->widget_list) {
					struct tplg_comp_info *ci =
						container_of(it, struct tplg_comp_info, item);
					if ((uint32_t)ci->module_id == mid) {
						init_payload.uuid = ci->uuid;
						break;
					}
				}
			}

			msg.primary.r.type = SOF_IPC4_MOD_INIT_INSTANCE;
			msg.primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_MODULE_MSG;
			msg.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
			msg.primary.r.module_id = mid;
			msg.primary.r.instance_id = iid;
			msg.extension.r.param_block_size = sizeof(init_payload) / sizeof(uint32_t);
			msg.extension.r.ppl_instance_id = ppl_id;
			msg.extension.r.core_id = core;
			msg.extension.r.proc_domain = dom ? 1 : 0;

			memset(mbox, 0, SOF_IPC_MSG_MAX_SIZE);
			mbox[0] = msg.primary.dat;
			mbox[1] = msg.extension.dat;
			memcpy(&mbox[2], &init_payload, sizeof(init_payload));
			ipc_cmd((struct ipc_cmd_hdr *)mbox);
			uint32_t st = mbox[0] & 0x00FFFFFFU;

			off += snprintf(out_buf + off, out_cap - off,
					"MOD_INIT_INSTANCE(mod=0x%x, inst=%u, ppl=%u, core=%u, dom=%s) -> status=%u (%s)\n",
					mid, iid, ppl_id, core, dom ? "DP" : "LL", st,
					st == 0 ? "IPC4_SUCCESS" : "FAILED");
		}
	} else if (strncmp(cmd, "ppl-state ", 10) == 0) {
		uint32_t inst_id = 0;
		char state_str[32] = {0};

		if (sscanf(cmd + 10, "%u %31s", &inst_id, state_str) < 2 ||
		    !sof_get() || !sof_get()->ipc || !sof_get()->ipc->comp_data) {
			off += snprintf(out_buf + off, out_cap - off,
					"Usage: ppl-state <instance_id> <running|paused|reset>\n");
		} else {
			uint32_t pstate = SOF_IPC4_PIPELINE_STATE_PAUSED;

			if (strncmp(state_str, "run", 3) == 0 || strcmp(state_str, "2") == 0)
				pstate = SOF_IPC4_PIPELINE_STATE_RUNNING;
			else if (strncmp(state_str, "pause", 5) == 0 || strcmp(state_str, "3") == 0)
				pstate = SOF_IPC4_PIPELINE_STATE_PAUSED;
			else if (strncmp(state_str, "reset", 5) == 0 || strcmp(state_str, "1") == 0)
				pstate = SOF_IPC4_PIPELINE_STATE_RESET;

			struct ipc4_pipeline_set_state msg = {{0}};
			uint32_t *mbox = (uint32_t *)sof_get()->ipc->comp_data;

			msg.primary.r.type = SOF_IPC4_GLB_SET_PIPELINE_STATE;
			msg.primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;
			msg.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
			msg.primary.r.ppl_state = pstate;
			msg.primary.r.ppl_id = inst_id;
			msg.extension.r.multi_ppl = 0;
			mbox[0] = msg.primary.dat;
			mbox[1] = msg.extension.dat;
			ipc_cmd((struct ipc_cmd_hdr *)mbox);
			uint32_t st = mbox[0] & 0x00FFFFFFU;

			schedule_ll_run_tasks();
			off += snprintf(out_buf + off, out_cap - off,
					"SET_PIPELINE_STATE(inst=%u, state=%u) -> status=%u (%s)\n",
					inst_id, pstate, st, st == 0 ? "IPC4_SUCCESS" : "FAILED");
		}
	} else if (strncmp(cmd, "ppl-delete ", 11) == 0) {
		uint32_t inst_id = (uint32_t)atoi(cmd + 11);

		if (!sof_get() || !sof_get()->ipc || !sof_get()->ipc->comp_data) {
			off += snprintf(out_buf + off, out_cap - off, "Error: IPC not ready\n");
		} else {
			struct ipc4_pipeline_delete msg = {{0}};
			uint32_t *mbox = (uint32_t *)sof_get()->ipc->comp_data;

			msg.primary.r.type = SOF_IPC4_GLB_DELETE_PIPELINE;
			msg.primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;
			msg.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
			msg.primary.r.instance_id = inst_id;
			memcpy(mbox, &msg, sizeof(msg));
			ipc_cmd((struct ipc_cmd_hdr *)mbox);
			uint32_t st = mbox[0] & 0x00FFFFFFU;

			off += snprintf(out_buf + off, out_cap - off,
					"DELETE_PIPELINE(inst=%u) -> status=%u (%s)\n",
					inst_id, st, st == 0 ? "IPC4_SUCCESS" : "FAILED");
		}
	} else if (strncmp(cmd, "mod-bind ", 9) == 0 || strncmp(cmd, "mod-unbind ", 11) == 0) {
		bool is_unbind = strncmp(cmd, "mod-unbind ", 11) == 0;
		const char *args = cmd + (is_unbind ? 11 : 9);
		uint32_t sm = 0, si = 0, sq = 0, dm = 0, di = 0, dq = 0;

		if (sscanf(args, "%i %i %i %i %i %i", &sm, &si, &sq, &dm, &di, &dq) < 6 ||
		    !sof_get() || !sof_get()->ipc || !sof_get()->ipc->comp_data) {
			off += snprintf(out_buf + off, out_cap - off,
					"Usage: %s <src_mod> <src_inst> <src_q> <dst_mod> <dst_inst> <dst_q>\n",
					is_unbind ? "mod-unbind" : "mod-bind");
		} else {
			struct ipc4_module_bind_unbind bu = {{0}};
			uint32_t *mbox = (uint32_t *)sof_get()->ipc->comp_data;

			bu.primary.r.module_id = sm;
			bu.primary.r.instance_id = si;
			bu.primary.r.type = is_unbind ? SOF_IPC4_MOD_UNBIND : SOF_IPC4_MOD_BIND;
			bu.primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_MODULE_MSG;
			bu.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
			bu.extension.r.dst_module_id = dm;
			bu.extension.r.dst_instance_id = di;
			bu.extension.r.dst_queue = dq;
			bu.extension.r.src_queue = sq;
			memcpy(mbox, &bu, sizeof(bu));
			ipc_cmd((struct ipc_cmd_hdr *)mbox);
			uint32_t st = mbox[0] & 0x00FFFFFFU;

			off += snprintf(out_buf + off, out_cap - off,
					"%s(0x%x:%u q%u -> 0x%x:%u q%u) -> status=%u (%s)\n",
					is_unbind ? "MOD_UNBIND" : "MOD_BIND",
					sm, si, sq, dm, di, dq, st,
					st == 0 ? "IPC4_SUCCESS" : "FAILED");
		}
	} else if (strncmp(cmd, "mod-delete ", 11) == 0) {
		uint32_t mid = 0, iid = 0;

		if (sscanf(cmd + 11, "%i %i", &mid, &iid) < 2 ||
		    !sof_get() || !sof_get()->ipc || !sof_get()->ipc->comp_data) {
			off += snprintf(out_buf + off, out_cap - off,
					"Usage: mod-delete <mod_id> <inst_id>\n");
		} else {
			struct ipc4_module_delete_instance del = {{0}};
			uint32_t *mbox = (uint32_t *)sof_get()->ipc->comp_data;

			del.primary.r.type = SOF_IPC4_MOD_DELETE_INSTANCE;
			del.primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_MODULE_MSG;
			del.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
			del.primary.r.module_id = mid;
			del.primary.r.instance_id = iid;
			memcpy(mbox, &del, sizeof(del));
			ipc_cmd((struct ipc_cmd_hdr *)mbox);
			uint32_t st = mbox[0] & 0x00FFFFFFU;

			off += snprintf(out_buf + off, out_cap - off,
					"MOD_DELETE_INSTANCE(mod=0x%x, inst=%u) -> status=%u (%s)\n",
					mid, iid, st, st == 0 ? "IPC4_SUCCESS" : "FAILED");
		}
	} else if (strcmp(cmd, "controls") == 0) {
		off += snprintf(out_buf + off, out_cap - off,
				"=== Topology Kcontrols (%u total) ===\n",
				g_tp ? g_tp->glb_ctx.num_ctls : 0);
		if (g_tp) {
			for (uint32_t i = 0; i < g_tp->glb_ctx.num_ctls && off + 128 < out_cap; i++) {
				struct tb_ctl *ctl = &g_tp->glb_ctx.ctl[i];
				const char *tstr = (ctl->type == SND_SOC_TPLG_TYPE_MIXER) ? "MIXER" :
						   (ctl->type == SND_SOC_TPLG_TYPE_BYTES) ? "BYTES" :
						   (ctl->type == SND_SOC_TPLG_TYPE_ENUM)  ? "ENUM" : "OTHER";

				off += snprintf(out_buf + off, out_cap - off,
						"  [%2u] %-6s mod=0x%02x inst=%u name='%s'\n",
						i, tstr, ctl->module_id, ctl->instance_id, ctl->name);
			}
		}
	} else if (strncmp(cmd, "amixer ", 7) == 0 ||
		   strncmp(cmd, "cset ", 5) == 0 ||
		   strncmp(cmd, "sof-ctl ", 8) == 0) {
		if (!g_tp) {
			off += snprintf(out_buf + off, out_cap - off, "Error: testbench not ready\n");
		} else {
			int rc = tb_apply_control_line(g_tp, cmd);

			off += snprintf(out_buf + off, out_cap - off,
					"Control command '%s' -> %s (rc=%d)\n",
					cmd, rc == 0 ? "OK" : "FAILED", rc);
		}
	} else if (strcmp(cmd, "stats") == 0 || strcmp(cmd, "heap") == 0) {
		struct host_sched_telemetry *st = host_telem_get();

		off += snprintf(out_buf + off, out_cap - off,
				"=== Platform Memory Summary (%s) ===\n"
				"  HPSRAM: used=%8zu B / %8zu B (%zu KB) | peak=%8zu B | EBBs=%u\n"
				"  LPSRAM: used=%8zu B / %8zu B (%zu KB) | peak=%8zu B | EBBs=%u\n"
				"  DRAM:   used=%8zu B / %8zu B (%zu KB) | peak=%8zu B\n"
				"  Allocs: %llu total (%llu frees), %u OOM failures\n",
				st->hw_cfg.platform_name[0] ? st->hw_cfg.platform_name : "default",
				st->hw_cfg.hpsram_used_bytes, st->hw_cfg.hpsram_size_bytes,
				st->hw_cfg.hpsram_size_bytes / 1024UL, st->hw_cfg.hpsram_peak_bytes,
				(uint32_t)(st->hw_cfg.hpsram_size_bytes / 65536UL),
				st->hw_cfg.lpsram_used_bytes, st->hw_cfg.lpsram_size_bytes,
				st->hw_cfg.lpsram_size_bytes / 1024UL, st->hw_cfg.lpsram_peak_bytes,
				(uint32_t)(st->hw_cfg.lpsram_size_bytes / 65536UL),
				st->hw_cfg.dram_used_bytes, st->hw_cfg.dram_size_bytes,
				st->hw_cfg.dram_size_bytes / 1024UL, st->hw_cfg.dram_peak_bytes,
				(unsigned long long)st->hw_cfg.total_alloc_calls,
				(unsigned long long)st->hw_cfg.total_free_calls,
				st->hw_cfg.alloc_failures);
		if (g_tp) {
			off += snprintf(out_buf + off, out_cap - off, "\n=== File I/O Stats ===\n");
			for (int i = 0; i < g_tp->input_file_num && off + 128 < out_cap; i++) {
				if (g_tp->fr[i].state)
					off += snprintf(out_buf + off, out_cap - off,
							"  IN [%d] pipe=%d file=%s samples=%d copies=%d\n",
							i, g_tp->fr[i].pipeline_id,
							g_tp->fr[i].state->fn,
							g_tp->fr[i].state->n,
							g_tp->fr[i].state->copy_count);
			}
			for (int i = 0; i < g_tp->output_file_num && off + 128 < out_cap; i++) {
				if (g_tp->fw[i].state)
					off += snprintf(out_buf + off, out_cap - off,
							"  OUT[%d] pipe=%d file=%s samples=%d copies=%d\n",
							i, g_tp->fw[i].pipeline_id,
							g_tp->fw[i].state->fn,
							g_tp->fw[i].state->n,
							g_tp->fw[i].state->copy_count);
			}

			struct tb_heap_usage_record recs[64];
			int rcount = 0;

			memset(recs, 0, sizeof(recs));
			tb_collect_heap_usage(g_tp, recs, &rcount);
			off += snprintf(out_buf + off, out_cap - off, "\n=== Module Heap Usage ===\n");
			for (int i = 0; i < rcount && off + 96 < out_cap; i++) {
				off += snprintf(out_buf + off, out_cap - off,
						"  %-32s %8zu bytes\n",
						recs[i].module_name, recs[i].heap_max);
			}
		}
	} else if (strcmp(cmd, "sched-telem-reset") == 0) {
		host_telem_reset();
		off += snprintf(out_buf + off, out_cap - off,
				"Scheduling & buffer telemetry counters reset.\n");
	} else if (strncmp(cmd, "workload-sim", 12) == 0) {
		struct host_sched_telemetry *st = host_telem_get();
		const char *sub = cmd + 12;

		while (*sub == ' ' || *sub == '\t')
			sub++;

		if (*sub == '\0' || strcmp(sub, "status") == 0) {
			const char *mstr =
				(st->sim_cfg.mode == HOST_WORKLOAD_SIM_REALTIME) ? "REALTIME-PACED" :
				(st->sim_cfg.mode == HOST_WORKLOAD_SIM_VIRTUAL)  ? "VIRTUAL-SIM" :
										   "OFF (HOST-MEASURED)";
			const char *dstr =
				(st->sim_cfg.default_mem_domain == HOST_MEM_DOMAIN_DRAM) ? "DRAM" : "SRAM";
			uint32_t i;

			off += snprintf(out_buf + off, out_cap - off,
					"=== Workload Execution Time Simulation ===\n"
					"  Platform:          %s (%u cores, HPSRAM=%zu KB, LPSRAM=%zu KB, DRAM=%zu KB)\n"
					"  Mode:              %s (%u)\n"
					"  Default Memory:    %s (SRAM=%u.%02ux, DRAM=%u.%02ux + %u cyc/word)\n"
					"  Module Overrides:  %u active\n",
					st->hw_cfg.platform_name[0] ? st->hw_cfg.platform_name : "default",
					st->hw_cfg.num_cores,
					st->hw_cfg.hpsram_size_bytes / 1024UL,
					st->hw_cfg.lpsram_size_bytes / 1024UL,
					st->hw_cfg.dram_size_bytes / 1024UL,
					mstr, st->sim_cfg.mode, dstr,
					st->sim_cfg.sram_factor_x100 / 100,
					st->sim_cfg.sram_factor_x100 % 100,
					st->sim_cfg.dram_factor_x100 / 100,
					st->sim_cfg.dram_factor_x100 % 100,
					st->sim_cfg.dram_cycles_per_word,
					st->sim_cfg.num_overrides);
			for (i = 0; i < st->hw_cfg.num_cores && i < TB_TELEM_MAX_CORES && off + 192 < out_cap; i++) {
				struct host_core_telem *ct = &st->cores[i];

				off += snprintf(out_buf + off, out_cap - off,
						"  [Core %u] target_freq=%u.%03u MHz (budget=%u cyc/ms) active=%u util=%u.%02u%% overruns=%u LL_exec=%llu.%03llu/%llu.%03lluus DP_exec=%llu.%03llu/%llu.%03lluus\n",
						i, st->sim_cfg.core_freq_hz[i] / 1000000U,
						(st->sim_cfg.core_freq_hz[i] % 1000000U) / 1000U,
						st->sim_cfg.core_freq_hz[i] / 1000U,
						ct->active ? 1 : 0,
						ct->total_util_x100 / 100, ct->total_util_x100 % 100,
						ct->ll_deadline_violations,
						(unsigned long long)(ct->ll_avg_exec_ns / 1000ULL),
						(unsigned long long)(ct->ll_avg_exec_ns % 1000ULL),
						(unsigned long long)(ct->ll_peak_exec_ns / 1000ULL),
						(unsigned long long)(ct->ll_peak_exec_ns % 1000ULL),
						(unsigned long long)(ct->dp_avg_exec_ns / 1000ULL),
						(unsigned long long)(ct->dp_avg_exec_ns % 1000ULL),
						(unsigned long long)(ct->dp_peak_exec_ns / 1000ULL),
						(unsigned long long)(ct->dp_peak_exec_ns % 1000ULL));
			}
			for (i = 0; i < st->num_mods && off + 192 < out_cap; i++) {
				struct host_mod_telem *mt = &st->mods[i];
				struct tplg_comp_info *ci;
				const char *wname;

				if (!mt->active)
					continue;
				ci = find_comp_info_by_ipc_id(g_tp, mt->comp_id);
				wname = (ci && ci->name) ? ci->name : "comp";
				off += snprintf(out_buf + off, out_cap - off,
						"  [Mod 0x%06x %-22s] core=%u %s mem=%s cpc=%u avg_cyc=%llu (%u kcps) exec=%llu.%03llu/%llu.%03lluus viol=%u\n",
						mt->comp_id, wname, mt->core_id,
						mt->proc_domain == COMP_PROCESSING_DOMAIN_DP ? "DP" : "LL",
						mt->mem_domain == HOST_MEM_DOMAIN_DRAM ? "DRAM" : "SRAM",
						mt->effective_cpc,
						(unsigned long long)mt->avg_cycles, mt->avg_kcps,
						(unsigned long long)(mt->avg_exec_ns / 1000ULL),
						(unsigned long long)(mt->avg_exec_ns % 1000ULL),
						(unsigned long long)(mt->peak_exec_ns / 1000ULL),
						(unsigned long long)(mt->peak_exec_ns % 1000ULL),
						mt->cpc_violations);
			}
		} else if (strncmp(sub, "enable", 6) == 0 || strncmp(sub, "on", 2) == 0) {
			const char *arg = sub + (strncmp(sub, "enable", 6) == 0 ? 6 : 2);
			uint32_t m = HOST_WORKLOAD_SIM_VIRTUAL;

			while (*arg == ' ' || *arg == '\t')
				arg++;
			if (strcasecmp(arg, "realtime") == 0 || strcasecmp(arg, "pace") == 0 ||
			    strcmp(arg, "2") == 0)
				m = HOST_WORKLOAD_SIM_REALTIME;
			host_workload_sim_set_mode(m);
			host_telem_reset();
			off += snprintf(out_buf + off, out_cap - off,
					"Workload simulation enabled (mode=%s)\n",
					m == HOST_WORKLOAD_SIM_REALTIME ? "REALTIME-PACED" : "VIRTUAL-SIM");
		} else if (strncmp(sub, "disable", 7) == 0 || strncmp(sub, "off", 3) == 0) {
			host_workload_sim_set_mode(HOST_WORKLOAD_SIM_OFF);
			host_telem_reset();
			off += snprintf(out_buf + off, out_cap - off,
					"Workload simulation disabled (using measured host CPU cycles)\n");
		} else if (strncmp(sub, "freq ", 5) == 0) {
			const char *farg = sub + 5;

			while (*farg == ' ' || *farg == '\t')
				farg++;
			remote_apply_freq_arg(farg);
			if (host_workload_sim_get_mode() == HOST_WORKLOAD_SIM_OFF)
				host_workload_sim_set_mode(HOST_WORKLOAD_SIM_VIRTUAL);
			host_telem_reset();
			off += snprintf(out_buf + off, out_cap - off,
					"Target core frequencies updated: C0=%u.%03uMHz C1=%u.%03uMHz C2=%u.%03uMHz C3=%u.%03uMHz\n",
					st->sim_cfg.core_freq_hz[0] / 1000000U,
					(st->sim_cfg.core_freq_hz[0] % 1000000U) / 1000U,
					st->sim_cfg.core_freq_hz[1] / 1000000U,
					(st->sim_cfg.core_freq_hz[1] % 1000000U) / 1000U,
					st->sim_cfg.core_freq_hz[2] / 1000000U,
					(st->sim_cfg.core_freq_hz[2] % 1000000U) / 1000U,
					st->sim_cfg.core_freq_hz[3] / 1000000U,
					(st->sim_cfg.core_freq_hz[3] % 1000000U) / 1000U);
		} else if (strncmp(sub, "mem ", 4) == 0) {
			const char *marg = sub + 4;
			char dom_str[16] = {0};
			uint32_t factor = 0, c_word = 0;
			uint32_t dom = HOST_MEM_DOMAIN_SRAM;

			while (*marg == ' ' || *marg == '\t')
				marg++;
			sscanf(marg, "%15s %u %u", dom_str, &factor, &c_word);
			if (strncasecmp(dom_str, "dram", 4) == 0 || strcmp(dom_str, "1") == 0)
				dom = HOST_MEM_DOMAIN_DRAM;
			host_workload_sim_set_mem_domain(dom, factor, c_word);
			if (host_workload_sim_get_mode() == HOST_WORKLOAD_SIM_OFF)
				host_workload_sim_set_mode(HOST_WORKLOAD_SIM_VIRTUAL);
			host_telem_reset();
			off += snprintf(out_buf + off, out_cap - off,
					"Memory execution domain set to %s (DRAM factor=%u.%02ux, %u cyc/word)\n",
					dom == HOST_MEM_DOMAIN_DRAM ? "DRAM" : "SRAM",
					st->sim_cfg.dram_factor_x100 / 100,
					st->sim_cfg.dram_factor_x100 % 100,
					st->sim_cfg.dram_cycles_per_word);
		} else if (strncmp(sub, "mod ", 4) == 0) {
			const char *marg = sub + 4;
			char target_tok[64] = {0};
			uint32_t comp_id = 0xFFFFFFFFU;
			uint32_t cpc_ov = 0;
			uint32_t mem_ov = HOST_MEM_DOMAIN_AUTO;
			const char *p_cpc, *p_mem;

			while (*marg == ' ' || *marg == '\t')
				marg++;
			sscanf(marg, "%63s", target_tok);
			if (target_tok[0] >= '0' && target_tok[0] <= '9') {
				comp_id = (uint32_t)strtoul(target_tok, NULL, 0);
			} else if (g_tp) {
				for (uint32_t i = 0; i < st->num_mods; i++) {
					struct tplg_comp_info *ci =
						find_comp_info_by_ipc_id(g_tp, st->mods[i].comp_id);
					if (ci && ci->name && strstr(ci->name, target_tok)) {
						comp_id = st->mods[i].comp_id;
						break;
					}
				}
			}
			p_cpc = strstr(marg, "cpc=");
			if (p_cpc)
				cpc_ov = (uint32_t)strtoul(p_cpc + 4, NULL, 0);
			p_mem = strstr(marg, "mem=");
			if (p_mem) {
				if (strncasecmp(p_mem + 4, "dram", 4) == 0)
					mem_ov = HOST_MEM_DOMAIN_DRAM;
				else if (strncasecmp(p_mem + 4, "sram", 4) == 0)
					mem_ov = HOST_MEM_DOMAIN_SRAM;
			}
			if (comp_id == 0xFFFFFFFFU) {
				off += snprintf(out_buf + off, out_cap - off,
						"Usage: workload-sim mod <comp_id|name> [cpc=<cycles>] [mem=<sram|dram>]\n");
			} else {
				host_workload_sim_set_mod_override(comp_id, cpc_ov, mem_ov);
				if (host_workload_sim_get_mode() == HOST_WORKLOAD_SIM_OFF)
					host_workload_sim_set_mode(HOST_WORKLOAD_SIM_VIRTUAL);
				host_telem_reset();
				off += snprintf(out_buf + off, out_cap - off,
						"Override set for module 0x%06x: cpc=%u mem=%s\n",
						comp_id, cpc_ov,
						mem_ov == HOST_MEM_DOMAIN_DRAM ? "DRAM" :
						(mem_ov == HOST_MEM_DOMAIN_SRAM ? "SRAM" : "AUTO"));
			}
		} else if (strcmp(sub, "reset") == 0) {
			host_workload_sim_clear_overrides();
			host_telem_reset();
			off += snprintf(out_buf + off, out_cap - off,
					"Cleared module workload overrides and reset telemetry.\n");
		} else {
			off += snprintf(out_buf + off, out_cap - off,
					"Usage:\n"
					"  workload-sim status\n"
					"  workload-sim enable [virtual|realtime]\n"
					"  workload-sim disable\n"
					"  workload-sim freq <mhz | core:mhz,...>\n"
					"  workload-sim mem <sram|dram> [dram_factor_x100] [cycles_per_word]\n"
					"  workload-sim mod <comp_id|name> [cpc=<cycles>] [mem=<sram|dram>]\n"
					"  workload-sim reset\n");
		}
	} else if (strcmp(cmd, "sched-telem") == 0) {
		struct host_sched_telemetry *st = host_telem_get();
		const char *mode_str =
			(st->sim_cfg.mode == HOST_WORKLOAD_SIM_REALTIME) ? "REALTIME-PACED" :
			(st->sim_cfg.mode == HOST_WORKLOAD_SIM_VIRTUAL)  ? "VIRTUAL-SIM" :
									   "HOST-MEASURED";
		const char *mem_str =
			(st->sim_cfg.default_mem_domain == HOST_MEM_DOMAIN_DRAM) ? "DRAM" : "SRAM";
		uint32_t i;

		off += snprintf(out_buf + off, out_cap - off,
				"=== Per-Core LL & DP Scheduler Telemetry [Platform=%s(%uc) Mode=%s Mem=%s DRAM_Factor=%u.%02ux] ===\n",
				st->hw_cfg.platform_name[0] ? st->hw_cfg.platform_name : "default",
				st->hw_cfg.num_cores,
				mode_str, mem_str,
				st->sim_cfg.dram_factor_x100 / 100,
				st->sim_cfg.dram_factor_x100 % 100);
		for (i = 0; i < st->hw_cfg.num_cores && i < TB_TELEM_MAX_CORES && off + 256 < out_cap; i++) {
			struct host_core_telem *ct = &st->cores[i];

			if (!ct->active && !(arch_cpu_enabled_cores() & BIT(i)))
				continue;
			off += snprintf(out_buf + off, out_cap - off,
					"  [Core %u %u.%03uMHz] util=%u.%02u%% overruns=%u | LL: ticks=%u tasks=%u avg=%llu peak=%llu tot=%llu cyc (%u/%u kcps) exec=%llu.%03llu/%llu.%03lluus | DP: runs=%u thr=%u avg=%llu peak=%llu tot=%llu cyc (%u/%u kcps) exec=%llu.%03llu/%llu.%03lluus\n",
					i, ct->freq_hz / 1000000U, (ct->freq_hz % 1000000U) / 1000U,
					ct->total_util_x100 / 100, ct->total_util_x100 % 100,
					ct->ll_deadline_violations,
					ct->ll_tick_count, ct->ll_active_tasks,
					(unsigned long long)ct->ll_avg_cycles,
					(unsigned long long)ct->ll_peak_cycles,
					(unsigned long long)ct->ll_total_cycles,
					ct->ll_avg_kcps, ct->ll_peak_kcps,
					(unsigned long long)(ct->ll_avg_exec_ns / 1000ULL),
					(unsigned long long)(ct->ll_avg_exec_ns % 1000ULL),
					(unsigned long long)(ct->ll_peak_exec_ns / 1000ULL),
					(unsigned long long)(ct->ll_peak_exec_ns % 1000ULL),
					ct->dp_run_count, ct->dp_active_threads,
					(unsigned long long)ct->dp_avg_cycles,
					(unsigned long long)ct->dp_peak_cycles,
					(unsigned long long)ct->dp_total_cycles,
					ct->dp_avg_kcps, ct->dp_peak_kcps,
					(unsigned long long)(ct->dp_avg_exec_ns / 1000ULL),
					(unsigned long long)(ct->dp_avg_exec_ns % 1000ULL),
					(unsigned long long)(ct->dp_peak_exec_ns / 1000ULL),
					(unsigned long long)(ct->dp_peak_exec_ns % 1000ULL));
		}

		off += snprintf(out_buf + off, out_cap - off,
				"\n=== Per-Pipeline Scheduling Telemetry (%u active) ===\n",
				st->num_pipes);
		for (i = 0; i < st->num_pipes && off + 224 < out_cap; i++) {
			struct host_pipe_telem *pt = &st->pipes[i];

			if (!pt->active)
				continue;
			off += snprintf(out_buf + off, out_cap - off,
					"  [Pipe %2u] core=%u prio=%u per=%uus mem=%s mods=%uLL/%uDP copies=%llu avg=%llu peak=%llu tot=%llu cyc (%u/%u kcps) exec=%llu.%03llu/%llu.%03lluus xruns=%u\n",
					pt->pipeline_id, pt->core_id, pt->priority, pt->period_us,
					pt->mem_domain == HOST_MEM_DOMAIN_DRAM ? "DRAM" : "SRAM",
					pt->num_ll_modules, pt->num_dp_modules,
					(unsigned long long)pt->copy_iterations,
					(unsigned long long)pt->avg_cycles,
					(unsigned long long)pt->peak_cycles,
					(unsigned long long)pt->total_cycles,
					pt->avg_kcps, pt->peak_kcps,
					(unsigned long long)(pt->avg_exec_ns / 1000ULL),
					(unsigned long long)(pt->avg_exec_ns % 1000ULL),
					(unsigned long long)(pt->peak_exec_ns / 1000ULL),
					(unsigned long long)(pt->peak_exec_ns % 1000ULL),
					pt->xrun_count);
		}

		off += snprintf(out_buf + off, out_cap - off,
				"\n=== Per-Module LL & DP Scheduling + Entry/Exit Buffer Telemetry (%u active) ===\n",
				st->num_mods);
		for (i = 0; i < st->num_mods && off + 288 < out_cap; i++) {
			struct host_mod_telem *mt = &st->mods[i];
			const char *wname;

			if (!mt->active)
				continue;
			wname = resolve_comp_widget_name(g_tp, mt->comp_id);

			off += snprintf(out_buf + off, out_cap - off,
					"  [0x%06x %-24s] pipe=%u core=%u %s mem=%s iters=%llu avg=%llu peak=%llu tot=%llu cyc (%u/%u kcps) exec=%llu.%03llu/%llu.%03lluus last=%llu.%03llu->%llu.%03lluus(dl=%llu.%03lluus) ibs=%u obs=%u cpc=%u(viol=%u)",
					mt->comp_id, wname, mt->pipeline_id, mt->core_id,
					mt->proc_domain == COMP_PROCESSING_DOMAIN_DP ? "DP" : "LL",
					mt->mem_domain == HOST_MEM_DOMAIN_DRAM ? "DRAM" : "SRAM",
					(unsigned long long)mt->iterations,
					(unsigned long long)mt->avg_cycles,
					(unsigned long long)mt->peak_cycles,
					(unsigned long long)mt->total_cycles,
					mt->avg_kcps, mt->peak_kcps,
					(unsigned long long)(mt->avg_exec_ns / 1000ULL),
					(unsigned long long)(mt->avg_exec_ns % 1000ULL),
					(unsigned long long)(mt->peak_exec_ns / 1000ULL),
					(unsigned long long)(mt->peak_exec_ns % 1000ULL),
					(unsigned long long)(mt->last_start_ns / 1000ULL),
					(unsigned long long)(mt->last_start_ns % 1000ULL),
					(unsigned long long)(mt->last_complete_ns / 1000ULL),
					(unsigned long long)(mt->last_complete_ns % 1000ULL),
					(unsigned long long)(mt->last_deadline_ns / 1000ULL),
					(unsigned long long)(mt->last_deadline_ns % 1000ULL),
					mt->ibs, mt->obs, mt->effective_cpc, mt->cpc_violations);
			if (mt->proc_domain == COMP_PROCESSING_DOMAIN_DP) {
				off += snprintf(out_buf + off, out_cap - off,
						" ll_sync=%llu(%llu cyc)",
						(unsigned long long)mt->ll_ring_sync_iterations,
						(unsigned long long)mt->ll_ring_sync_total_cycles);
			}
			if (mt->num_in_pins > 0 && mt->in_pins[0].active) {
				struct host_mod_pin_telem *pin = &mt->in_pins[0];

				off += snprintf(out_buf + off, out_cap - off,
						" | IN0: %u->%uB xfer=%uB(tot=%lluB) rms=%d.%02ddBFS",
						pin->dp_ring_size ? pin->dp_ring_entry_avail :
								    pin->entry_avail_bytes,
						pin->dp_ring_size ? pin->dp_ring_exit_avail :
								    pin->exit_avail_bytes,
						pin->last_bytes_transferred,
						(unsigned long long)pin->total_bytes_transferred,
						pin->entry_rms_dbfs_x100 / 100,
						abs(pin->entry_rms_dbfs_x100 % 100));
			}
			if (mt->num_out_pins > 0 && mt->out_pins[0].active) {
				struct host_mod_pin_telem *pin = &mt->out_pins[0];

				off += snprintf(out_buf + off, out_cap - off,
						" | OUT0: %u->%uB xfer=%uB(tot=%lluB) rms=%d.%02ddBFS",
						pin->dp_ring_size ? pin->dp_ring_entry_avail :
								    pin->entry_avail_bytes,
						pin->dp_ring_size ? pin->dp_ring_exit_avail :
								    pin->exit_avail_bytes,
						pin->last_bytes_transferred,
						(unsigned long long)pin->total_bytes_transferred,
						pin->exit_rms_dbfs_x100 / 100,
						abs(pin->exit_rms_dbfs_x100 % 100));
			}
			off += snprintf(out_buf + off, out_cap - off, "\n");
		}
	} else if (strncmp(cmd, "sched-trace", 11) == 0) {
		struct host_sched_telemetry *st = host_telem_get();
		const char *arg = cmd + 11;
		uint32_t max_ev = 64;
		struct host_sched_trace_event *ev_buf;
		uint64_t total_ev = st ? st->trace_total_events : 0;
		uint32_t avail_ev = st ? st->trace_count : 0;
		uint32_t n_ev, e;

		while (*arg == ' ' || *arg == '\t')
			arg++;
		if (strcmp(arg, "clear") == 0 || strcmp(arg, "--clear") == 0) {
			host_telem_trace_clear();
			off += snprintf(out_buf + off, out_cap - off,
					"Cleared per-iteration scheduling timeline ring buffer.\n");
		} else {
			if (*arg >= '0' && *arg <= '9') {
				max_ev = (uint32_t)strtoul(arg, NULL, 10);
				if (max_ev == 0)
					max_ev = 64;
				if (max_ev > TB_TELEM_TRACE_CAPACITY)
					max_ev = TB_TELEM_TRACE_CAPACITY;
			}
			ev_buf = calloc(max_ev, sizeof(*ev_buf));
			if (ev_buf) {
				n_ev = host_telem_trace_read(ev_buf, max_ev, 0, -1, -1, -1, -1);
				off += snprintf(out_buf + off, out_cap - off,
						"=== Per-Iteration LL & DP Module Scheduling Timeline (showing %u of %llu events, ring=%u) ===\n"
						"  %-5s %-5s %-5s %-4s %-4s %-7s %-32s %12s %12s %10s %12s %21s %13s %11s\n",
						n_ev, (unsigned long long)total_ev, avail_ev,
						"Seq", "Tick", "Iter", "Core", "Pipe", "Domain",
						"Module", "Start(us)", "Complete(us)", "Dur(us)",
						"Deadline(us)", "Start->End(cyc)", "Cyc/Budget",
						"In->Out(B)");
				for (e = 0; e < n_ev && off + 224 < out_cap; e++) {
					struct host_sched_trace_event *ev = &ev_buf[e];
					const char *ename = resolve_comp_widget_name(g_tp, ev->comp_id);
					const char *dom =
						(ev->event_type == HOST_SCHED_EVT_DP_RUN) ? "DP-RUN" :
						(ev->event_type == HOST_SCHED_EVT_DP_SYNC) ? "DP-SYNC" :
											     "LL-COPY";
					uint64_t dur_ns = (ev->complete_ns >= ev->start_ns) ?
							  (ev->complete_ns - ev->start_ns) : 0;
					char mod_col[48];
					char cyc_col[48];
					char bud_col[32];
					char io_col[32];

					snprintf(mod_col, sizeof(mod_col), "0x%06x %.22s",
						 ev->comp_id, ename);
					snprintf(cyc_col, sizeof(cyc_col), "%llu->%llu",
						 (unsigned long long)ev->start_cycles,
						 (unsigned long long)ev->complete_cycles);
					snprintf(bud_col, sizeof(bud_col), "%u/%u%s",
						 ev->duration_cycles, ev->cpc_budget,
						 ev->cpc_violation ? "!" : "");
					snprintf(io_col, sizeof(io_col), "%u->%u",
						 ev->bytes_in, ev->bytes_out);

					off += snprintf(out_buf + off, out_cap - off,
							"  %-5u %-5u %-5u C%-3u P%-3u %-7s %-32s %8llu.%03llu %8llu.%03llu %6llu.%03llu %8llu.%03llu %21s %13s %11s\n",
							ev->seq_id, ev->ll_tick_nr, ev->mod_iter_nr,
							ev->core_id, ev->pipeline_id, dom, mod_col,
							(unsigned long long)(ev->start_ns / 1000ULL),
							(unsigned long long)(ev->start_ns % 1000ULL),
							(unsigned long long)(ev->complete_ns / 1000ULL),
							(unsigned long long)(ev->complete_ns % 1000ULL),
							(unsigned long long)(dur_ns / 1000ULL),
							(unsigned long long)(dur_ns % 1000ULL),
							(unsigned long long)(ev->deadline_ns / 1000ULL),
							(unsigned long long)(ev->deadline_ns % 1000ULL),
							cyc_col, bud_col, io_col);
				}
				free(ev_buf);
			}
		}
	} else if (strncmp(cmd, "mod-buffers", 11) == 0) {
		struct host_sched_telemetry *st = host_telem_get();
		const char *filter = cmd + 11;
		uint32_t filter_id = 0xFFFFFFFFU;
		uint32_t i, p, s;

		while (*filter == ' ' || *filter == '\t')
			filter++;
		if (*filter >= '0' && *filter <= '9')
			filter_id = (uint32_t)strtoul(filter, NULL, 0);

		off += snprintf(out_buf + off, out_cap - off,
				"=== Module Entry/Exit Buffer Data & Sample Taps ===\n");
		for (i = 0; i < st->num_mods && off + 384 < out_cap; i++) {
			struct host_mod_telem *mt = &st->mods[i];
			struct tplg_comp_info *ci;
			const char *wname;

			if (!mt->active)
				continue;
			ci = find_comp_info_by_ipc_id(g_tp, mt->comp_id);
			wname = (ci && ci->name) ? ci->name : "comp";

			if (*filter) {
				if (filter_id != 0xFFFFFFFFU) {
					if (mt->comp_id != filter_id)
						continue;
				} else if (!strstr(wname, filter)) {
					continue;
				}
			}

			off += snprintf(out_buf + off, out_cap - off,
					"[0x%06x %s] pipe=%u core=%u domain=%s iters=%llu cyc(last=%llu avg=%llu peak=%llu)\n",
					mt->comp_id, wname, mt->pipeline_id, mt->core_id,
					mt->proc_domain == COMP_PROCESSING_DOMAIN_DP ? "DP" : "LL",
					(unsigned long long)mt->iterations,
					(unsigned long long)mt->last_cycles,
					(unsigned long long)mt->avg_cycles,
					(unsigned long long)mt->peak_cycles);

			for (p = 0; p < mt->num_in_pins && off + 256 < out_cap; p++) {
				struct host_mod_pin_telem *pin = &mt->in_pins[p];

				if (!pin->active)
					continue;
				off += snprintf(out_buf + off, out_cap - off,
						"  IN [%u] buf=%u peer=0x%x %uHz/%uch/fmt=%u size=%uB LL(entry=%u/%uB -> exit=%u/%uB)",
						p, pin->buf_id, pin->peer_comp_id,
						pin->rate, pin->channels, pin->frame_fmt,
						pin->buf_size,
						pin->entry_avail_bytes, pin->entry_free_bytes,
						pin->exit_avail_bytes, pin->exit_free_bytes);
				if (pin->dp_ring_size > 0) {
					off += snprintf(out_buf + off, out_cap - off,
							" DP_RING(size=%uB entry=%u/%uB -> exit=%u/%uB)",
							pin->dp_ring_size,
							pin->dp_ring_entry_avail,
							pin->dp_ring_entry_free,
							pin->dp_ring_exit_avail,
							pin->dp_ring_exit_free);
				}
				off += snprintf(out_buf + off, out_cap - off,
						" consumed=%uB(tot=%lluB) entry_rms=%d.%02ddB(pk=%d.%02ddB) exit_rms=%d.%02ddB\n",
						pin->last_bytes_transferred,
						(unsigned long long)pin->total_bytes_transferred,
						pin->entry_rms_dbfs_x100 / 100,
						abs(pin->entry_rms_dbfs_x100 % 100),
						pin->entry_peak_dbfs_x100 / 100,
						abs(pin->entry_peak_dbfs_x100 % 100),
						pin->exit_rms_dbfs_x100 / 100,
						abs(pin->exit_rms_dbfs_x100 % 100));
				if (pin->entry_num_samples > 0 && off + 160 < out_cap) {
					uint16_t n = pin->entry_num_samples < 8 ?
						     pin->entry_num_samples : 8;
					off += snprintf(out_buf + off, out_cap - off,
							"         entry_samples[%u]:",
							pin->entry_num_samples);
					for (s = 0; s < n; s++)
						off += snprintf(out_buf + off, out_cap - off,
								" %d", pin->entry_samples[s]);
					off += snprintf(out_buf + off, out_cap - off, "\n");
				}
			}

			for (p = 0; p < mt->num_out_pins && off + 256 < out_cap; p++) {
				struct host_mod_pin_telem *pin = &mt->out_pins[p];

				if (!pin->active)
					continue;
				off += snprintf(out_buf + off, out_cap - off,
						"  OUT[%u] buf=%u peer=0x%x %uHz/%uch/fmt=%u size=%uB LL(entry=%u/%uB -> exit=%u/%uB)",
						p, pin->buf_id, pin->peer_comp_id,
						pin->rate, pin->channels, pin->frame_fmt,
						pin->buf_size,
						pin->entry_avail_bytes, pin->entry_free_bytes,
						pin->exit_avail_bytes, pin->exit_free_bytes);
				if (pin->dp_ring_size > 0) {
					off += snprintf(out_buf + off, out_cap - off,
							" DP_RING(size=%uB entry=%u/%uB -> exit=%u/%uB)",
							pin->dp_ring_size,
							pin->dp_ring_entry_avail,
							pin->dp_ring_entry_free,
							pin->dp_ring_exit_avail,
							pin->dp_ring_exit_free);
				}
				off += snprintf(out_buf + off, out_cap - off,
						" produced=%uB(tot=%lluB) entry_rms=%d.%02ddB exit_rms=%d.%02ddB(pk=%d.%02ddB)\n",
						pin->last_bytes_transferred,
						(unsigned long long)pin->total_bytes_transferred,
						pin->entry_rms_dbfs_x100 / 100,
						abs(pin->entry_rms_dbfs_x100 % 100),
						pin->exit_rms_dbfs_x100 / 100,
						abs(pin->exit_rms_dbfs_x100 % 100),
						pin->exit_peak_dbfs_x100 / 100,
						abs(pin->exit_peak_dbfs_x100 % 100));
				if (pin->exit_num_samples > 0 && off + 160 < out_cap) {
					uint16_t n = pin->exit_num_samples < 8 ?
						     pin->exit_num_samples : 8;
					off += snprintf(out_buf + off, out_cap - off,
							"         exit_samples[%u]: ",
							pin->exit_num_samples);
					for (s = 0; s < n; s++)
						off += snprintf(out_buf + off, out_cap - off,
								" %d", pin->exit_samples[s]);
					off += snprintf(out_buf + off, out_cap - off, "\n");
				}
			}
		}
	} else if (strcmp(cmd, "kernel cycles") == 0) {
		struct host_sched_telemetry *st = host_telem_get();
		uint32_t mask = arch_cpu_enabled_cores();
		uint32_t ncores = st ? st->hw_cfg.num_cores : CONFIG_CORE_COUNT;

		off += snprintf(out_buf + off, out_cap - off,
				"Simulated DSP Clock: %u Hz (%u.%03u MHz, Platform: %s, %u cores), Total LL Ticks: %llu\n",
				st ? st->sim_cfg.core_freq_hz[0] : (uint32_t)TB_DSP_NOMINAL_HZ,
				(st ? st->sim_cfg.core_freq_hz[0] : (uint32_t)TB_DSP_NOMINAL_HZ) / 1000000U,
				((st ? st->sim_cfg.core_freq_hz[0] : (uint32_t)TB_DSP_NOMINAL_HZ) % 1000000U) / 1000U,
				(st && st->hw_cfg.platform_name[0]) ? st->hw_cfg.platform_name : "default",
				ncores, (unsigned long long)g_total_ticks);
		for (int c = 0; c < (int)ncores && c < CONFIG_CORE_COUNT && off + 128 < out_cap; c++) {
			if (!(mask & BIT(c)))
				continue;
			off += snprintf(out_buf + off, out_cap - off,
					"  Core %d (%u.%03u MHz): ticks=%u last_cyc=%u max_cyc=%u avg_kcps=%u peak_kcps=%u\n",
					c,
					st ? st->sim_cfg.core_freq_hz[c] / 1000000U : 400U,
					st ? (st->sim_cfg.core_freq_hz[c] % 1000000U) / 1000U : 0U,
					g_core_ticks[c].count,
					g_core_ticks[c].last_time_elapsed,
					g_core_ticks[c].max_time_elapsed,
					g_core_ticks[c].avg_utilization,
					g_core_ticks[c].peak_utilization);
		}
	} else if (strcmp(cmd, "kernel threads") == 0) {
		struct host_sched_telemetry *st = host_telem_get();
		uint32_t ncores = st ? st->hw_cfg.num_cores : CONFIG_CORE_COUNT;
		uint32_t mask = arch_cpu_enabled_cores();

		off += snprintf(out_buf + off, out_cap - off,
				"=== Active Testbench Worker Threads (%s, %u configured cores) ===\n",
				(st && st->hw_cfg.platform_name[0]) ? st->hw_cfg.platform_name : "default",
				ncores);
		for (int c = 0; c < (int)ncores && c < CONFIG_CORE_COUNT && off + 128 < out_cap; c++) {
			if (mask & BIT(c))
				off += snprintf(out_buf + off, out_cap - off,
						"  [LL Worker Core %d] state=%s ticks=%u\n",
						c, g_dsp_paused ? "SUSPENDED" : "RUNNING",
						g_core_ticks[c].count);
		}
		if (g_tp && g_tp->widget_list.next) {
			struct list_item *item;

			list_for_item(item, &g_tp->widget_list) {
				struct tplg_comp_info *ci =
					container_of(item, struct tplg_comp_info, item);

				if (off + 128 >= out_cap)
					break;
				if (ci->proc_domain == 1 &&
				    tb_is_pipeline_enabled(g_tp, ci->pipeline_id))
					off += snprintf(out_buf + off, out_cap - off,
							"  [DP Worker Core %d] module=%s (0x%x)\n",
							ci->core_id,
							ci->name ? ci->name : "dp_mod",
							(uint32_t)IPC4_COMP_ID(ci->module_id,
									       ci->instance_id));
			}
		} else if (g_tp && g_tp->info) {
			for (int i = 0; i < g_tp->info_elems && off + 128 < out_cap; i++) {
				struct tplg_comp_info *ci = &g_tp->info[i];

				if (ci->proc_domain == 1 &&
				    tb_is_pipeline_enabled(g_tp, ci->pipeline_id))
					off += snprintf(out_buf + off, out_cap - off,
							"  [DP Worker Core %d] module=%s (0x%x)\n",
							ci->core_id,
							ci->name ? ci->name : "dp_mod",
							(uint32_t)IPC4_COMP_ID(ci->module_id,
									       ci->instance_id));
			}
		}
	} else if (strcmp(cmd, "pause") == 0) {
		g_dsp_paused = true;
		off += snprintf(out_buf + off, out_cap - off, "Testbench paused (D3)\n");
	} else if (strcmp(cmd, "resume") == 0) {
		g_dsp_paused = false;
		pthread_cond_broadcast(&g_tb_cond);
		off += snprintf(out_buf + off, out_cap - off, "Testbench resumed (D0)\n");
	} else if (strncmp(cmd, "step", 4) == 0) {
		int n = atoi(cmd + 4);

		if (n <= 0)
			n = 1;
		g_step_ticks_remaining = n;
		pthread_cond_broadcast(&g_tb_cond);
		off += snprintf(out_buf + off, out_cap - off, "Stepping %d tick(s)\n", n);
	} else if (strncmp(cmd, "loglevel ", 9) == 0) {
		int lvl = atoi(cmd + 9);

		if (lvl >= 0 && lvl <= 4) {
			tb_enable_trace((unsigned int)lvl);
			off += snprintf(out_buf + off, out_cap - off,
					"Trace level set to %d\n", lvl);
		} else {
			off += snprintf(out_buf + off, out_cap - off,
					"Invalid loglevel (expected 0..4)\n");
		}
	} else if (strcmp(cmd, "quit") == 0) {
		g_quit_requested = true;
		g_dsp_paused = false;
		pthread_cond_broadcast(&g_tb_cond);
		off += snprintf(out_buf + off, out_cap - off, "Stopping testbench...\n");
	} else {
		off += snprintf(out_buf + off, out_cap - off,
				"Unknown command '%s' (try 'help')\n", cmd);
	}

	snprintf(g_last_shell_output, sizeof(g_last_shell_output), "%s\nuart:~$ ", out_buf);
	pthread_mutex_unlock(&g_tb_lock);
	return (int)off;
}

/* -------------------------------------------------------------------------
 * Per-Opcode RPC Handlers
 * -------------------------------------------------------------------------
 */

static int handle_ipc_large_cfg(int sock, uint32_t req_id, const uint8_t *req_buf,
				uint32_t req_len)
{
	if (req_len < sizeof(struct sof_net_large_cfg_req))
		return send_reply(sock, SOF_NET_OP_IPC_LARGE_CFG, req_id, -EINVAL, NULL, 0);

	const struct sof_net_large_cfg_req *lreq = (const struct sof_net_large_cfg_req *)req_buf;

	if (!lreq->data_size || lreq->data_size > (256 * 1024))
		return send_reply(sock, SOF_NET_OP_IPC_LARGE_CFG, req_id, -EINVAL, NULL, 0);

	uint8_t *dbuf = calloc(1, lreq->data_size);

	if (!dbuf)
		return send_reply(sock, SOF_NET_OP_IPC_LARGE_CFG, req_id, -ENOMEM, NULL, 0);

	uint32_t out_len = 0;
	int st = 0;

	pthread_mutex_lock(&g_tb_lock);

	if (lreq->module_id == 0) {
		/* BaseFW Large Config */
		uint32_t param_type = lreq->param_id & 0xFF;

		if (lreq->set_get == SOF_CDEV_LARGE_CFG_SET) {
			if (req_len < sizeof(*lreq) + lreq->data_size) {
				st = -EINVAL;
			} else if (param_type == TB_IPC4_PERF_MEASUREMENTS_STATE) {
				g_perf_meas_state = *(const uint8_t *)(req_buf + sizeof(*lreq));
				if (g_perf_meas_state == 1 /* STOPPED */) {
					memset(g_core_ticks, 0, sizeof(g_core_ticks));
					host_telem_reset();
				}
			}
		} else {
			switch (param_type) {
			case TB_IPC4_FW_CONFIG:
			case TB_IPC4_HW_CONFIG_GET:
				out_len = build_hw_config_tlv(dbuf, lreq->data_size);
				break;
			case TB_IPC4_MODULES_INFO_GET:
				out_len = build_modules_info_tlv(g_tp, dbuf, lreq->data_size);
				break;
			case TB_IPC4_MEMORY_STATE_INFO_GET:
				out_len = build_memory_state_tlv(g_tp, dbuf, lreq->data_size);
				break;
			case TB_IPC4_POWER_STATE_INFO_GET:
				out_len = build_power_state_tlv(dbuf, lreq->data_size);
				break;
			case TB_IPC4_GLOBAL_PERF_DATA:
			case TB_IPC4_EXTENDED_GLOBAL_PERF_DATA:
				out_len = build_ext_perf_tlv(g_tp, dbuf, lreq->data_size);
				break;
			case TB_IPC4_PERF_MEASUREMENTS_STATE:
				if (lreq->data_size >= 4) {
					memcpy(dbuf, &g_perf_meas_state, 4);
					out_len = 4;
				}
				break;
			default:
				st = -ENOENT;
				break;
			}
		}
	} else {
		/* Module Instance Large Config */
		uint32_t comp_id = IPC4_COMP_ID(lreq->module_id, lreq->instance_id);
		struct ipc_comp_dev *icd = sof_get()->ipc ?
					   ipc_get_comp_by_id(sof_get()->ipc, comp_id) : NULL;

		if (!icd || !icd->cd || !icd->cd->drv) {
			st = -ENOENT;
		} else if (lreq->set_get == SOF_CDEV_LARGE_CFG_SET) {
			if (req_len < sizeof(*lreq) + lreq->data_size) {
				st = -EINVAL;
			} else if (icd->cd->drv->ops.set_large_config) {
				int rc = icd->cd->drv->ops.set_large_config(
					icd->cd, lreq->param_id, true, true, lreq->data_size,
					(const char *)(req_buf + sizeof(*lreq)));
				st = (rc == 0) ? 0 : -EIO;
			} else {
				st = -ENOTSUP;
			}
		} else {
			if (icd->cd->drv->ops.get_large_config) {
				out_len = lreq->data_size;
				int rc = icd->cd->drv->ops.get_large_config(
					icd->cd, lreq->param_id, true, true,
					&out_len, (char *)dbuf);
				st = (rc == 0) ? 0 : -EIO;
			} else {
				st = -ENOTSUP;
			}
		}
	}

	pthread_mutex_unlock(&g_tb_lock);

	int ret = send_reply(sock, SOF_NET_OP_IPC_LARGE_CFG, req_id, st,
			     (st == 0 && lreq->set_get == SOF_CDEV_LARGE_CFG_GET) ? dbuf : NULL,
			     (st == 0 && lreq->set_get == SOF_CDEV_LARGE_CFG_GET) ? out_len : 0);
	free(dbuf);
	return ret;
}

static int handle_workload_sim_cfg(int sock, uint32_t req_id, const uint8_t *req_buf,
				   uint32_t req_len)
{
	struct sof_net_workload_sim_req req = {0};
	struct sof_net_workload_sim_rsp rsp = {0};
	struct host_sched_telemetry *st;
	int c;

	if (req_len >= sizeof(req))
		memcpy(&req, req_buf, sizeof(req));

	pthread_mutex_lock(&g_tb_lock);
	st = host_telem_get();

	if (req.op == SOF_WORKLOAD_SIM_OP_SET) {
		host_workload_sim_set_mode(req.mode);
		host_workload_sim_set_mem_domain(req.default_mem_domain,
						 req.dram_factor_x100,
						 req.dram_cycles_per_word);
		for (c = 0; c < TB_TELEM_MAX_CORES && c < 8; c++) {
			if (req.core_freq_hz[c] > 0) {
				uint32_t hz = (req.core_freq_hz[c] < 100000U) ?
					      (req.core_freq_hz[c] * 1000000U) :
					      req.core_freq_hz[c];
				host_workload_sim_set_core_freq(c, hz);
			}
		}
		host_telem_reset();
	} else if (req.op == SOF_WORKLOAD_SIM_OP_MOD_SET) {
		host_workload_sim_set_mod_override(req.target_comp_id,
						   req.cpc_override,
						   req.mem_domain_override);
		if (host_workload_sim_get_mode() == HOST_WORKLOAD_SIM_OFF)
			host_workload_sim_set_mode(HOST_WORKLOAD_SIM_VIRTUAL);
		host_telem_reset();
	} else if (req.op == SOF_WORKLOAD_SIM_OP_RESET_OVERRIDES) {
		host_workload_sim_clear_overrides();
		host_telem_reset();
	}

	rsp.status = 0;
	rsp.mode = st->sim_cfg.mode;
	rsp.default_mem_domain = st->sim_cfg.default_mem_domain;
	rsp.sram_factor_x100 = st->sim_cfg.sram_factor_x100;
	rsp.dram_factor_x100 = st->sim_cfg.dram_factor_x100;
	rsp.dram_cycles_per_word = st->sim_cfg.dram_cycles_per_word;
	rsp.num_cores = st->hw_cfg.num_cores;
	rsp.num_overrides = st->sim_cfg.num_overrides;

	for (c = 0; c < TB_TELEM_MAX_CORES && c < 8; c++) {
		rsp.cores[c].active = st->cores[c].active ? 1 : 0;
		rsp.cores[c].freq_hz = st->sim_cfg.core_freq_hz[c];
		rsp.cores[c].total_util_x100 = st->cores[c].total_util_x100;
		rsp.cores[c].ll_deadline_violations = st->cores[c].ll_deadline_violations;
		rsp.cores[c].ll_avg_kcps = st->cores[c].ll_avg_kcps;
		rsp.cores[c].ll_peak_kcps = st->cores[c].ll_peak_kcps;
		rsp.cores[c].ll_avg_exec_ns = st->cores[c].ll_avg_exec_ns;
		rsp.cores[c].ll_peak_exec_ns = st->cores[c].ll_peak_exec_ns;
		rsp.cores[c].dp_avg_kcps = st->cores[c].dp_avg_kcps;
		rsp.cores[c].dp_peak_kcps = st->cores[c].dp_peak_kcps;
		rsp.cores[c].dp_avg_exec_ns = st->cores[c].dp_avg_exec_ns;
		rsp.cores[c].dp_peak_exec_ns = st->cores[c].dp_peak_exec_ns;
	}

	pthread_mutex_unlock(&g_tb_lock);
	return send_reply(sock, SOF_NET_OP_WORKLOAD_SIM_CFG, req_id, 0, &rsp, sizeof(rsp));
}

static int handle_sched_trace(int sock, uint32_t req_id, const uint8_t *req_buf,
			      uint32_t req_len)
{
	struct sof_net_sched_trace_req req = {
		.op = SOF_SCHED_TRACE_OP_READ,
		.max_events = 256,
		.filter_core_id = 0xFFFFFFFFU,
		.filter_pipe_id = 0xFFFFFFFFU,
		.filter_comp_id = 0xFFFFFFFFU,
		.filter_domain  = 0xFFFFFFFFU,
	};
	struct host_sched_trace_event *raw_events = NULL;
	struct host_sched_telemetry *st;
	uint64_t total_recorded = 0;
	uint32_t avail_in_ring = 0;
	uint32_t raw_count = 0;
	uint32_t matched_count = 0;
	uint32_t max_ret;
	uint32_t start_idx = 0;
	uint32_t out_idx = 0;
	uint32_t i;

	if (req_len >= sizeof(req))
		memcpy(&req, req_buf, sizeof(req));

	max_ret = (req.max_events > 0 && req.max_events <= TB_TELEM_TRACE_CAPACITY) ?
		  req.max_events : 256;

	pthread_mutex_lock(&g_tb_lock);
	st = host_telem_get();

	if (req.op == SOF_SCHED_TRACE_OP_CLEAR) {
		host_telem_trace_clear();
		struct sof_net_sched_trace_hdr chdr = {0};

		if (st) {
			chdr.num_cores = st->hw_cfg.num_cores;
			chdr.sim_mode = st->sim_cfg.mode;
			chdr.current_ll_tick = st->current_ll_tick;
			snprintf(chdr.platform_name, sizeof(chdr.platform_name), "%s",
				 st->hw_cfg.platform_name[0] ?
				 st->hw_cfg.platform_name : "default");
		}
		pthread_mutex_unlock(&g_tb_lock);
		return send_reply(sock, SOF_NET_OP_SCHED_TRACE, req_id, 0, &chdr, sizeof(chdr));
	}

	raw_events = calloc(max_ret, sizeof(*raw_events));
	if (!raw_events) {
		pthread_mutex_unlock(&g_tb_lock);
		return send_reply(sock, SOF_NET_OP_SCHED_TRACE, req_id, -ENOMEM, NULL, 0);
	}

	total_recorded = st ? st->trace_total_events : 0;
	avail_in_ring = st ? st->trace_count : 0;
	raw_count = host_telem_trace_read(raw_events, max_ret, req.since_seq_id,
					  (req.filter_core_id == 0xFFFFFFFFU) ?
					  -1 : (int)req.filter_core_id,
					  (req.filter_pipe_id == 0xFFFFFFFFU) ?
					  -1 : (int)req.filter_pipe_id,
					  (req.filter_comp_id == 0xFFFFFFFFU) ?
					  -1 : (int)req.filter_comp_id,
					  (req.filter_domain == 0xFFFFFFFFU) ?
					  -1 : (int)req.filter_domain);
	matched_count = raw_count;

	uint32_t ret_events = matched_count - start_idx;
	uint32_t rsp_len = sizeof(struct sof_net_sched_trace_hdr) +
			   ret_events * sizeof(struct sof_net_sched_trace_entry);
	uint8_t *rsp_buf = calloc(1, rsp_len);

	if (!rsp_buf) {
		free(raw_events);
		pthread_mutex_unlock(&g_tb_lock);
		return send_reply(sock, SOF_NET_OP_SCHED_TRACE, req_id, -ENOMEM, NULL, 0);
	}

	struct sof_net_sched_trace_hdr *hdr = (struct sof_net_sched_trace_hdr *)rsp_buf;
	struct sof_net_sched_trace_entry *entries =
		(struct sof_net_sched_trace_entry *)(rsp_buf + sizeof(*hdr));

	hdr->status = 0;
	hdr->total_recorded_events = (uint32_t)total_recorded;
	hdr->avail_in_ring = avail_in_ring;
	hdr->returned_events = ret_events;
	hdr->current_ll_tick = st ? st->current_ll_tick : 0;
	hdr->num_cores = st ? st->hw_cfg.num_cores : CONFIG_CORE_COUNT;
	hdr->sim_mode = st ? st->sim_cfg.mode : 0;
	snprintf(hdr->platform_name, sizeof(hdr->platform_name), "%s",
		 (st && st->hw_cfg.platform_name[0]) ? st->hw_cfg.platform_name : "default");

	for (i = start_idx; i < matched_count; i++) {
		const struct host_sched_trace_event *src = &raw_events[i];
		struct sof_net_sched_trace_entry *dst = &entries[out_idx++];
		const char *wname = resolve_comp_widget_name(g_tp, src->comp_id);

		dst->seq_id = src->seq_id;
		dst->ll_tick_nr = src->ll_tick_nr;
		dst->mod_iter_nr = src->mod_iter_nr;
		dst->comp_id = src->comp_id;
		dst->pipeline_id = src->pipeline_id;
		dst->core_id = src->core_id;
		dst->event_type = src->event_type;
		dst->mem_domain = src->mem_domain;
		dst->cpc_violation = src->cpc_violation;
		dst->period_us = src->period_us;
		dst->start_cycles = src->start_cycles;
		dst->complete_cycles = src->complete_cycles;
		dst->duration_cycles = src->duration_cycles;
		dst->cpc_budget = src->cpc_budget;
		dst->start_ns = src->start_ns;
		dst->complete_ns = src->complete_ns;
		dst->deadline_ns = src->deadline_ns;
		dst->wall_start_ns = src->wall_start_ns;
		dst->wall_complete_ns = src->wall_complete_ns;
		dst->bytes_in = src->bytes_in;
		dst->bytes_out = src->bytes_out;
		dst->ret_status = src->ret_status;
		snprintf(dst->widget_name, sizeof(dst->widget_name), "%s", wname);
	}

	free(raw_events);
	pthread_mutex_unlock(&g_tb_lock);

	int ret = send_reply(sock, SOF_NET_OP_SCHED_TRACE, req_id, 0, rsp_buf, rsp_len);

	free(rsp_buf);
	return ret;
}

static int handle_telem_snapshot(int sock, uint32_t req_id, const uint8_t *req_buf,
				 uint32_t req_len)
{
	struct sof_net_telem_req req = {
		.query_mask = SOF_TELEM_FL_FW_REGS |
			      SOF_TELEM_FL_TELEM_SLOT |
			      SOF_TELEM_FL_EXT_PERF_TLV,
		.flags = 0,
	};

	if (req_len >= sizeof(req))
		memcpy(&req, req_buf, sizeof(req));

	struct sof_net_telem_snapshot *snap = calloc(1, sizeof(*snap));

	if (!snap)
		return send_reply(sock, SOF_NET_OP_TELEM_SNAPSHOT, req_id, -ENOMEM, NULL, 0);

	pthread_mutex_lock(&g_tb_lock);

	populate_cdev_info(g_tp, &snap->dev_info);
	refresh_emulated_windows(g_tp);

	snap->active_pcm_streams = g_tp ? (uint32_t)g_tp->pipeline_num : 1;

	if (req.query_mask & SOF_TELEM_FL_FW_REGS) {
		memcpy(snap->fw_regs, g_win0_fw_regs, sizeof(snap->fw_regs));
		snap->flags_present |= SOF_TELEM_FL_FW_REGS;
	}

	if (req.query_mask & SOF_TELEM_FL_TELEM_SLOT) {
		memcpy(snap->telem_slot, g_win2_debug + 4096, sizeof(snap->telem_slot));
		snap->flags_present |= SOF_TELEM_FL_TELEM_SLOT;
	}

	if (req.query_mask & SOF_TELEM_FL_PERF_WIN3) {
		memcpy(snap->perf_win3, g_win3_perf, sizeof(snap->perf_win3));
		snap->flags_present |= SOF_TELEM_FL_PERF_WIN3;
	}

	if (req.query_mask & SOF_TELEM_FL_MEM_STATE_TLV) {
		snap->mem_state_len = build_memory_state_tlv(g_tp, snap->mem_state_tlv,
							     sizeof(snap->mem_state_tlv));
		if (snap->mem_state_len > 0)
			snap->flags_present |= SOF_TELEM_FL_MEM_STATE_TLV;
	}

	if (req.query_mask & SOF_TELEM_FL_PWR_STATE_TLV) {
		snap->pwr_state_len = build_power_state_tlv(snap->pwr_state_tlv,
							    sizeof(snap->pwr_state_tlv));
		if (snap->pwr_state_len > 0)
			snap->flags_present |= SOF_TELEM_FL_PWR_STATE_TLV;
	}

	if (req.query_mask & SOF_TELEM_FL_EXT_PERF_TLV) {
		snap->ext_perf_len = build_ext_perf_tlv(g_tp, snap->ext_perf_tlv,
							sizeof(snap->ext_perf_tlv));
		if (snap->ext_perf_len > 0)
			snap->flags_present |= SOF_TELEM_FL_EXT_PERF_TLV;
	}

	if (req.query_mask & SOF_TELEM_FL_MODULES_INFO_TLV) {
		snap->modules_info_len = build_modules_info_tlv(g_tp, snap->modules_info_tlv,
								sizeof(snap->modules_info_tlv));
		if (snap->modules_info_len > 0)
			snap->flags_present |= SOF_TELEM_FL_MODULES_INFO_TLV;
	}

	pthread_mutex_unlock(&g_tb_lock);

	int ret = send_reply(sock, SOF_NET_OP_TELEM_SNAPSHOT, req_id, 0, snap, sizeof(*snap));

	free(snap);
	return ret;
}

static int handle_probe_ctl(int sock, uint32_t req_id, const uint8_t *req_buf, uint32_t req_len)
{
	if (req_len < sizeof(struct sof_net_probe_req))
		return send_reply(sock, SOF_NET_OP_PROBE_CTL, req_id, -EINVAL, NULL, 0);

	const struct sof_net_probe_req *preq = (const struct sof_net_probe_req *)req_buf;

	if (preq->op == SOF_PROBE_OP_READ_BUFFER) {
		uint32_t target_id = preq->stream_tag;
		uint32_t max_bytes = (preq->dma_buffer_size && preq->dma_buffer_size <= 8192) ?
				     preq->dma_buffer_size : 64;
		uint8_t *out_buf = calloc(1, sizeof(*preq) + max_bytes);

		if (!out_buf)
			return send_reply(sock, SOF_NET_OP_PROBE_CTL, req_id, -ENOMEM, NULL, 0);

		struct sof_net_probe_req *prsp = (struct sof_net_probe_req *)out_buf;
		int32_t *dst_samples = (int32_t *)(out_buf + sizeof(*preq));
		uint32_t copied_bytes = 0;

		*prsp = *preq;
		pthread_mutex_lock(&g_tb_lock);
		struct host_sched_telemetry *st = host_telem_get();

		if (st) {
			for (uint32_t i = 0; i < st->num_mods; i++) {
				struct host_mod_telem *mt = &st->mods[i];

				if (!mt->active)
					continue;
				if (target_id != 0 && mt->comp_id != target_id)
					continue;
				if (mt->num_out_pins > 0 && mt->out_pins[0].exit_num_samples > 0) {
					uint32_t n = mt->out_pins[0].exit_num_samples;
					uint32_t nb = n * sizeof(int32_t);

					if (nb > max_bytes)
						nb = max_bytes;
					memcpy(dst_samples, mt->out_pins[0].exit_samples, nb);
					copied_bytes = nb;
					prsp->stream_tag = mt->comp_id;
					break;
				}
			}
		}
		pthread_mutex_unlock(&g_tb_lock);

		prsp->dma_buffer_size = copied_bytes;
		prsp->num_points = copied_bytes / sizeof(int32_t);
		int ret = send_reply(sock, SOF_NET_OP_PROBE_CTL, req_id, 0,
				     out_buf, sizeof(*preq) + copied_bytes);
		free(out_buf);
		return ret;
	}

	uint32_t cap = (preq->num_points && preq->num_points <= 64) ? preq->num_points : 64;
	uint8_t *out_buf = calloc(1, sizeof(*preq) + cap * sizeof(struct sof_cdev_probe_point));

	if (!out_buf)
		return send_reply(sock, SOF_NET_OP_PROBE_CTL, req_id, -ENOMEM, NULL, 0);

	struct sof_net_probe_req *prsp = (struct sof_net_probe_req *)out_buf;
	struct sof_cdev_probe_point *pts = (struct sof_cdev_probe_point *)(out_buf + sizeof(*preq));
	uint32_t count = 0;

	*prsp = *preq;

	pthread_mutex_lock(&g_tb_lock);
	if ((preq->op == SOF_PROBE_OP_INFO_AVAILABLE || preq->op == SOF_PROBE_OP_INFO_ACTIVE) &&
	    sof_get() && sof_get()->ipc) {
		struct host_sched_telemetry *st = host_telem_get();
		struct list_item *clist;

		list_for_item(clist, &sof_get()->ipc->comp_list) {
			struct ipc_comp_dev *icd = container_of(clist, struct ipc_comp_dev, list);
			struct host_mod_telem *mt = NULL;
			uint32_t i;

			if (icd->type != COMP_TYPE_COMPONENT || !icd->cd || count >= cap)
				continue;

			struct tplg_comp_info *ci = find_comp_info_by_ipc_id(g_tp, icd->id);
			const char *wname = (ci && ci->name) ? ci->name : "sof_widget";

			if (st) {
				for (i = 0; i < st->num_mods; i++) {
					if (st->mods[i].active && st->mods[i].comp_id == icd->id) {
						mt = &st->mods[i];
						break;
					}
				}
			}

			pts[count].buffer_id = icd->id;
			pts[count].purpose = 0; /* EXTRACTION */
			pts[count].stream_tag = (uint32_t)(icd->cd->pipeline ?
							   icd->cd->pipeline->pipeline_id : 0);
			if (mt && mt->num_out_pins > 0 && mt->out_pins[0].active) {
				snprintf(pts[count].widget_name, sizeof(pts[count].widget_name),
					 "%.15s %uB %ddB",
					 wname,
					 mt->out_pins[0].last_bytes_transferred & 0xFFFFU,
					 (int)(mt->out_pins[0].exit_rms_dbfs_x100 / 100));
			} else {
				snprintf(pts[count].widget_name, sizeof(pts[count].widget_name),
					 "%s", wname);
			}
			count++;
		}
	}
	pthread_mutex_unlock(&g_tb_lock);

	prsp->num_points = count;
	uint32_t rsp_len = sizeof(*prsp) + count * sizeof(*pts);
	int ret = send_reply(sock, SOF_NET_OP_PROBE_CTL, req_id, 0, out_buf, rsp_len);

	free(out_buf);
	return ret;
}

static int handle_pcm_ctl(int sock, uint32_t req_id, const uint8_t *req_buf, uint32_t req_len)
{
	/* Accept v1 56-byte requests and extended 72-byte requests */
	if (req_len < 56)
		return send_reply(sock, SOF_NET_OP_PCM_CTL, req_id, -EINVAL, NULL, 0);

	struct sof_net_pcm_req req;
	struct sof_net_pcm_rsp rsp;
	int status = 0;

	memset(&req, 0, sizeof(req));
	memcpy(&req, req_buf, req_len < sizeof(req) ? req_len : sizeof(req));
	memset(&rsp, 0, sizeof(rsp));

	switch (req.op) {
	case SOF_PCM_OP_STATUS:
		break;

	case SOF_PCM_OP_START_TONE:
		if (req.device[0])
			snprintf(g_pcm_device, sizeof(g_pcm_device), "%s", req.device);
		if (req.freq_hz)
			g_tone_freq_hz = req.freq_hz;
		if (req.rate)
			g_pcm_rate = req.rate;
		if (req.channels)
			g_pcm_channels = req.channels;
		g_pcm_api_used = req.api_backend ? req.api_backend : SOF_AUDIO_API_ALSA;
		g_pcm_format = req.format ? req.format : SOF_PCM_FMT_S16_LE;
		g_tone_running = true;
		break;

	case SOF_PCM_OP_STOP_TONE:
		g_tone_running = false;
		break;

	case SOF_PCM_OP_PLAY_BUFFER: {
		if (req.device[0])
			snprintf(g_pcm_device, sizeof(g_pcm_device), "%s", req.device);
		if (req.rate)
			g_pcm_rate = req.rate;
		if (req.channels)
			g_pcm_channels = req.channels;
		g_pcm_api_used = req.api_backend ? req.api_backend : SOF_AUDIO_API_ALSA;
		g_pcm_format = req.format ? req.format : SOF_PCM_FMT_S16_LE;
		if (req.data_size > 0 && req_len >= sizeof(req) + req.data_size) {
			const int16_t *samples = (const int16_t *)(req_buf + sizeof(req));
			size_t sample_cnt = req.data_size / sizeof(int16_t);

			tb_remote_record_output_samples_s16(samples, sample_cnt,
							    (int)g_pcm_channels,
							    (int)g_pcm_rate);
		}
		break;
	}

	case SOF_PCM_OP_CAPTURE_BUFFER: {
		uint32_t rate = g_pcm_rate ? g_pcm_rate : 48000;
		uint32_t ch = g_pcm_channels ? g_pcm_channels : 2;
		uint32_t dur_ms = (req.duration_ms && req.duration_ms <= 2000) ?
				  req.duration_ms : 200;
		size_t req_samples = (size_t)((rate * dur_ms) / 1000) * ch;

		if (req_samples > TB_PCM_CAPTURE_RING_SAMPLES)
			req_samples = TB_PCM_CAPTURE_RING_SAMPLES;

		uint8_t *out_buf = calloc(1, sizeof(rsp) + req_samples * sizeof(int16_t));

		if (!out_buf)
			return send_reply(sock, SOF_NET_OP_PCM_CTL, req_id, -ENOMEM, NULL, 0);

		int16_t *dst = (int16_t *)(out_buf + sizeof(rsp));
		size_t copy_samples;

		pthread_mutex_lock(&g_pcm_ring_lock);
		copy_samples = (req_samples <= g_pcm_ring_avail) ? req_samples : g_pcm_ring_avail;
		size_t start_idx = (g_pcm_ring_write_idx + TB_PCM_CAPTURE_RING_SAMPLES -
				    copy_samples) % TB_PCM_CAPTURE_RING_SAMPLES;
		for (size_t i = 0; i < copy_samples; i++)
			dst[i] = g_pcm_capture_ring[(start_idx + i) % TB_PCM_CAPTURE_RING_SAMPLES];

		struct sof_net_pcm_rsp *crsp = (struct sof_net_pcm_rsp *)out_buf;

		crsp->tone_running = g_tone_running ? 1 : 0;
		snprintf(crsp->active_device, sizeof(crsp->active_device), "%s", g_pcm_device);
		crsp->rate = rate;
		crsp->channels = ch;
		crsp->freq_hz = g_tone_freq_hz;
		crsp->frames_played = g_pcm_frames_processed;
		crsp->rms_level = g_pcm_last_rms;
		crsp->peak_level = g_pcm_last_peak;
		crsp->data_size = (uint32_t)(copy_samples * sizeof(int16_t));
		crsp->api_used = g_pcm_api_used;
		crsp->format = req.format ? req.format : g_pcm_format;
		crsp->xruns = g_pcm_xruns;
		pthread_mutex_unlock(&g_pcm_ring_lock);

		int ret = send_reply(sock, SOF_NET_OP_PCM_CTL, req_id, 0,
				     out_buf, sizeof(rsp) + crsp->data_size);
		free(out_buf);
		return ret;
	}

	default:
		status = -EINVAL;
		break;
	}

	pthread_mutex_lock(&g_pcm_ring_lock);
	rsp.tone_running = g_tone_running ? 1 : 0;
	snprintf(rsp.active_device, sizeof(rsp.active_device), "%s", g_pcm_device);
	rsp.rate = g_pcm_rate;
	rsp.channels = g_pcm_channels;
	rsp.freq_hz = g_tone_freq_hz;
	rsp.frames_played = g_pcm_frames_processed;
	rsp.rms_level = g_pcm_last_rms;
	rsp.peak_level = g_pcm_last_peak;
	rsp.data_size = 0;
	rsp.api_used = g_pcm_api_used;
	rsp.format = g_pcm_format;
	rsp.xruns = g_pcm_xruns;
	pthread_mutex_unlock(&g_pcm_ring_lock);

	return send_reply(sock, SOF_NET_OP_PCM_CTL, req_id, status, &rsp, sizeof(rsp));
}

/* -------------------------------------------------------------------------
 * Sound Card, PCM Capabilities, DAPM Graph, Compress & Kcontrol Handlers
 * -------------------------------------------------------------------------
 */

static uint32_t collect_tb_pcm_nodes(struct testbench_prm *tp,
				     struct sof_net_pcm_node_info *nodes,
				     uint32_t max_nodes,
				     uint32_t *out_num_pcm_devs)
{
	uint32_t n_nodes = 0;
	uint32_t n_devs = 0;
	const char *state_str = g_dsp_paused ? "PAUSED" : "RUNNING";

	if (tp && tp->pcm_list.next && !list_is_empty(&tp->pcm_list)) {
		struct list_item *item;

		list_for_item(item, &tp->pcm_list) {
			struct tplg_pcm_info *pi = container_of(item, struct tplg_pcm_info, item);
			bool has_pb = pi->playback_host || pi->playback_pipeline_list.count > 0;
			bool has_cp = pi->capture_host || pi->capture_pipeline_list.count > 0;

			n_devs++;
			if (!has_pb && !has_cp)
				has_pb = true;

			if (has_pb && n_nodes < max_nodes) {
				struct sof_net_pcm_node_info *pn = &nodes[n_nodes++];

				memset(pn, 0, sizeof(*pn));
				pn->card_id = 0;
				pn->device_id = (uint32_t)pi->id;
				pn->direction = SOF_STREAM_DIR_PLAYBACK;
				pn->is_compress = 0;
				snprintf(pn->id, sizeof(pn->id), "%s (*)",
					 pi->name ? pi->name : "Port0");
				snprintf(pn->name, sizeof(pn->name), "%s",
					 pi->name ? pi->name : "Port0");
				pn->subdevices_count = 1;
				pn->subdevices_avail = 1;
				snprintf(pn->state_str, sizeof(pn->state_str), "%s", state_str);
				pn->is_running = g_dsp_paused ? 0 : 1;
				snprintf(pn->active_format, sizeof(pn->active_format), "S16_LE");
				pn->active_rate = g_pcm_rate ? g_pcm_rate : 48000;
				pn->active_channels = g_pcm_channels ? g_pcm_channels : 2;
				pn->period_size = 48;
				pn->buffer_size = 192;
				pn->hw_ptr = g_pcm_frames_processed;
				pn->appl_ptr = g_pcm_frames_processed;
			}
			if (has_cp && n_nodes < max_nodes) {
				struct sof_net_pcm_node_info *cn = &nodes[n_nodes++];

				memset(cn, 0, sizeof(*cn));
				cn->card_id = 0;
				cn->device_id = (uint32_t)pi->id;
				cn->direction = SOF_STREAM_DIR_CAPTURE;
				cn->is_compress = 0;
				snprintf(cn->id, sizeof(cn->id), "%s (*)",
					 pi->name ? pi->name : "Port0");
				snprintf(cn->name, sizeof(cn->name), "%s",
					 pi->name ? pi->name : "Port0");
				cn->subdevices_count = 1;
				cn->subdevices_avail = 1;
				snprintf(cn->state_str, sizeof(cn->state_str), "%s", state_str);
				cn->is_running = g_dsp_paused ? 0 : 1;
				snprintf(cn->active_format, sizeof(cn->active_format), "S16_LE");
				cn->active_rate = g_pcm_rate ? g_pcm_rate : 48000;
				cn->active_channels = g_pcm_channels ? g_pcm_channels : 2;
				cn->period_size = 48;
				cn->buffer_size = 192;
				cn->hw_ptr = g_pcm_frames_processed;
				cn->appl_ptr = g_pcm_frames_processed;
			}
		}
	}

	if (n_nodes == 0) {
		n_devs = 1;
		for (uint32_t dir = 0; dir < 2 && n_nodes < max_nodes; dir++) {
			struct sof_net_pcm_node_info *pn = &nodes[n_nodes++];

			memset(pn, 0, sizeof(*pn));
			pn->card_id = 0;
			pn->device_id = 0;
			pn->direction = dir;
			pn->is_compress = 0;
			snprintf(pn->id, sizeof(pn->id),
				 dir == 0 ? "Testbench Playback (*)" : "Testbench Capture (*)");
			snprintf(pn->name, sizeof(pn->name),
				 dir == 0 ? "Testbench Playback" : "Testbench Capture");
			pn->subdevices_count = 1;
			pn->subdevices_avail = 1;
			snprintf(pn->state_str, sizeof(pn->state_str), "%s", state_str);
			pn->is_running = g_dsp_paused ? 0 : 1;
			snprintf(pn->active_format, sizeof(pn->active_format), "S16_LE");
			pn->active_rate = g_pcm_rate ? g_pcm_rate : 48000;
			pn->active_channels = g_pcm_channels ? g_pcm_channels : 2;
			pn->period_size = 48;
			pn->buffer_size = 192;
			pn->hw_ptr = g_pcm_frames_processed;
			pn->appl_ptr = g_pcm_frames_processed;
		}
	}

	/* Add 1 virtual Compress Offload node */
	if (n_nodes < max_nodes) {
		struct sof_net_pcm_node_info *cn = &nodes[n_nodes++];

		memset(cn, 0, sizeof(*cn));
		cn->card_id = 0;
		cn->device_id = 0;
		cn->direction = SOF_STREAM_DIR_PLAYBACK;
		cn->is_compress = 1;
		snprintf(cn->id, sizeof(cn->id), "Testbench Compr (*)");
		snprintf(cn->name, sizeof(cn->name), "SOF Testbench Compress");
		cn->subdevices_count = 1;
		cn->subdevices_avail = 1;
		snprintf(cn->state_str, sizeof(cn->state_str), "CLOSED");
	}

	if (out_num_pcm_devs)
		*out_num_pcm_devs = n_devs;
	return n_nodes;
}

static int handle_card_list(int sock, uint32_t req_id)
{
	struct sof_net_pcm_node_info nodes[64];
	struct sof_net_card_info card;
	struct sof_net_card_list_hdr hdr;
	uint32_t n_devs = 0;
	uint32_t n_nodes = 0;

	memset(&hdr, 0, sizeof(hdr));
	memset(&card, 0, sizeof(card));
	memset(nodes, 0, sizeof(nodes));

	pthread_mutex_lock(&g_tb_lock);
	n_nodes = collect_tb_pcm_nodes(g_tp, nodes, 64, &n_devs);

	const char *base_tplg = "headless";

	if (g_tp && g_tp->tplg_file) {
		const char *slash = strrchr(g_tp->tplg_file, '/');

		base_tplg = slash ? (slash + 1) : g_tp->tplg_file;
	}

	card.card_id = 0;
	snprintf(card.id, sizeof(card.id), "softestbench4");
	snprintf(card.driver, sizeof(card.driver), "sof-testbench4");
	snprintf(card.name, sizeof(card.name), "SOF Testbench IPC4");
	snprintf(card.longname, sizeof(card.longname),
		 "SOF Testbench IPC4 Virtual DSP (%s)", base_tplg);
	snprintf(card.mixername, sizeof(card.mixername), "SOF Testbench Mixer");
	snprintf(card.tplg_file, sizeof(card.tplg_file), "%s", base_tplg);
	card.num_pcm_streams = n_nodes > 0 ? (n_nodes - 1) : n_devs;
	card.num_compr_streams = 1;
	card.num_kcontrols = g_tp ? g_tp->glb_ctx.num_ctls : 0;
	card.active_streams = g_dsp_paused ? 0 : (g_tp ? (uint32_t)g_tp->pipeline_num : 1);
	pthread_mutex_unlock(&g_tb_lock);

	hdr.num_cards = 1;
	hdr.num_pcm_nodes = n_nodes;

	size_t total_len = sizeof(hdr) + sizeof(card) + n_nodes * sizeof(nodes[0]);
	uint8_t *out_buf = calloc(1, total_len);

	if (!out_buf)
		return send_reply(sock, SOF_NET_OP_CARD_LIST, req_id, -ENOMEM, NULL, 0);

	memcpy(out_buf, &hdr, sizeof(hdr));
	memcpy(out_buf + sizeof(hdr), &card, sizeof(card));
	if (n_nodes > 0)
		memcpy(out_buf + sizeof(hdr) + sizeof(card), nodes, n_nodes * sizeof(nodes[0]));

	int ret = send_reply(sock, SOF_NET_OP_CARD_LIST, req_id, 0, out_buf, (uint32_t)total_len);

	free(out_buf);
	return ret;
}

static int handle_pcm_caps(int sock, uint32_t req_id, const uint8_t *req_buf, uint32_t req_len)
{
	struct sof_net_pcm_caps_req creq = {
		.card_id = 0xFFFFFFFFU,
		.device_id = 0xFFFFFFFFU,
		.dir_mask = SOF_CAPS_DIR_ALL,
		.api_backend = SOF_AUDIO_API_ALSA,
	};

	if (req_len >= sizeof(creq))
		memcpy(&creq, req_buf, sizeof(creq));

	struct sof_net_pcm_node_info nodes[64];
	struct sof_net_pcm_caps caps[64];
	uint32_t n_nodes = 0;
	uint32_t n_caps = 0;

	memset(nodes, 0, sizeof(nodes));
	memset(caps, 0, sizeof(caps));

	pthread_mutex_lock(&g_tb_lock);
	n_nodes = collect_tb_pcm_nodes(g_tp, nodes, 64, NULL);
	pthread_mutex_unlock(&g_tb_lock);

	for (uint32_t i = 0; i < n_nodes && n_caps < 64; i++) {
		if (creq.card_id != 0xFFFFFFFFU && nodes[i].card_id != creq.card_id)
			continue;
		if (creq.device_id != 0xFFFFFFFFU && nodes[i].device_id != creq.device_id)
			continue;
		if (nodes[i].is_compress) {
			if (!(creq.dir_mask & SOF_CAPS_DIR_COMPRESS))
				continue;
		} else {
			if (!(creq.dir_mask & (1U << nodes[i].direction)))
				continue;
		}

		struct sof_net_pcm_caps *cp = &caps[n_caps++];

		cp->card_id = nodes[i].card_id;
		cp->device_id = nodes[i].device_id;
		cp->direction = nodes[i].direction;
		cp->is_compress = nodes[i].is_compress;
		cp->api_used = creq.api_backend ? creq.api_backend : SOF_AUDIO_API_ALSA;
		cp->status = 0;
		snprintf(cp->id, sizeof(cp->id), "%s", nodes[i].id);
		snprintf(cp->name, sizeof(cp->name), "%s", nodes[i].name);

		if (cp->is_compress) {
			cp->min_fragment_size = 1024;
			cp->max_fragment_size = 65536;
			cp->min_fragments = 2;
			cp->max_fragments = 16;
			cp->num_codecs = 3;
			cp->codecs[0] = 1; /* PCM */
			cp->codecs[1] = 2; /* MP3 */
			cp->codecs[2] = 3; /* AAC */
			snprintf(cp->formats_str, sizeof(cp->formats_str), "PCM MP3 AAC");
		} else {
			cp->formats_mask = SOF_FMTBIT_S16_LE | SOF_FMTBIT_S24_LE |
					   SOF_FMTBIT_S32_LE | SOF_FMTBIT_FLOAT_LE;
			cp->access_mask = 0x09; /* MMAP_INTERLEAVED | RW_INTERLEAVED */
			cp->rates_mask = SOF_RATEBIT_8000 | SOF_RATEBIT_16000 |
					 SOF_RATEBIT_32000 | SOF_RATEBIT_44100 |
					 SOF_RATEBIT_48000 | SOF_RATEBIT_96000 |
					 SOF_RATEBIT_192000;
			cp->rate_min = 8000;
			cp->rate_max = 192000;
			cp->channels_min = 1;
			cp->channels_max = 8;
			cp->sample_bits_min = 16;
			cp->sample_bits_max = 32;
			cp->period_size_min = 48;
			cp->period_size_max = 16384;
			cp->period_bytes_min = 192;
			cp->period_bytes_max = 65536;
			cp->periods_min = 2;
			cp->periods_max = 32;
			cp->buffer_size_min = 96;
			cp->buffer_size_max = 65536;
			cp->buffer_bytes_min = 384;
			cp->buffer_bytes_max = 262144;
			snprintf(cp->formats_str, sizeof(cp->formats_str),
				 "S16_LE S24_LE S32_LE FLOAT_LE");
			snprintf(cp->rates_str, sizeof(cp->rates_str),
				 "8000 16000 32000 44100 48000 96000 192000");
		}
	}

	struct sof_net_pcm_caps_hdr hdr = {
		.num_entries = n_caps,
		.api_used = creq.api_backend ? creq.api_backend : SOF_AUDIO_API_ALSA,
	};
	size_t total_len = sizeof(hdr) + n_caps * sizeof(caps[0]);
	uint8_t *out_buf = calloc(1, total_len);

	if (!out_buf)
		return send_reply(sock, SOF_NET_OP_PCM_CAPS, req_id, -ENOMEM, NULL, 0);

	memcpy(out_buf, &hdr, sizeof(hdr));
	if (n_caps > 0)
		memcpy(out_buf + sizeof(hdr), caps, n_caps * sizeof(caps[0]));

	int ret = send_reply(sock, SOF_NET_OP_PCM_CAPS, req_id, 0, out_buf, (uint32_t)total_len);

	free(out_buf);
	return ret;
}

static void classify_tb_dapm_widget(const struct tplg_comp_info *ci,
				    char *wtype_out, size_t wtype_cap,
				    uint16_t *col_out)
{
	*col_out = 1;
	snprintf(wtype_out, wtype_cap, "widget");

	if (!ci)
		return;

	switch (ci->type) {
	case SND_SOC_TPLG_DAPM_AIF_IN:
		snprintf(wtype_out, wtype_cap, "aif_in");
		*col_out = 0;
		break;
	case SND_SOC_TPLG_DAPM_AIF_OUT:
		snprintf(wtype_out, wtype_cap, "aif_out");
		*col_out = 0;
		break;
	case SND_SOC_TPLG_DAPM_DAI_IN:
		snprintf(wtype_out, wtype_cap, "dai_in");
		*col_out = 3;
		break;
	case SND_SOC_TPLG_DAPM_DAI_OUT:
		snprintf(wtype_out, wtype_cap, "dai_out");
		*col_out = 3;
		break;
	case SND_SOC_TPLG_DAPM_PGA:
		snprintf(wtype_out, wtype_cap, "pga");
		*col_out = 1;
		break;
	case SND_SOC_TPLG_DAPM_MIXER:
		snprintf(wtype_out, wtype_cap, "mixer");
		*col_out = 2;
		break;
	case SND_SOC_TPLG_DAPM_MUX:
		snprintf(wtype_out, wtype_cap, "mux");
		*col_out = 2;
		break;
	case SND_SOC_TPLG_DAPM_BUFFER:
		snprintf(wtype_out, wtype_cap, "buffer");
		*col_out = 1;
		break;
	case SND_SOC_TPLG_DAPM_SCHEDULER:
		snprintf(wtype_out, wtype_cap, "scheduler");
		*col_out = 1;
		break;
	case SND_SOC_TPLG_DAPM_EFFECT:
		snprintf(wtype_out, wtype_cap, "effect");
		*col_out = 1;
		break;
	case SND_SOC_TPLG_DAPM_SRC:
		snprintf(wtype_out, wtype_cap, "src");
		*col_out = 1;
		break;
	case SND_SOC_TPLG_DAPM_ASRC:
		snprintf(wtype_out, wtype_cap, "asrc");
		*col_out = 1;
		break;
	default:
		break;
	}

	if (ci->name) {
		if (strncmp(ci->name, "host-copier", 11) == 0)
			*col_out = 0;
		else if (strncmp(ci->name, "dai-copier", 10) == 0)
			*col_out = 3;
	}
}

static int handle_dapm_graph(int sock, uint32_t req_id, const uint8_t *req_buf, uint32_t req_len)
{
	(void)req_buf;
	(void)req_len;

	uint32_t max_nodes = 256;
	uint32_t max_edges = 512;
	uint32_t max_links = 32;

	struct sof_net_dapm_comp_status comp;
	struct sof_net_dapm_node *nodes = calloc(max_nodes, sizeof(*nodes));
	struct sof_net_dapm_edge *edges = calloc(max_edges, sizeof(*edges));
	struct sof_net_dpcm_link_state *links = calloc(max_links, sizeof(*links));
	const struct tplg_comp_info **ci_ptrs = calloc(max_nodes, sizeof(*ci_ptrs));

	if (!nodes || !edges || !links || !ci_ptrs) {
		free(nodes);
		free(edges);
		free(links);
		free(ci_ptrs);
		return send_reply(sock, SOF_NET_OP_DAPM_GRAPH, req_id, -ENOMEM, NULL, 0);
	}

	uint32_t n_nodes = 0;
	uint32_t n_powered = 0;
	uint32_t n_edges = 0;
	uint32_t n_active_edges = 0;
	uint32_t n_links = 0;

	pthread_mutex_lock(&g_tb_lock);

	const char *base_tplg = "headless";

	if (g_tp && g_tp->tplg_file) {
		const char *slash = strrchr(g_tp->tplg_file, '/');

		base_tplg = slash ? (slash + 1) : g_tp->tplg_file;
	}

	if (g_tp && g_tp->widget_list.next && !list_is_empty(&g_tp->widget_list)) {
		struct list_item *item;

		list_for_item(item, &g_tp->widget_list) {
			struct tplg_comp_info *ci = container_of(item, struct tplg_comp_info, item);

			if (n_nodes >= max_nodes)
				break;

			struct sof_net_dapm_node *nd = &nodes[n_nodes];
			uint16_t col = 1;

			ci_ptrs[n_nodes] = ci;
			nd->node_id = n_nodes;
			snprintf(nd->name, sizeof(nd->name), "%s",
				 ci->name ? ci->name : "unnamed");
			snprintf(nd->component, sizeof(nd->component), "sof-testbench4");
			snprintf(nd->stream_name, sizeof(nd->stream_name), "%s",
				 ci->stream_name ? ci->stream_name : "");
			classify_tb_dapm_widget(ci, nd->widget_type, sizeof(nd->widget_type), &col);
			nd->layout_col = col;
			nd->power = (ci->set_up && !g_dsp_paused &&
				     tb_is_pipeline_enabled(g_tp, ci->pipeline_id)) ? 1 : 0;
			if (nd->power)
				n_powered++;
			nd->stream_active = nd->power;
			nd->in_ep = 1;
			nd->out_ep = 1;
			nd->reg = -1;
			nd->pipeline_id = ci->pipeline_id;
			nd->layout_row = (uint16_t)(ci->pipeline_id >= 0 ? ci->pipeline_id : 0);

			/* Attach kcontrols belonging to this widget */
			for (uint32_t k = 0; k < g_tp->glb_ctx.num_ctls &&
					     nd->num_kcontrols < 4; k++) {
				struct tb_ctl *ctl = &g_tp->glb_ctx.ctl[k];

				if (ctl->comp_info == ci ||
				    ((int)ctl->module_id == ci->module_id &&
				     (int)ctl->instance_id == ci->instance_id &&
				     ci->module_id > 0)) {
					uint32_t slot = nd->num_kcontrols++;

					nd->kcontrol_numids[slot] = k + 1;
					if (slot < 2)
						snprintf(nd->kcontrol_names[slot],
							 sizeof(nd->kcontrol_names[slot]),
							 "%.47s", ctl->name);
				}
			}
			n_nodes++;
		}
	}

	if (g_tp && g_tp->route_list.next && !list_is_empty(&g_tp->route_list)) {
		struct list_item *item;

		list_for_item(item, &g_tp->route_list) {
			struct tplg_route_info *rt =
				container_of(item, struct tplg_route_info, item);

			if (!rt->source || !rt->sink || n_edges >= max_edges)
				continue;

			int32_t src_idx = -1;
			int32_t dst_idx = -1;

			for (uint32_t i = 0; i < n_nodes; i++) {
				if (ci_ptrs[i] == rt->source)
					src_idx = (int32_t)i;
				if (ci_ptrs[i] == rt->sink)
					dst_idx = (int32_t)i;
			}
			if (src_idx >= 0 && dst_idx >= 0) {
				struct sof_net_dapm_edge *ed = &edges[n_edges];

				ed->edge_id = n_edges++;
				ed->src_node_id = src_idx;
				ed->dst_node_id = dst_idx;
				snprintf(ed->src_name, sizeof(ed->src_name), "%s",
					 rt->source->name ? rt->source->name : "src");
				snprintf(ed->dst_name, sizeof(ed->dst_name), "%s",
					 rt->sink->name ? rt->sink->name : "dst");
				snprintf(ed->src_comp, sizeof(ed->src_comp), "sof-testbench4");
				snprintf(ed->dst_comp, sizeof(ed->dst_comp), "sof-testbench4");
				snprintf(ed->control_name, sizeof(ed->control_name), "static");
				ed->active = (rt->set_up && !g_dsp_paused) ? 1 : 0;
				if (ed->active)
					n_active_edges++;
			}
		}
	}

	if (g_tp && g_tp->pcm_list.next && !list_is_empty(&g_tp->pcm_list)) {
		struct list_item *item;

		list_for_item(item, &g_tp->pcm_list) {
			struct tplg_pcm_info *pi = container_of(item, struct tplg_pcm_info, item);

			if ((pi->playback_host || pi->playback_pipeline_list.count > 0) &&
			    n_links < max_links) {
				struct sof_net_dpcm_link_state *lk = &links[n_links++];

				snprintf(lk->fe_name, sizeof(lk->fe_name), "%s",
					 pi->name ? pi->name : "Port0");
				lk->direction = SOF_STREAM_DIR_PLAYBACK;
				snprintf(lk->state_str, sizeof(lk->state_str), "%s",
					 g_dsp_paused ? "PAUSED" : "RUNNING");
				snprintf(lk->fe_format, sizeof(lk->fe_format), "S16_LE");
				lk->fe_channels = g_pcm_channels ? g_pcm_channels : 2;
				lk->fe_rate = g_pcm_rate ? g_pcm_rate : 48000;
				lk->num_backends = 1;
				snprintf(lk->be_names[0], sizeof(lk->be_names[0]), "SSP0-Codec");
				snprintf(lk->be_states[0], sizeof(lk->be_states[0]), "%s",
					 g_dsp_paused ? "PAUSED" : "RUNNING");
				snprintf(lk->be_formats[0], sizeof(lk->be_formats[0]), "S16_LE");
				lk->be_channels[0] = lk->fe_channels;
				lk->be_rates[0] = lk->fe_rate;
			}
			if ((pi->capture_host || pi->capture_pipeline_list.count > 0) &&
			    n_links < max_links) {
				struct sof_net_dpcm_link_state *lk = &links[n_links++];

				snprintf(lk->fe_name, sizeof(lk->fe_name), "%s",
					 pi->name ? pi->name : "Port0");
				lk->direction = SOF_STREAM_DIR_CAPTURE;
				snprintf(lk->state_str, sizeof(lk->state_str), "%s",
					 g_dsp_paused ? "PAUSED" : "RUNNING");
				snprintf(lk->fe_format, sizeof(lk->fe_format), "S16_LE");
				lk->fe_channels = g_pcm_channels ? g_pcm_channels : 2;
				lk->fe_rate = g_pcm_rate ? g_pcm_rate : 48000;
				lk->num_backends = 1;
				snprintf(lk->be_names[0], sizeof(lk->be_names[0]), "SSP0-Codec");
				snprintf(lk->be_states[0], sizeof(lk->be_states[0]), "%s",
					 g_dsp_paused ? "PAUSED" : "RUNNING");
				snprintf(lk->be_formats[0], sizeof(lk->be_formats[0]), "S16_LE");
				lk->be_channels[0] = lk->fe_channels;
				lk->be_rates[0] = lk->fe_rate;
			}
		}
	}

	memset(&comp, 0, sizeof(comp));
	snprintf(comp.name, sizeof(comp.name), "sof-testbench4");
	snprintf(comp.bias_str, sizeof(comp.bias_str), "%s", g_dsp_paused ? "Standby" : "On");
	comp.bias_level = g_dsp_paused ? 1 : 3;
	comp.widget_count = n_nodes;
	comp.powered_count = n_powered;

	struct sof_net_dapm_graph_hdr hdr;

	memset(&hdr, 0, sizeof(hdr));
	hdr.card_id = 0;
	snprintf(hdr.card_name, sizeof(hdr.card_name), "sof-testbench4");
	snprintf(hdr.tplg_file, sizeof(hdr.tplg_file), "%s", base_tplg);
	hdr.num_components = 1;
	hdr.num_nodes = n_nodes;
	hdr.num_powered_nodes = n_powered;
	hdr.num_edges = n_edges;
	hdr.num_active_edges = n_active_edges;
	hdr.num_dpcm_links = n_links;

	pthread_mutex_unlock(&g_tb_lock);

	size_t total_len = sizeof(hdr) + sizeof(comp) +
			   n_nodes * sizeof(*nodes) +
			   n_edges * sizeof(*edges) +
			   n_links * sizeof(*links);
	uint8_t *out_buf = calloc(1, total_len);
	int ret;

	if (!out_buf) {
		ret = send_reply(sock, SOF_NET_OP_DAPM_GRAPH, req_id, -ENOMEM, NULL, 0);
	} else {
		uint8_t *p = out_buf;

		memcpy(p, &hdr, sizeof(hdr));
		p += sizeof(hdr);
		memcpy(p, &comp, sizeof(comp));
		p += sizeof(comp);
		if (n_nodes > 0) {
			memcpy(p, nodes, n_nodes * sizeof(*nodes));
			p += n_nodes * sizeof(*nodes);
		}
		if (n_edges > 0) {
			memcpy(p, edges, n_edges * sizeof(*edges));
			p += n_edges * sizeof(*edges);
		}
		if (n_links > 0)
			memcpy(p, links, n_links * sizeof(*links));

		ret = send_reply(sock, SOF_NET_OP_DAPM_GRAPH, req_id, 0,
				 out_buf, (uint32_t)total_len);
		free(out_buf);
	}

	free(nodes);
	free(edges);
	free(links);
	free(ci_ptrs);
	return ret;
}

static int handle_compr_ctl(int sock, uint32_t req_id, const uint8_t *req_buf, uint32_t req_len)
{
	if (req_len < sizeof(struct sof_net_compr_req))
		return send_reply(sock, SOF_NET_OP_COMPR_CTL, req_id, -EINVAL, NULL, 0);

	const struct sof_net_compr_req *creq = (const struct sof_net_compr_req *)req_buf;
	struct sof_net_compr_rsp rsp;

	memset(&rsp, 0, sizeof(rsp));
	rsp.status = 0;
	rsp.card_id = creq->card_id;
	rsp.device_id = creq->device_id;
	rsp.direction = creq->direction;
	rsp.codec_id = creq->codec_id ? creq->codec_id : 1;
	rsp.fragment_size = creq->fragment_size ? creq->fragment_size : 4096;
	rsp.fragments = creq->fragments ? creq->fragments : 4;
	rsp.min_fragment_size = 1024;
	rsp.max_fragment_size = 65536;
	rsp.num_codecs = 3;
	rsp.codecs[0] = 1; /* SND_AUDIOCODEC_PCM */
	rsp.codecs[1] = 2; /* SND_AUDIOCODEC_MP3 */
	rsp.codecs[2] = 3; /* SND_AUDIOCODEC_AAC */
	rsp.sampling_rate = creq->rate ? creq->rate : (g_pcm_rate ? g_pcm_rate : 48000);

	if (creq->op == SOF_COMPR_OP_PLAY_BUFFER) {
		uint32_t dur_ms = creq->duration_ms ? creq->duration_ms : 500;
		uint32_t frames = (rsp.sampling_rate * dur_ms) / 1000;

		rsp.bytes_transferred = creq->data_size ? creq->data_size : (frames * 4);
		rsp.rendered_samples = frames;
	} else if (creq->op == SOF_COMPR_OP_CAPTURE_BUFFER) {
		uint32_t cap_bytes = creq->data_size ? creq->data_size : 4096;

		if (cap_bytes > 65536)
			cap_bytes = 65536;

		uint8_t *out_buf = calloc(1, sizeof(rsp) + cap_bytes);

		if (!out_buf)
			return send_reply(sock, SOF_NET_OP_COMPR_CTL, req_id, -ENOMEM, NULL, 0);

		pthread_mutex_lock(&g_pcm_ring_lock);
		size_t avail_bytes = g_pcm_ring_avail * sizeof(int16_t);
		size_t to_copy = cap_bytes < avail_bytes ? cap_bytes : avail_bytes;

		if (to_copy > 0)
			memcpy(out_buf + sizeof(rsp), g_pcm_capture_ring, to_copy);
		pthread_mutex_unlock(&g_pcm_ring_lock);

		rsp.bytes_transferred = cap_bytes;
		rsp.rendered_samples = cap_bytes / 4;
		rsp.data_size = cap_bytes;
		memcpy(out_buf, &rsp, sizeof(rsp));

		int ret = send_reply(sock, SOF_NET_OP_COMPR_CTL, req_id, 0,
				     out_buf, sizeof(rsp) + cap_bytes);
		free(out_buf);
		return ret;
	}

	return send_reply(sock, SOF_NET_OP_COMPR_CTL, req_id, 0, &rsp, sizeof(rsp));
}

static void populate_tb_kctl_info(uint32_t idx, struct tb_ctl *ctl, struct sof_net_kctl_info *ki)
{
	struct tb_kctl_shadow *sh = (idx < TB_REMOTE_MAX_KCTLS) ? &g_kctl_shadow[idx] : NULL;

	memset(ki, 0, sizeof(*ki));
	ki->card_id = 0;
	ki->numid = idx + 1;
	ki->iface = 2; /* SNDRV_CTL_ELEM_IFACE_MIXER */
	snprintf(ki->iface_str, sizeof(ki->iface_str), "MIXER");
	ki->index = ctl->index;
	snprintf(ki->name, sizeof(ki->name), "%.63s", ctl->name);

	if (ctl->type == SND_SOC_TPLG_TYPE_MIXER) {
		uint32_t nch = ctl->mixer_ctl.num_channels ?
			       (uint32_t)ctl->mixer_ctl.num_channels : 2;
		int64_t max_v = ctl->mixer_ctl.max ? ctl->mixer_ctl.max : 32;

		if (nch > 8)
			nch = 8;
		ki->count = nch;
		ki->type = (max_v == 1) ? SOF_KCTL_TYPE_BOOLEAN : SOF_KCTL_TYPE_INTEGER;
		snprintf(ki->type_str, sizeof(ki->type_str), "%s",
			 (max_v == 1) ? "BOOLEAN" : "INTEGER");
		ki->min_val = ctl->mixer_ctl.min;
		ki->max_val = max_v;
		ki->step_val = 1;
		ki->access_mask = SOF_KCTL_ACC_READ | SOF_KCTL_ACC_WRITE | SOF_KCTL_ACC_TLV_READ;
		snprintf(ki->access_str, sizeof(ki->access_str), "rwR");

		if (ctl->mixer_ctl.hdr.tlv.size > 0) {
			ki->has_db_scale = 1;
			ki->db_min_cdb = (int32_t)ctl->mixer_ctl.hdr.tlv.scale.min;
			ki->db_step_cdb = (int32_t)(ctl->mixer_ctl.hdr.tlv.scale.step ?
						    ctl->mixer_ctl.hdr.tlv.scale.step : 100);
			ki->db_mute = ctl->mixer_ctl.hdr.tlv.scale.mute ? 1 : 0;
		} else if (max_v > 1) {
			ki->has_db_scale = 1;
			ki->db_min_cdb = -5000;
			ki->db_step_cdb = 200;
			ki->db_mute = 1;
		}

		if (sh && !sh->initialized) {
			int64_t def_val = (max_v == 1) ? 1 : (max_v * 3) / 4;

			for (uint32_t c = 0; c < nch; c++)
				sh->values[c] = def_val;
			sh->initialized = true;
		}
	} else if (ctl->type == SND_SOC_TPLG_TYPE_ENUM) {
		uint32_t nch = ctl->enum_ctl.num_channels ?
			       (uint32_t)ctl->enum_ctl.num_channels : 1;
		uint32_t nitems = (uint32_t)ctl->enum_ctl.items;

		if (nch > 8)
			nch = 8;
		if (nitems > 8)
			nitems = 8;
		ki->type = SOF_KCTL_TYPE_ENUMERATED;
		snprintf(ki->type_str, sizeof(ki->type_str), "ENUM");
		ki->count = nch;
		ki->access_mask = SOF_KCTL_ACC_READ | SOF_KCTL_ACC_WRITE;
		snprintf(ki->access_str, sizeof(ki->access_str), "rw");
		ki->num_enums = nitems;
		for (uint32_t e = 0; e < nitems; e++)
			snprintf(ki->enum_names[e], sizeof(ki->enum_names[e]),
				 "%s", ctl->enum_ctl.texts[e]);
		if (sh && !sh->initialized)
			sh->initialized = true;
	} else {
		ki->type = SOF_KCTL_TYPE_BYTES;
		snprintf(ki->type_str, sizeof(ki->type_str), "BYTES");
		ki->count = ctl->bytes_ctl.size ? (uint32_t)ctl->bytes_ctl.size : 64;
		ki->access_mask = SOF_KCTL_ACC_READ | SOF_KCTL_ACC_WRITE |
				  SOF_KCTL_ACC_TLV_READ | SOF_KCTL_ACC_TLV_WRITE;
		snprintf(ki->access_str, sizeof(ki->access_str), "rwRW");
		if (sh && !sh->initialized) {
			if (ctl->bytes_ctl.priv.size > 0) {
				uint32_t blen = (uint32_t)ctl->bytes_ctl.priv.size;

				if (blen > sizeof(sh->bytes_data))
					blen = sizeof(sh->bytes_data);
				memcpy(sh->bytes_data, ctl->bytes_ctl.priv.data, blen);
				sh->bytes_len = blen;
			}
			sh->initialized = true;
		}
	}

	if (sh) {
		for (int c = 0; c < 8; c++)
			ki->values[c] = sh->values[c];
	}

	if (ctl->comp_info) {
		char wtype[24];
		uint16_t col = 1;

		ki->anchor_type = SOF_KCTL_ANCHOR_WIDGET;
		snprintf(ki->target_widget, sizeof(ki->target_widget), "%s",
			 ctl->comp_info->name ? ctl->comp_info->name : "");
		snprintf(ki->target_component, sizeof(ki->target_component), "sof-testbench4");
		ki->pipeline_id = ctl->comp_info->pipeline_id;
		ki->layout_row = (uint16_t)(ctl->comp_info->pipeline_id >= 0 ?
					    ctl->comp_info->pipeline_id : 0);
		classify_tb_dapm_widget(ctl->comp_info, wtype, sizeof(wtype), &col);
		ki->layout_col = col;
	} else {
		ki->anchor_type = SOF_KCTL_ANCHOR_CARD;
		snprintf(ki->target_component, sizeof(ki->target_component), "sof-testbench4");
		ki->pipeline_id = -1;
		ki->layout_col = 4;
	}
}

static int handle_kctl_list(int sock, uint32_t req_id, const uint8_t *req_buf, uint32_t req_len)
{
	uint32_t api_used = SOF_AUDIO_API_ALSA;

	if (req_len >= sizeof(struct sof_net_kctl_list_req)) {
		const struct sof_net_kctl_list_req *kreq =
			(const struct sof_net_kctl_list_req *)req_buf;

		if (kreq->api_backend)
			api_used = kreq->api_backend;
	}

	pthread_mutex_lock(&g_tb_lock);
	uint32_t n_ctls = g_tp ? g_tp->glb_ctx.num_ctls : 0;

	if (n_ctls > TB_REMOTE_MAX_KCTLS)
		n_ctls = TB_REMOTE_MAX_KCTLS;

	size_t total_len = sizeof(struct sof_net_kctl_list_hdr) +
			   n_ctls * sizeof(struct sof_net_kctl_info);
	uint8_t *out_buf = calloc(1, total_len);

	if (!out_buf) {
		pthread_mutex_unlock(&g_tb_lock);
		return send_reply(sock, SOF_NET_OP_KCTL_LIST, req_id, -ENOMEM, NULL, 0);
	}

	struct sof_net_kctl_list_hdr *hdr = (struct sof_net_kctl_list_hdr *)out_buf;
	struct sof_net_kctl_info *items = (struct sof_net_kctl_info *)(out_buf + sizeof(*hdr));

	hdr->card_id = 0;
	hdr->api_used = api_used;
	hdr->num_kcontrols = n_ctls;

	for (uint32_t i = 0; i < n_ctls; i++)
		populate_tb_kctl_info(i, &g_tp->glb_ctx.ctl[i], &items[i]);

	pthread_mutex_unlock(&g_tb_lock);

	int ret = send_reply(sock, SOF_NET_OP_KCTL_LIST, req_id, 0, out_buf, (uint32_t)total_len);

	free(out_buf);
	return ret;
}

static int handle_kctl_io(int sock, uint16_t opcode, uint32_t req_id,
			  const uint8_t *req_buf, uint32_t req_len)
{
	if (req_len < sizeof(struct sof_net_kctl_io_req))
		return send_reply(sock, opcode, req_id, -EINVAL, NULL, 0);

	const struct sof_net_kctl_io_req *kreq = (const struct sof_net_kctl_io_req *)req_buf;
	struct sof_net_kctl_io_rsp rsp;
	int status = 0;

	memset(&rsp, 0, sizeof(rsp));

	pthread_mutex_lock(&g_tb_lock);
	uint32_t n_ctls = g_tp ? g_tp->glb_ctx.num_ctls : 0;

	if (n_ctls > TB_REMOTE_MAX_KCTLS)
		n_ctls = TB_REMOTE_MAX_KCTLS;

	int32_t found_idx = -1;

	for (uint32_t i = 0; i < n_ctls; i++) {
		if (kreq->numid > 0 && (i + 1) == kreq->numid) {
			found_idx = (int32_t)i;
			break;
		}
		if (kreq->name[0] && strcmp(g_tp->glb_ctx.ctl[i].name, kreq->name) == 0) {
			found_idx = (int32_t)i;
			break;
		}
	}

	if (found_idx < 0) {
		pthread_mutex_unlock(&g_tb_lock);
		return send_reply(sock, opcode, req_id, -ENOENT, NULL, 0);
	}

	uint32_t idx = (uint32_t)found_idx;
	struct tb_ctl *ctl = &g_tp->glb_ctx.ctl[idx];
	struct sof_net_kctl_info kinfo;

	populate_tb_kctl_info(idx, ctl, &kinfo);

	if (opcode == SOF_NET_OP_KCTL_SET) {
		struct tb_kctl_shadow *sh = &g_kctl_shadow[idx];

		if (kinfo.type == SOF_KCTL_TYPE_BYTES || kreq->bytes_len > 0) {
			uint32_t blen = kreq->bytes_len;

			if (blen > sizeof(sh->bytes_data))
				blen = sizeof(sh->bytes_data);
			memcpy(sh->bytes_data, kreq->bytes_data, blen);
			sh->bytes_len = blen;
		} else {
			uint32_t cnt = kreq->count ? kreq->count : kinfo.count;

			if (cnt > 8)
				cnt = 8;
			for (uint32_t c = 0; c < cnt; c++) {
				int64_t v = kreq->values[c];

				if (kinfo.type == SOF_KCTL_TYPE_INTEGER && kinfo.max_val > 0) {
					if (v < kinfo.min_val)
						v = kinfo.min_val;
					if (v > kinfo.max_val)
						v = kinfo.max_val;
				}
				sh->values[c] = v;
			}
			/* Propagate live control change to the active SOF DSP component */
			char cmd_line[256];

			if (ctl->type == SND_SOC_TPLG_TYPE_MIXER) {
				if (kinfo.count >= 2)
					snprintf(cmd_line, sizeof(cmd_line),
						 "cset name=\"%s\" %lld,%lld",
						 ctl->name,
						 (long long)sh->values[0],
						 (long long)sh->values[1]);
				else
					snprintf(cmd_line, sizeof(cmd_line),
						 "cset name=\"%s\" %lld",
						 ctl->name,
						 (long long)sh->values[0]);
				tb_apply_control_line(g_tp, cmd_line);
			} else if (ctl->type == SND_SOC_TPLG_TYPE_ENUM) {
				uint32_t eidx = (uint32_t)sh->values[0];

				if (eidx < kinfo.num_enums) {
					snprintf(cmd_line, sizeof(cmd_line),
						 "cset name=\"%s\" %s",
						 ctl->name, kinfo.enum_names[eidx]);
					tb_apply_control_line(g_tp, cmd_line);
				}
			}
		}
		populate_tb_kctl_info(idx, ctl, &kinfo);
	}

	rsp.status = status;
	rsp.api_used = kreq->api_backend ? kreq->api_backend : SOF_AUDIO_API_ALSA;
	rsp.info = kinfo;
	rsp.bytes_len = g_kctl_shadow[idx].bytes_len;
	if (rsp.bytes_len > 0)
		memcpy(rsp.bytes_data, g_kctl_shadow[idx].bytes_data, rsp.bytes_len);

	pthread_mutex_unlock(&g_tb_lock);

	return send_reply(sock, opcode, req_id, status, &rsp, sizeof(rsp));
}

/* -------------------------------------------------------------------------
 * Per-Client Worker Thread & Accept Loop
 * -------------------------------------------------------------------------
 */

struct tb_client_ctx {
	int sock;
};

static void *tb_client_worker_fn(void *arg)
{
	struct tb_client_ctx *ctx = (struct tb_client_ctx *)arg;
	int sock = ctx->sock;
	int one = 1;

	free(ctx);
	g_in_remote_rpc = true;
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

	while (g_srv_running) {
		struct sof_net_hdr hdr;

		if (read_exact(sock, &hdr, sizeof(hdr)) < 0)
			break;

		if (hdr.magic != SOF_NET_MAGIC || hdr.version != SOF_NET_VERSION) {
			send_reply(sock, hdr.opcode, hdr.req_id, -EPROTO, NULL, 0);
			break;
		}

		if (hdr.payload_len > (16 * 1024 * 1024)) {
			send_reply(sock, hdr.opcode, hdr.req_id, -E2BIG, NULL, 0);
			break;
		}

		uint8_t *req_buf = NULL;

		if (hdr.payload_len > 0) {
			req_buf = malloc(hdr.payload_len + 1);
			if (!req_buf)
				break;
			if (read_exact(sock, req_buf, hdr.payload_len) < 0) {
				free(req_buf);
				break;
			}
			req_buf[hdr.payload_len] = '\0';
		}

		int rc = 0;

		switch (hdr.opcode) {
		case SOF_NET_OP_PING:
			rc = send_reply(sock, hdr.opcode, hdr.req_id, 0, "PONG", 4);
			break;

		case SOF_NET_OP_GET_INFO: {
			struct sof_cdev_info info;

			pthread_mutex_lock(&g_tb_lock);
			populate_cdev_info(g_tp, &info);
			pthread_mutex_unlock(&g_tb_lock);
			rc = send_reply(sock, hdr.opcode, hdr.req_id, 0, &info, sizeof(info));
			break;
		}

		case SOF_NET_OP_PM_CTL: {
			if (hdr.payload_len < sizeof(struct sof_cdev_pm_ctl)) {
				rc = send_reply(sock, hdr.opcode, hdr.req_id, -EINVAL, NULL, 0);
				break;
			}
			struct sof_cdev_pm_ctl ctl;

			memcpy(&ctl, req_buf, sizeof(ctl));
			pthread_mutex_lock(&g_tb_lock);
			if (ctl.op == SOF_PM_OP_WAKE_HOLD || ctl.op == SOF_PM_OP_BOOT_DSP) {
				g_dsp_paused = false;
				pthread_cond_broadcast(&g_tb_cond);
			} else if (ctl.op == SOF_PM_OP_RELEASE_HOLD) {
				g_dsp_paused = true;
			} else if (ctl.op == SOF_PM_OP_CORE_GET && ctl.core_id < CONFIG_CORE_COUNT) {
				cpu_enable_core(ctl.core_id);
			} else if (ctl.op == SOF_PM_OP_CORE_PUT && ctl.core_id > 0 &&
				   ctl.core_id < CONFIG_CORE_COUNT) {
				cpu_disable_core(ctl.core_id);
			}
			ctl.fw_state = 7;
			ctl.dsp_power_state = g_dsp_paused ? 4 : 1;
			ctl.pm_runtime_active = g_dsp_paused ? 0 : 1;
			ctl.enabled_cores_mask = arch_cpu_enabled_cores();
			ctl.fd_holding_d0 = g_dsp_paused ? 0 : 1;
			pthread_mutex_unlock(&g_tb_lock);

			rc = send_reply(sock, hdr.opcode, hdr.req_id, 0, &ctl, sizeof(ctl));
			break;
		}

		case SOF_NET_OP_IPC_TX: {
			if (hdr.payload_len < sizeof(struct sof_net_ipc_tx_req)) {
				rc = send_reply(sock, hdr.opcode, hdr.req_id, -EINVAL, NULL, 0);
				break;
			}
			const struct sof_net_ipc_tx_req *ireq =
				(const struct sof_net_ipc_tx_req *)req_buf;
			if (hdr.payload_len < sizeof(*ireq) + ireq->tx_size ||
			    ireq->rx_size > 65536) {
				rc = send_reply(sock, hdr.opcode, hdr.req_id, -EINVAL, NULL, 0);
				break;
			}

			uint32_t rx_cap = ireq->rx_size <= SOF_IPC_MSG_MAX_SIZE - 8 ?
					  ireq->rx_size : 0;
			uint8_t *out_buf = calloc(1, sizeof(struct sof_net_ipc_tx_rsp) + rx_cap);

			if (!out_buf) {
				rc = send_reply(sock, hdr.opcode, hdr.req_id, -ENOMEM, NULL, 0);
				break;
			}
			struct sof_net_ipc_tx_rsp *rsp = (struct sof_net_ipc_tx_rsp *)out_buf;

			pthread_mutex_lock(&g_tb_lock);
			if (sof_get() && sof_get()->ipc && sof_get()->ipc->comp_data) {
				uint32_t *mbox = (uint32_t *)sof_get()->ipc->comp_data;
				uint32_t msg_tgt = (ireq->primary >> 30) & 0x1;
				uint32_t msg_type = (ireq->primary >> 24) & 0x1F;
				uint32_t mod_id = ireq->primary & 0xFFFFU;
				uint32_t inst_id = (ireq->primary >> 16) & 0xFFU;
				uint32_t ppl_id = (ireq->extension >> 16) & 0xFFU;
				int file_slot_idx = -1;
				uint32_t file_mode = 0;

				/* Enable target core automatically on CREATE_PIPELINE / MOD_INIT */
				if (msg_tgt == SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG &&
				    msg_type == SOF_IPC4_GLB_CREATE_PIPELINE) {
					uint32_t core = (ireq->extension >> 20) & 0xF;

					if (core < CONFIG_CORE_COUNT)
						cpu_enable_core(core);
				} else if (msg_tgt == SOF_IPC4_MESSAGE_TARGET_MODULE_MSG &&
					   msg_type == SOF_IPC4_MOD_INIT_INSTANCE) {
					uint32_t core = (ireq->extension >> 24) & 0xF;

					if (core < CONFIG_CORE_COUNT)
						cpu_enable_core(core);
				}

				memset(mbox, 0, SOF_IPC_MSG_MAX_SIZE);
				mbox[0] = ireq->primary;
				mbox[1] = ireq->extension;
				if (ireq->tx_size > 0 &&
				    ireq->tx_size + 8 <= SOF_IPC_MSG_MAX_SIZE) {
					memcpy(&mbox[2], req_buf + sizeof(*ireq), ireq->tx_size);
				}

				/* Resolve file path pointer for testbench file modules (0x9a..0x9d) */
				if (msg_tgt == SOF_IPC4_MESSAGE_TARGET_MODULE_MSG &&
				    msg_type == SOF_IPC4_MOD_INIT_INSTANCE &&
				    (mod_id == TB_FILE_OUT_AIF_MODULE_ID ||
				     mod_id == TB_FILE_IN_AIF_MODULE_ID ||
				     mod_id == TB_FILE_OUT_DAI_MODULE_ID ||
				     mod_id == TB_FILE_IN_DAI_MODULE_ID) &&
				    ireq->tx_size >= sizeof(struct ipc4_file_module_cfg) && g_tp) {
					struct ipc4_file_module_cfg *fcfg =
						(struct ipc4_file_module_cfg *)&mbox[2];
					uintptr_t raw_fn = (uintptr_t)fcfg->config.fn;

					file_mode = fcfg->config.mode;
					if (file_mode == FILE_READ) {
						file_slot_idx = (raw_fn > 0 &&
								 raw_fn <= (uintptr_t)g_tp->input_file_num) ?
								(int)(raw_fn - 1) : g_tp->input_file_index;
						if (file_slot_idx < 0 ||
						    file_slot_idx >= g_tp->input_file_num)
							file_slot_idx = 0;
						if (file_slot_idx < g_tp->input_file_num &&
						    g_tp->input_file[file_slot_idx])
							fcfg->config.fn = g_tp->input_file[file_slot_idx];
						else
							fcfg->config.fn = "/tmp/sof_tb_srv_in.raw";
					} else {
						file_slot_idx = (raw_fn > 0 &&
								 raw_fn <= (uintptr_t)g_tp->output_file_num) ?
								(int)(raw_fn - 1) : g_tp->output_file_index;
						if (file_slot_idx < 0 ||
						    file_slot_idx >= g_tp->output_file_num)
							file_slot_idx = 0;
						if (file_slot_idx < g_tp->output_file_num &&
						    g_tp->output_file[file_slot_idx])
							fcfg->config.fn = g_tp->output_file[file_slot_idx];
						else
							fcfg->config.fn = "/tmp/sof_tb_srv_out0.raw";
					}
				}

				ipc_cmd((struct ipc_cmd_hdr *)mbox);

				/* Track active file component state for IPC-created file modules */
				if (msg_tgt == SOF_IPC4_MESSAGE_TARGET_MODULE_MSG &&
				    msg_type == SOF_IPC4_MOD_INIT_INSTANCE &&
				    file_slot_idx >= 0 && (mbox[0] & 0x00FFFFFFU) == 0 && g_tp) {
					struct ipc_comp_dev *ficd =
						ipc_get_comp_by_id(sof_get()->ipc,
								   IPC4_COMP_ID(mod_id, inst_id));
					struct file_state *fstate = NULL;

					if (ficd && ficd->cd && comp_mod(ficd->cd)) {
						struct copier_data *ccd =
							module_get_private_data(comp_mod(ficd->cd));
						if (ccd && get_file_comp_data(ccd))
							fstate = &get_file_comp_data(ccd)->fs;
					}
					if (file_mode == FILE_READ &&
					    file_slot_idx < TB_MAX_INPUT_FILE_NUM) {
						g_tp->fr[file_slot_idx].id =
							IPC4_COMP_ID(mod_id, inst_id);
						g_tp->fr[file_slot_idx].instance_id = inst_id;
						g_tp->fr[file_slot_idx].pipeline_id = ppl_id;
						g_tp->fr[file_slot_idx].state = fstate;
						if (file_slot_idx >= g_tp->input_file_index)
							g_tp->input_file_index = file_slot_idx + 1;
					} else if (file_mode == FILE_WRITE &&
						   file_slot_idx < TB_MAX_OUTPUT_FILE_NUM) {
						g_tp->fw[file_slot_idx].id =
							IPC4_COMP_ID(mod_id, inst_id);
						g_tp->fw[file_slot_idx].instance_id = inst_id;
						g_tp->fw[file_slot_idx].pipeline_id = ppl_id;
						g_tp->fw[file_slot_idx].state = fstate;
						if (file_slot_idx >= g_tp->output_file_index)
							g_tp->output_file_index = file_slot_idx + 1;
					}
				}

				/* Clean up file component tracking when a pipeline is deleted */
				if (msg_tgt == SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG &&
				    msg_type == SOF_IPC4_GLB_DELETE_PIPELINE && g_tp) {
					int fi;

					for (fi = 0; fi < TB_MAX_INPUT_FILE_NUM; fi++) {
						if (g_tp->fr[fi].id >= 0 &&
						    (g_tp->fr[fi].pipeline_id == (int)inst_id ||
						     !ipc_get_comp_by_id(sof_get()->ipc,
									 g_tp->fr[fi].id))) {
							g_tp->fr[fi].id = -1;
							g_tp->fr[fi].state = NULL;
						}
					}
					for (fi = 0; fi < TB_MAX_OUTPUT_FILE_NUM; fi++) {
						if (g_tp->fw[fi].id >= 0 &&
						    (g_tp->fw[fi].pipeline_id == (int)inst_id ||
						     !ipc_get_comp_by_id(sof_get()->ipc,
									 g_tp->fw[fi].id))) {
							g_tp->fw[fi].id = -1;
							g_tp->fw[fi].state = NULL;
						}
					}
					if (list_is_empty(&sof_get()->ipc->comp_list)) {
						g_tp->input_file_index = 0;
						g_tp->output_file_index = 0;
					}
				}

				/* If SET_PIPELINE_STATE was sent, run one LL scheduler tick so
				 * the compound pipeline trigger task completes immediately.
				 */
				if (msg_tgt == SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG &&
				    msg_type == SOF_IPC4_GLB_SET_PIPELINE_STATE)
					schedule_ll_run_tasks();
				if (msg_tgt == SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG &&
				    msg_type == SOF_IPC4_GLB_CREATE_PIPELINE)
					pthread_cond_broadcast(&g_tb_cond);

				rsp->reply_primary = mbox[0];
				rsp->reply_extension = mbox[1];
				rsp->ipc_status = (int32_t)(mbox[0] & 0x00FFFFFFU);
				rsp->rx_size = rx_cap;
				if (rx_cap > 0)
					memcpy(out_buf + sizeof(*rsp), &mbox[2], rx_cap);
			}
			pthread_mutex_unlock(&g_tb_lock);

			rc = send_reply(sock, hdr.opcode, hdr.req_id, 0,
					out_buf, sizeof(*rsp) + rx_cap);
			free(out_buf);
			break;
		}

		case SOF_NET_OP_IPC_LARGE_CFG:
			rc = handle_ipc_large_cfg(sock, hdr.req_id, req_buf, hdr.payload_len);
			break;

		case SOF_NET_OP_WIN_INFO: {
			struct sof_cdev_info info;
			struct sof_cdev_win_info winfo;

			pthread_mutex_lock(&g_tb_lock);
			populate_cdev_info(g_tp, &info);
			pthread_mutex_unlock(&g_tb_lock);
			memcpy(winfo.windows, info.windows, sizeof(winfo.windows));
			memcpy(winfo.debug_slots, info.debug_slots, sizeof(winfo.debug_slots));
			rc = send_reply(sock, hdr.opcode, hdr.req_id, 0, &winfo, sizeof(winfo));
			break;
		}

		case SOF_NET_OP_WIN_READ: {
			if (hdr.payload_len < sizeof(struct sof_net_win_io_req)) {
				rc = send_reply(sock, hdr.opcode, hdr.req_id, -EINVAL, NULL, 0);
				break;
			}
			const struct sof_net_win_io_req *wreq =
				(const struct sof_net_win_io_req *)req_buf;
			if (!wreq->size || wreq->size > 65536) {
				rc = send_reply(sock, hdr.opcode, hdr.req_id, -EINVAL, NULL, 0);
				break;
			}
			uint8_t *rbuf = calloc(1, wreq->size);

			if (!rbuf) {
				rc = send_reply(sock, hdr.opcode, hdr.req_id, -ENOMEM, NULL, 0);
				break;
			}

			pthread_mutex_lock(&g_tb_lock);
			refresh_emulated_windows(g_tp);
			const uint8_t *src_win = g_win0_fw_regs;
			size_t win_cap = sizeof(g_win0_fw_regs);

			if (wreq->win_id == SOF_CDEV_WIN_OUTBOX) {
				src_win = g_win1_outbox;
				win_cap = sizeof(g_win1_outbox);
			} else if (wreq->win_id == SOF_CDEV_WIN_DEBUG) {
				src_win = g_win2_debug;
				win_cap = sizeof(g_win2_debug);
			} else if (wreq->win_id == SOF_CDEV_WIN_PERF) {
				src_win = g_win3_perf;
				win_cap = sizeof(g_win3_perf);
			} else if (wreq->win_id == SOF_CDEV_WIN_BY_SLOT_TYPE) {
				if (wreq->slot_type == SOF_CDEV_SLOT_TELEMETRY) {
					src_win = g_win2_debug + 4096;
					win_cap = 4096;
				} else if (wreq->slot_type == SOF_CDEV_SLOT_SHELL) {
					src_win = g_win2_debug + 8192;
					win_cap = 4096;
				}
			}

			if (wreq->offset < win_cap) {
				size_t avail = win_cap - wreq->offset;
				size_t to_copy = wreq->size < avail ? wreq->size : avail;

				memcpy(rbuf, src_win + wreq->offset, to_copy);
			}
			pthread_mutex_unlock(&g_tb_lock);

			rc = send_reply(sock, hdr.opcode, hdr.req_id, 0, rbuf, wreq->size);
			free(rbuf);
			break;
		}

		case SOF_NET_OP_WIN_WRITE:
			rc = send_reply(sock, hdr.opcode, hdr.req_id, 0, NULL, 0);
			break;

		case SOF_NET_OP_WINSTREAM_IO: {
			if (hdr.payload_len < sizeof(struct sof_net_winstream_req)) {
				rc = send_reply(sock, hdr.opcode, hdr.req_id, -EINVAL, NULL, 0);
				break;
			}
			const struct sof_net_winstream_req *wsreq =
				(const struct sof_net_winstream_req *)req_buf;
			if (wsreq->tx_len > 0 &&
			    hdr.payload_len >= sizeof(*wsreq) + wsreq->tx_len) {
				char cmd_line[256];
				char sh_out[4096];
				size_t clen = wsreq->tx_len < sizeof(cmd_line) - 1 ?
					      wsreq->tx_len : sizeof(cmd_line) - 1;

				memcpy(cmd_line, req_buf + sizeof(*wsreq), clen);
				cmd_line[clen] = '\0';
				/* Strip trailing CR/LF */
				while (clen > 0 && (cmd_line[clen - 1] == '\r' ||
						    cmd_line[clen - 1] == '\n'))
					cmd_line[--clen] = '\0';
				memset(sh_out, 0, sizeof(sh_out));
				execute_tb_shell_cmd(cmd_line, sh_out, sizeof(sh_out));
				g_shell_seq++;
			}

			size_t slen = strlen(g_last_shell_output);
			size_t rx_len = (wsreq->rx_buf_len > 0 && slen > 0) ?
					(slen < wsreq->rx_buf_len ? slen : wsreq->rx_buf_len) : 0;
			uint8_t *out_buf = calloc(1, sizeof(struct sof_net_winstream_rsp) + rx_len);

			if (!out_buf) {
				rc = send_reply(sock, hdr.opcode, hdr.req_id, -ENOMEM, NULL, 0);
				break;
			}
			struct sof_net_winstream_rsp *wsrsp =
				(struct sof_net_winstream_rsp *)out_buf;
			wsrsp->rx_bytes_read = (uint32_t)rx_len;
			wsrsp->seq_out = g_shell_seq;
			if (rx_len > 0) {
				memcpy(out_buf + sizeof(*wsrsp), g_last_shell_output, rx_len);
				g_last_shell_output[0] = '\0';
			}
			rc = send_reply(sock, hdr.opcode, hdr.req_id, 0,
					out_buf, sizeof(*wsrsp) + (uint32_t)rx_len);
			free(out_buf);
			break;
		}

		case SOF_NET_OP_TELEM_SNAPSHOT:
			rc = handle_telem_snapshot(sock, hdr.req_id, req_buf, hdr.payload_len);
			break;

		case SOF_NET_OP_WORKLOAD_SIM_CFG:
			rc = handle_workload_sim_cfg(sock, hdr.req_id, req_buf, hdr.payload_len);
			break;

		case SOF_NET_OP_SCHED_TRACE:
			rc = handle_sched_trace(sock, hdr.req_id, req_buf, hdr.payload_len);
			break;

		case SOF_NET_OP_LLEXT_LOAD: {
			if (hdr.payload_len < sizeof(struct sof_net_llext_req)) {
				rc = send_reply(sock, hdr.opcode, hdr.req_id, -EINVAL, NULL, 0);
				break;
			}
			const struct sof_net_llext_req *lreq =
				(const struct sof_net_llext_req *)req_buf;
			if (hdr.payload_len < sizeof(*lreq) + lreq->data_size) {
				rc = send_reply(sock, hdr.opcode, hdr.req_id, -EINVAL, NULL, 0);
				break;
			}
			struct sof_net_llext_rsp rsp;
			uint32_t lid = lreq->lib_id ? lreq->lib_id : 1;
			uint32_t slot = (lid - 1) % TB_MAX_LOADED_LLEXTS;
			const uint8_t *mod_bytes = req_buf + sizeof(*lreq);
			char tmp_path[128];
			void *dl_h = NULL;

			memset(&rsp, 0, sizeof(rsp));
			rsp.lib_id = lid;
			rsp.fw_state = 7;

			if (g_tp && g_tp->server_tmp_dir[0]) {
				snprintf(tmp_path, sizeof(tmp_path), "%s/llext_%u.so",
					 g_tp->server_tmp_dir, lid);
				FILE *fp = fopen(tmp_path, "wb");

				if (fp) {
					fwrite(mod_bytes, 1, lreq->data_size, fp);
					fclose(fp);
					dl_h = dlopen(tmp_path, RTLD_NOW | RTLD_GLOBAL);
				}
			} else {
				snprintf(tmp_path, sizeof(tmp_path),
					 "/tmp/sof-tb-llext-%u-XXXXXX.so", lid);
				int lfd = mkstemps(tmp_path, 3);

				if (lfd >= 0) {
					ssize_t wr = write(lfd, mod_bytes, lreq->data_size);

					(void)wr;
					close(lfd);
					dl_h = dlopen(tmp_path, RTLD_NOW | RTLD_GLOBAL);
				}
			}

			pthread_mutex_lock(&g_tb_lock);
			if (g_loaded_llexts[slot].dl_handle)
				dlclose(g_loaded_llexts[slot].dl_handle);
			g_loaded_llexts[slot].active = true;
			g_loaded_llexts[slot].lib_id = lid;
			g_loaded_llexts[slot].data_size = lreq->data_size;
			g_loaded_llexts[slot].dl_handle = dl_h;
			snprintf(g_loaded_llexts[slot].path, sizeof(g_loaded_llexts[slot].path),
				 "%s", tmp_path);
			snprintf(g_loaded_llexts[slot].name, sizeof(g_loaded_llexts[slot].name),
				 "llext%u", lid);
			pthread_mutex_unlock(&g_tb_lock);

			snprintf(rsp.shell_out, sizeof(rsp.shell_out),
				 "Testbench loaded module image (%u bytes) at lib_id %u (%s)\n",
				 lreq->data_size, rsp.lib_id,
				 dl_h ? "dlopen active" : "registered in LLEXT table");
			rc = send_reply(sock, hdr.opcode, hdr.req_id, 0, &rsp, sizeof(rsp));
			break;
		}

		case SOF_NET_OP_SHELL_EXEC: {
			if (hdr.payload_len < sizeof(struct sof_net_shell_req)) {
				rc = send_reply(sock, hdr.opcode, hdr.req_id, -EINVAL, NULL, 0);
				break;
			}
			const struct sof_net_shell_req *sreq =
				(const struct sof_net_shell_req *)req_buf;
			const char *cmd_str = (const char *)(req_buf + sizeof(*sreq));
			size_t out_cap = 32768;
			char *out_text = calloc(1, out_cap);

			if (!out_text) {
				rc = send_reply(sock, hdr.opcode, hdr.req_id, -ENOMEM, NULL, 0);
				break;
			}
			int n = execute_tb_shell_cmd(cmd_str, out_text, out_cap);

			if (n < 0)
				rc = send_reply(sock, hdr.opcode, hdr.req_id, n, NULL, 0);
			else
				rc = send_reply(sock, hdr.opcode, hdr.req_id, 0,
						out_text, (uint32_t)n);
			free(out_text);
			break;
		}

		case SOF_NET_OP_PROBE_CTL:
			rc = handle_probe_ctl(sock, hdr.req_id, req_buf, hdr.payload_len);
			break;

		case SOF_NET_OP_PCM_CTL:
			rc = handle_pcm_ctl(sock, hdr.req_id, req_buf, hdr.payload_len);
			break;

		case SOF_NET_OP_CARD_LIST:
			rc = handle_card_list(sock, hdr.req_id);
			break;

		case SOF_NET_OP_PCM_CAPS:
			rc = handle_pcm_caps(sock, hdr.req_id, req_buf, hdr.payload_len);
			break;

		case SOF_NET_OP_DAPM_GRAPH:
			rc = handle_dapm_graph(sock, hdr.req_id, req_buf, hdr.payload_len);
			break;

		case SOF_NET_OP_COMPR_CTL:
			rc = handle_compr_ctl(sock, hdr.req_id, req_buf, hdr.payload_len);
			break;

		case SOF_NET_OP_KCTL_LIST:
			rc = handle_kctl_list(sock, hdr.req_id, req_buf, hdr.payload_len);
			break;

		case SOF_NET_OP_KCTL_GET:
		case SOF_NET_OP_KCTL_SET:
			rc = handle_kctl_io(sock, hdr.opcode, hdr.req_id, req_buf, hdr.payload_len);
			break;

		default:
			rc = send_reply(sock, hdr.opcode, hdr.req_id, -ENOSYS, NULL, 0);
			break;
		}

		free(req_buf);
		if (rc < 0)
			break;
	}

	close(sock);
	return NULL;
}

static void *tb_accept_worker_fn(void *arg)
{
	(void)arg;
	while (g_srv_running && g_srv_fd >= 0) {
		struct sockaddr_in cli_addr;
		socklen_t cli_len = sizeof(cli_addr);
		int cli_fd = accept(g_srv_fd, (struct sockaddr *)&cli_addr, &cli_len);

		if (cli_fd < 0) {
			if (errno == EINTR && g_srv_running)
				continue;
			break;
		}

		struct tb_client_ctx *ctx = malloc(sizeof(*ctx));

		if (!ctx) {
			close(cli_fd);
			continue;
		}
		ctx->sock = cli_fd;

		pthread_t tid;
		pthread_attr_t attr;

		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		if (pthread_create(&tid, &attr, tb_client_worker_fn, ctx) != 0) {
			close(cli_fd);
			free(ctx);
		}
		pthread_attr_destroy(&attr);
	}
	return NULL;
}

int tb_remote_server_start(struct testbench_prm *tp)
{
	struct sockaddr_in bind_addr;
	int one = 1;
	int port;

	if (!tp || tp->server_port <= 0)
		return 0;

	port = tp->server_port;
	g_tp = tp;
	signal(SIGPIPE, SIG_IGN);

	g_srv_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (g_srv_fd < 0) {
		perror("tb_remote_server: socket");
		return -errno;
	}

	setsockopt(g_srv_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

	memset(&bind_addr, 0, sizeof(bind_addr));
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind_addr.sin_port = htons((uint16_t)port);

	if (bind(g_srv_fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
		perror("tb_remote_server: bind");
		close(g_srv_fd);
		g_srv_fd = -1;
		return -errno;
	}

	if (listen(g_srv_fd, 16) < 0) {
		perror("tb_remote_server: listen");
		close(g_srv_fd);
		g_srv_fd = -1;
		return -errno;
	}

	g_srv_running = true;
	g_quit_requested = false;
	if (pthread_create(&g_accept_thread, NULL, tb_accept_worker_fn, NULL) != 0) {
		g_srv_running = false;
		close(g_srv_fd);
		g_srv_fd = -1;
		return -EIO;
	}

	printf("sof-testbench4 remote server listening on 0.0.0.0:%d (SOFR protocol v%d)\n",
	       port, SOF_NET_VERSION);
	fflush(stdout);
	return 0;
}

void tb_remote_server_stop(void)
{
	if (!g_srv_running || g_in_remote_rpc)
		return;

	g_srv_running = false;
	g_dsp_paused = false;
	pthread_cond_broadcast(&g_tb_cond);

	if (g_srv_fd >= 0) {
		shutdown(g_srv_fd, SHUT_RDWR);
		close(g_srv_fd);
		g_srv_fd = -1;
	}

	pthread_join(g_accept_thread, NULL);
}

#endif /* !__XTENSA__ && !CONFIG_XT_SIMULATOR */
