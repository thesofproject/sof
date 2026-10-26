/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 *
 * Per-Core, Per-Pipeline, and Per-Module (LL + DP) Scheduling & Buffer Telemetry
 * for SOF Testbench (Host Pthread and Xtensa Simulator).
 */

#ifndef __PLATFORM_LIB_TELEMETRY_SCHED_H__
#define __PLATFORM_LIB_TELEMETRY_SCHED_H__

#include <sof/common.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef CONFIG_CORE_COUNT
#define CONFIG_CORE_COUNT 4
#endif

#define TB_TELEM_MAX_CORES	CONFIG_CORE_COUNT
#define TB_TELEM_MAX_PIPES	32
#define TB_TELEM_MAX_MODS	128
#define TB_TELEM_MAX_PINS	4
#define TB_TELEM_MAX_OVERRIDES	64
#define TB_MOD_TAP_SAMPLES	64
#define TB_TELEM_TRACE_CAPACITY	4096
#define TB_DSP_NOMINAL_HZ	400000000ULL	/* 400 MHz nominal DSP clock */

/* Per-iteration module scheduling timeline event type */
enum host_sched_event_type {
	HOST_SCHED_EVT_LL_COPY		= 0,	/* LL module comp_copy() execution */
	HOST_SCHED_EVT_DP_SYNC		= 1,	/* DP module LL-tick ring buffer sync */
	HOST_SCHED_EVT_DP_RUN		= 2,	/* DP worker thread dp_task_run() execution */
};

/* Per-iteration LL/DP module scheduling timeline trace event */
struct host_sched_trace_event {
	uint32_t seq_id;		/* 1-based monotonic sequence ID */
	uint32_t ll_tick_nr;		/* 1-based 1ms LL tick number */
	uint32_t mod_iter_nr;		/* 1-based iteration count for this module */
	uint32_t comp_id;		/* IPC4 component ID ((inst << 16) | mod_id) */
	uint16_t pipeline_id;
	uint8_t  core_id;		/* DSP core ID (0..7) */
	uint8_t  event_type;		/* enum host_sched_event_type */
	uint8_t  mem_domain;		/* HOST_MEM_DOMAIN_SRAM (0) or DRAM (1) */
	uint8_t  cpc_violation;		/* 1 if duration_cycles > cpc_budget */
	uint16_t period_us;		/* Module scheduling period in us */
	uint64_t start_cycles;		/* Virtual DSP cycle count at module start */
	uint64_t complete_cycles;	/* Virtual DSP cycle count at module completion */
	uint32_t duration_cycles;	/* Execution cycles (complete_cycles - start_cycles) */
	uint32_t cpc_budget;		/* Effective CPC budget */
	uint64_t start_ns;		/* Virtual DSP timeline start time (ns from t=0) */
	uint64_t complete_ns;		/* Virtual DSP timeline completion time (ns from t=0) */
	uint64_t deadline_ns;		/* Scheduling deadline timestamp (ns from t=0) */
	uint64_t wall_start_ns;		/* Host monotonic start timestamp (ns from run start) */
	uint64_t wall_complete_ns;	/* Host monotonic completion timestamp (ns from run start) */
	uint32_t bytes_in;		/* Total bytes consumed across input pins */
	uint32_t bytes_out;		/* Total bytes produced across output pins */
	int32_t  ret_status;		/* Return code from module processing */
};

struct pipeline;
struct comp_dev;

/* Workload execution time simulation mode */
enum host_workload_sim_mode {
	HOST_WORKLOAD_SIM_OFF		= 0,	/* Measure host CPU thread time */
	HOST_WORKLOAD_SIM_VIRTUAL	= 1,	/* Simulate cycles & exec time from CPC/freq/mem */
	HOST_WORKLOAD_SIM_REALTIME	= 2,	/* Simulate cycles & pace wall-clock execution */
};

/* Memory execution domain (SRAM vs DRAM residency) */
enum host_mem_exec_domain {
	HOST_MEM_DOMAIN_SRAM		= 0,	/* L2 HPSRAM/LPSRAM (1.00x base CPC) */
	HOST_MEM_DOMAIN_DRAM		= 1,	/* L3/System DRAM (cache/bus stall penalty) */
	HOST_MEM_DOMAIN_AUTO		= 0xFF,	/* Inherit global/pipeline default */
};

/* Per-module CPC and memory domain override entry */
struct host_workload_mod_override {
	bool active;
	uint32_t comp_id;
	uint32_t cpc_override;			/* 0 = use topology/default CPC */
	uint32_t mem_domain_override;		/* HOST_MEM_DOMAIN_SRAM, DRAM, or AUTO */
};

/* Global workload execution time simulation configuration */
struct host_workload_sim_cfg {
	uint32_t mode;				/* enum host_workload_sim_mode */
	uint32_t default_mem_domain;		/* HOST_MEM_DOMAIN_SRAM (0) or DRAM (1) */
	uint32_t sram_factor_x100;		/* Default 100 (1.00x) */
	uint32_t dram_factor_x100;		/* Default 160 (1.60x cache/bus stall factor) */
	uint32_t dram_cycles_per_word;		/* Default 2 cycles per 32-bit word in DRAM */
	uint32_t core_freq_hz[TB_TELEM_MAX_CORES]; /* Target clock frequency in Hz per core */
	uint32_t num_overrides;
	struct host_workload_mod_override overrides[TB_TELEM_MAX_OVERRIDES];
};

/* Per-core LL and DP scheduler telemetry */
struct host_core_telem {
	bool active;
	uint32_t freq_hz;			/* Active target core clock frequency in Hz */
	uint32_t ll_budget_cycles;		/* Available cycles per 1ms LL tick (freq_hz/1000) */
	uint32_t ll_deadline_violations;	/* Count of LL ticks exceeding ll_budget_cycles */

	uint32_t ll_tick_count;
	uint32_t ll_active_tasks;
	uint64_t ll_last_cycles;
	uint64_t ll_avg_cycles;
	uint64_t ll_peak_cycles;
	uint64_t ll_total_cycles;
	uint32_t ll_avg_kcps;
	uint32_t ll_peak_kcps;
	uint64_t ll_last_exec_ns;		/* Execution time in ns at target core freq */
	uint64_t ll_avg_exec_ns;
	uint64_t ll_peak_exec_ns;
	uint64_t ll_total_exec_ns;

	uint32_t dp_run_count;
	uint32_t dp_active_threads;
	uint64_t dp_last_cycles;
	uint64_t dp_avg_cycles;
	uint64_t dp_peak_cycles;
	uint64_t dp_total_cycles;
	uint32_t dp_avg_kcps;
	uint32_t dp_peak_kcps;
	uint64_t dp_last_exec_ns;		/* Execution time in ns at target core freq */
	uint64_t dp_avg_exec_ns;
	uint64_t dp_peak_exec_ns;
	uint64_t dp_total_exec_ns;

	uint32_t total_util_x100;	/* Combined LL+DP utilization vs freq_hz (0.01% units) */

	/* Internal scratch accumulators for workload simulation & timeline cursors */
	uint64_t _sim_ll_tick_cycles;
	uint64_t _sim_dp_run_cycles;
	uint64_t _tick_ll_offset_cycles;
	uint64_t _dp_cursor_cycles;
};

/* Per-pipeline scheduling telemetry */
struct host_pipe_telem {
	bool active;
	uint32_t pipeline_id;
	uint32_t comp_id;
	uint32_t core_id;
	uint32_t priority;
	uint32_t period_us;
	uint32_t direction;
	uint32_t status;
	uint32_t mem_domain;		/* HOST_MEM_DOMAIN_SRAM (0) or DRAM (1) */
	uint64_t copy_iterations;
	uint64_t last_cycles;
	uint64_t avg_cycles;
	uint64_t peak_cycles;
	uint64_t total_cycles;
	uint32_t avg_kcps;
	uint32_t peak_kcps;
	uint64_t last_exec_ns;		/* Pipeline execution time in ns at core freq */
	uint64_t avg_exec_ns;
	uint64_t peak_exec_ns;
	uint64_t total_exec_ns;
	uint32_t xrun_count;
	uint32_t num_ll_modules;
	uint32_t num_dp_modules;

	/* Internal scratch accumulator for workload simulation */
	uint64_t _sim_tick_cycles;
};

/* Per-pin buffer telemetry captured on module entry and exit */
struct host_mod_pin_telem {
	bool active;
	uint32_t buf_id;
	uint32_t peer_comp_id;		/* Upstream producer or downstream consumer ID */
	uint32_t buf_size;		/* Total primary buffer capacity in bytes */
	uint32_t rate;
	uint16_t channels;
	uint16_t frame_fmt;

	/* Primary LL comp_buffer levels (bytes) */
	uint32_t entry_avail_bytes;
	uint32_t entry_free_bytes;
	uint32_t exit_avail_bytes;
	uint32_t exit_free_bytes;

	/* Secondary DP ring_buffer levels (bytes, non-zero for DP connections) */
	uint32_t dp_ring_size;
	uint32_t dp_ring_entry_avail;
	uint32_t dp_ring_exit_avail;
	uint32_t dp_ring_entry_free;
	uint32_t dp_ring_exit_free;

	/* Bytes consumed (input pin) or produced (output pin) */
	uint32_t last_bytes_transferred;
	uint64_t total_bytes_transferred;
	uint32_t xrun_count;

	/* Signal level telemetry on entry and exit (0.01 dBFS, e.g. -301 = -3.01 dBFS) */
	int16_t entry_rms_dbfs_x100;
	int16_t entry_peak_dbfs_x100;
	int16_t exit_rms_dbfs_x100;
	int16_t exit_peak_dbfs_x100;

	/* Rolling S16-normalized sample snapshots at module entry and exit */
	uint16_t entry_num_samples;
	uint16_t exit_num_samples;
	int16_t entry_samples[TB_MOD_TAP_SAMPLES];
	int16_t exit_samples[TB_MOD_TAP_SAMPLES];

	/* Internal scratch pointers saved at entry for exit delta calculation */
	uintptr_t _entry_ptr;
	uint32_t _entry_avail;
	uint32_t _dp_entry_woff;
};

/* Per-module (LL or DP) scheduling and buffer telemetry */
struct host_mod_telem {
	bool active;
	uint32_t comp_id;
	uint32_t pipeline_id;
	uint32_t core_id;
	uint32_t proc_domain;		/* COMP_PROCESSING_DOMAIN_LL (0) or DP (1) */
	uint32_t type;			/* enum sof_comp_type */
	uint32_t period_us;
	uint32_t ibs;
	uint32_t obs;
	uint32_t cpc;			/* Cycles Per Chunk budget from topology */
	uint32_t effective_cpc;		/* Effective base CPC (override -> topology -> default) */
	uint32_t mem_domain;		/* Effective memory domain: SRAM (0) or DRAM (1) */

	/* Processing execution telemetry (comp_copy for LL, dp_task_run for DP) */
	uint64_t iterations;
	uint64_t last_cycles;
	uint64_t avg_cycles;
	uint64_t peak_cycles;
	uint64_t total_cycles;
	uint32_t avg_kcps;
	uint32_t peak_kcps;
	uint64_t last_exec_ns;		/* Execution time in ns at core's target frequency */
	uint64_t avg_exec_ns;
	uint64_t peak_exec_ns;
	uint64_t total_exec_ns;
	uint64_t last_start_cycles;	/* Virtual DSP start cycle of most recent iteration */
	uint64_t last_complete_cycles;	/* Virtual DSP completion cycle of most recent iteration */
	uint64_t last_start_ns;		/* Virtual DSP start time (ns) of most recent iteration */
	uint64_t last_complete_ns;	/* Virtual DSP completion time (ns) of most recent iter */
	uint64_t last_deadline_ns;	/* Scheduling deadline (ns) of most recent iteration */
	uint64_t last_wall_start_ns;	/* Host monotonic start time (ns) of most recent iter */
	uint64_t last_wall_complete_ns;	/* Host monotonic completion time (ns) of most recent iter */
	uint32_t cpc_violations;
	uint32_t error_count;

	/* Internal entry wall-clock scratch timestamps */
	uint64_t _ll_entry_wall_ns;
	uint64_t _dp_entry_wall_ns;

	/* For DP modules: LL tick ring-buffer synchronization stats */
	uint64_t ll_ring_sync_iterations;
	uint64_t ll_ring_sync_last_cycles;
	uint64_t ll_ring_sync_total_cycles;

	/* Input (producer) and Output (consumer) pin buffer telemetry */
	uint32_t num_in_pins;
	uint32_t num_out_pins;
	struct host_mod_pin_telem in_pins[TB_TELEM_MAX_PINS];
	struct host_mod_pin_telem out_pins[TB_TELEM_MAX_PINS];
};

/* Hardware platform preset descriptor */
struct host_platform_preset {
	const char *name;
	const char *aliases;
	const char *desc;
	uint32_t hw_ip_version;		/* 2=cAVS2.5, 4=ACE1.5, 5=ACE2.0, 6=ACE3.0, 7=ACE4.0 */
	uint32_t num_cores;
	uint32_t freq_hz;
	size_t hpsram_size_bytes;
	size_t lpsram_size_bytes;
	size_t dram_size_bytes;
};

/* Runtime hardware platform configuration & live memory allocation state */
struct host_platform_hw_cfg {
	char platform_name[32];
	char arch_desc[48];
	uint32_t hw_ip_version;
	uint32_t num_cores;		/* Active platform core count (1..TB_TELEM_MAX_CORES) */
	uint32_t enabled_cores_mask;	/* Bitmask of enabled cores */
	uint32_t default_freq_hz;	/* Nominal DSP core clock frequency in Hz */
	size_t hpsram_size_bytes;	/* Total L2 HPSRAM capacity in bytes */
	size_t lpsram_size_bytes;	/* Total L2 LPSRAM capacity in bytes */
	size_t dram_size_bytes;		/* Total L3 IMR/DRAM capacity in bytes */
	uint32_t sram_bank_size;	/* SRAM EBB bank size in bytes (65536) */
	bool enforce_mem_limit;		/* Reject allocations exceeding configured capacity */

	/* Live heap allocation tracking across rmalloc/rzalloc/rballoc/sof_heap_alloc */
	size_t hpsram_used_bytes;
	size_t hpsram_peak_bytes;
	size_t lpsram_used_bytes;
	size_t lpsram_peak_bytes;
	size_t dram_used_bytes;
	size_t dram_peak_bytes;
	uint64_t total_alloc_calls;
	uint64_t total_free_calls;
	uint32_t alloc_failures;
};

/* Global testbench scheduling and buffer telemetry state */
struct host_sched_telemetry {
	bool enabled;
	uint32_t num_cores;
	uint32_t num_pipes;
	uint32_t num_mods;
	uint32_t current_ll_tick;
	uint64_t run_start_wall_ns;
	uint32_t trace_head;
	uint32_t trace_count;
	uint32_t trace_total_events;
	struct host_platform_hw_cfg hw_cfg;
	struct host_workload_sim_cfg sim_cfg;
	struct host_core_telem cores[TB_TELEM_MAX_CORES];
	struct host_pipe_telem pipes[TB_TELEM_MAX_PIPES];
	struct host_mod_telem mods[TB_TELEM_MAX_MODS];
	struct host_sched_trace_event trace_events[TB_TELEM_TRACE_CAPACITY];
};

/* Reset all telemetry counters (preserves workload simulation & platform configuration) */
void host_telem_reset(void);

/* Enable or disable telemetry collection (enabled by default) */
void host_telem_set_enabled(bool enabled);

/* Retrieve pointer to global telemetry structure */
struct host_sched_telemetry *host_telem_get(void);

/* Hardware platform configuration & preset APIs */
const struct host_platform_preset *host_platform_get_presets(size_t *out_count);
const struct host_platform_preset *host_platform_find_preset(const char *name);
int host_platform_apply_preset(const char *name);
int host_platform_set_core_count(uint32_t num_cores);
uint32_t host_platform_get_core_count(void);
void host_platform_set_memory(size_t hpsram_bytes, size_t lpsram_bytes,
			      size_t dram_bytes, bool enforce_limit);
size_t host_platform_get_hpsram_size(void);
size_t host_platform_get_lpsram_size(void);
size_t host_platform_get_dram_size(void);
void host_platform_reset_mem_stats(void);
bool host_platform_account_alloc(const void *ptr, uint32_t flags, size_t bytes);
void host_platform_account_free(const void *ptr, size_t bytes);

/* Host arch_cpu_* runtime backing functions */
int host_cpu_num_cores(void);
int host_cpu_enable_core(int id);
void host_cpu_disable_core(int id);
int host_cpu_is_core_enabled(int id);
int host_cpu_enabled_cores(void);

/* Workload execution time simulation configuration APIs */
void host_workload_sim_set_mode(uint32_t mode);
uint32_t host_workload_sim_get_mode(void);
void host_workload_sim_set_core_freq(int core, uint32_t freq_hz);
uint32_t host_workload_sim_get_core_freq(int core);
void host_workload_sim_set_mem_domain(uint32_t mem_domain, uint32_t dram_factor_x100,
				      uint32_t dram_cycles_per_word);
int host_workload_sim_set_mod_override(uint32_t comp_id, uint32_t cpc_override,
				       uint32_t mem_domain_override);
void host_workload_sim_clear_overrides(void);

/* Read current thread/CPU execution timestamp in nominal DSP cycles */
uint64_t host_telem_timestamp_cycles(void);

/* Core LL and DP scheduler hooks */
void host_telem_ll_tick_begin(void);
void host_telem_core_ll_tick(int core, uint32_t tasks_run, uint64_t cycles);
void host_telem_core_dp_run(int core, uint32_t active_dp_threads, uint64_t cycles);

/* Per-iteration scheduling timeline trace ring buffer APIs */
void host_telem_trace_clear(void);
uint32_t host_telem_trace_read(struct host_sched_trace_event *out_events,
			       uint32_t max_events, uint32_t since_seq_id,
			       int core_filter, int pipe_filter,
			       int comp_filter, int domain_filter);

#ifdef UNIT_TEST
static inline uint64_t host_telem_pipe_begin(struct pipeline *p)
{
	(void)p;
	return 0;
}

static inline void host_telem_pipe_end(struct pipeline *p, uint64_t start_cycles, int ret)
{
	(void)p;
	(void)start_cycles;
	(void)ret;
}

static inline uint64_t host_telem_mod_entry(struct comp_dev *dev, bool is_dp_thread)
{
	(void)dev;
	(void)is_dp_thread;
	return 0;
}

static inline void host_telem_mod_exit(struct comp_dev *dev, bool is_dp_thread,
				       uint64_t start_cycles, int ret)
{
	(void)dev;
	(void)is_dp_thread;
	(void)start_cycles;
	(void)ret;
}
#else
/* Pipeline copy entry/exit hooks */
uint64_t host_telem_pipe_begin(struct pipeline *p);
void host_telem_pipe_end(struct pipeline *p, uint64_t start_cycles, int ret);

/* Module processing entry/exit hooks (is_dp_thread = false in comp_copy, true in dp_task_run) */
uint64_t host_telem_mod_entry(struct comp_dev *dev, bool is_dp_thread);
void host_telem_mod_exit(struct comp_dev *dev, bool is_dp_thread, uint64_t start_cycles, int ret);
#endif

#endif /* __PLATFORM_LIB_TELEMETRY_SCHED_H__ */
