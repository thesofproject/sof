// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <stdio.h>
#include <soc.h>
#include <adsp_debug_window.h>

#include "telemetry2_slot.h"

LOG_MODULE_REGISTER(thread_info);

#define THREAD_INFO_MAX_THREADS 16

struct thread_info {
	char name[14];
	uint8_t stack_usage;	/* Relative stack usage U(0,8) fixed point value */
	uint8_t cpu_usage;	/* Relative cpu usage U(0,8) fixed point value */
} __packed;

#define THREAD_INFO_VERSION_0_0		0

#define THREAD_INFO_STATE_UNINITIALIZED	0
#define THREAD_INFO_STATE_BEING_UPDATED	1
#define THREAD_INFO_STATE_UPTODATE	2

/* Core specific data, updated each round, including the thread table. */
struct thread_info_core {
	uint8_t state;		/* indicates if core data is in consistent state */
	uint8_t counter;	/* incremented every round */
	uint8_t load;		/* Core's load U(0,8) fixed point value */
	uint8_t thread_count;
	struct thread_info thread[THREAD_INFO_MAX_THREADS];
} __packed __aligned(CONFIG_DCACHE_LINE_SIZE);

/* Thread info telemetry2 chunk header. Only initialized at first
 * thread info thread start. Should be used to find the core specific
 * sections where the thread info is when decoding.
 */
struct thread_info_chunk {
	struct telemetry2_chunk_hdr hdr;
	uint16_t core_count;
	uint16_t core_offset[CONFIG_MP_MAX_NUM_CPUS];
	struct thread_info_core __aligned(CONFIG_DCACHE_LINE_SIZE) core[CONFIG_MP_MAX_NUM_CPUS];
} __packed;

#ifdef CONFIG_THREAD_RUNTIME_STATS
/* Data structure to store the cycle counter values from the previous
 * round. The numbers are used to calculate what the load was on this
 * round.
 */
static struct previous_counters { // Cached data from previous round
	uint64_t active;	  // All execution cycles
	uint64_t all;		  // All cycles including idle
	struct thread_counters {
		void *tid;	 // thread ID (the thread struct ptr)
		uint64_t cycles; // cycle counter value
	} threads[THREAD_INFO_MAX_THREADS]; // The max amount of threads we follow
} previous[CONFIG_MP_MAX_NUM_CPUS];
#endif

/* Data structure to be passed down to thread_info_cb() by
 * k_thread_foreach_current_cpu().
 */
struct user_data {
	struct thread_info_core *core_data;
	int thread_count;
#ifdef CONFIG_THREAD_RUNTIME_STATS
	bool stats_valid;
	uint32_t all_cycles;
	void *active_threads[THREAD_INFO_MAX_THREADS];
	struct previous_counters *previous;
#endif
};

#ifdef CONFIG_THREAD_RUNTIME_STATS

/* Finds and/or updates cached cycle counter value for 'tid'-thread
 * and calculates used execution cycles since previous round and
 * returns it.
 */

static uint32_t get_cycles(void *tid, k_thread_runtime_stats_t *thread_stats,
			   struct user_data *ud, const char *name)

{
	int i;

	ud->active_threads[ud->thread_count] = tid; // Mark the thread as active
	// look for cached value from previous round for 'tid'-thread
	for (i = 0; i < ARRAY_SIZE(ud->previous->threads); i++) {
		if (ud->previous->threads[i].tid == tid) {
			// Calculate number cycles since previous round
			uint32_t cycles = (uint32_t) (thread_stats->execution_cycles -
						      ud->previous->threads[i].cycles);

			LOG_DBG("%p found at %d (%s %llu)", tid, i,
				name, thread_stats->execution_cycles);
			// updare cached value
			ud->previous->threads[i].cycles = thread_stats->execution_cycles;
			return cycles;
		}
	}

	// If no cached value was found, look for an empty slot to store the recent value
	for (i = 0; i < ARRAY_SIZE(ud->previous->threads); i++) {
		if (ud->previous->threads[i].tid == NULL) {
			ud->previous->threads[i].tid = tid;
			ud->previous->threads[i].cycles = thread_stats->execution_cycles;
			LOG_DBG("%p placed at %d (%s %llu)", tid, i,
				name, ud->previous->threads[i].cycles);
			break;
		}
	}

	// If there is more than THREAD_INFO_MAX_THREADS threads in the system
	if (i == ARRAY_SIZE(ud->previous->threads))
		LOG_INF("No place found for %s %p", name, tid);

	// If there was no previous counter value to compare, return 0 cycles.
	return 0;
}

static uint8_t thread_info_cpu_utilization(struct k_thread *thread,
					   struct user_data *ud, const char *name)
{
	k_thread_runtime_stats_t thread_stats;
	uint32_t cycles;
	int ret;

	if (!ud->stats_valid)
		return 0;

	if (k_thread_runtime_stats_get(thread, &thread_stats) != 0)
		return 0;

	cycles = get_cycles(thread, &thread_stats, ud, name);

	LOG_DBG("thread %s %u / %u", name, cycles, ud->all_cycles);

	return (uint8_t) ((255llu * cycles) / ud->all_cycles);
}
#else
static uint8_t thread_info_cpu_utilization(struct k_thread *thread,
					   struct user_data *ud)
{
	return 0;
}
#endif

#ifdef CONFIG_THREAD_STACK_INFO
static uint8_t thread_info_stack_level(struct k_thread *thread, const char *name)
{
	size_t stack_size, stack_unused;
	int ret;

	stack_size = thread->stack_info.size;
	ret = k_thread_stack_space_get(thread, &stack_unused);
	if (ret) {
		LOG_ERR(" %-20s: unable to get stack space (%d)",
			name, ret);
		stack_unused = 0;
	}
	return (UINT8_MAX * (stack_size - stack_unused)) / stack_size;
}
#else
static uint8_t thread_info_stack_level(struct k_thread *thread, const char *name)
{
	return 0;
}
#endif

static void thread_info_cb(const struct k_thread *cthread, void *user_data)
{
	struct k_thread *thread = (struct k_thread *)cthread;
	struct user_data *ud = user_data;
	struct thread_info *thread_info;
	const char *name;

	if (ud->thread_count > ARRAY_SIZE(ud->core_data->thread)) {
		LOG_ERR("Thread count %u exceeds the memory window size\n",
			ud->thread_count++);
		return;
	}

	thread_info = &ud->core_data->thread[ud->thread_count];

	name = k_thread_name_get((k_tid_t)thread);
	if (!name || name[0] == '\0') {
		snprintk(thread_info->name, sizeof(thread_info->name), "%p",
			 (void *)thread);
	} else {
		strncpy(thread_info->name, name, sizeof(thread_info->name));
	}

	thread_info->stack_usage = thread_info_stack_level(thread, name);
	thread_info->cpu_usage = thread_info_cpu_utilization(thread, ud, name);

	LOG_DBG("core %u %s stack %u%% cpu %u%%", ud->core,
		thread_info->name, thread_info->stack_usage * 100U / 255,
		thread_info->cpu_usage * 100U / 255);

	ud->thread_count++;
}

#ifdef CONFIG_THREAD_RUNTIME_STATS

/* Marks cached thread cycle counter entries of removed threads
 * free. This also happens to threads pinned to other cpu than the
 * primary. In the beginning they are listed on primary cpu, until the
 * pinned cpu is started up and the thread is executed for the fist
 * time and it moves to the cpu its pinned on.
 */
static void cleanup_old_thread_cycles(struct user_data *ud)
{
	int i, j;

	for (i = 0; i < ARRAY_SIZE(ud->previous->threads); i++) {
		bool found = false;

		// This entry is already free, continue
		if (ud->previous->threads[i].tid == NULL)
			continue;

		// Check if the thread is any more active
		for (j = 0; j < ud->thread_count; j++) {
			if (ud->active_threads[j] == ud->previous->threads[i].tid) {
				found = true;
				break;
			}
		}
		// If the thead is not any more active, mark the entry free
		if (!found) {
			ud->previous->threads[i].tid = NULL;
			ud->previous->threads[i].cycles = 0;
		}
	}
}
#else
static void cleanup_old_thread_cycles(struct user_data *ud) { }
#endif

static void thread_info_get(struct thread_info_core *core_data)
{
	k_thread_runtime_stats_t core_stats;
	struct user_data ud = {
		.core_data = core_data,
		.thread_count = 0,
#ifdef CONFIG_THREAD_RUNTIME_STATS
		.previous = &previous[arch_curr_cpu()->id],
		.active_threads = { NULL },
#endif
	};
	uint8_t load = 0;
#ifdef CONFIG_THREAD_RUNTIME_STATS
	int ret = k_thread_runtime_stats_current_cpu_get(&core_stats);

	if (ret == 0) {
		uint32_t active_cycles = (uint32_t) (core_stats.total_cycles -
						     ud.previous->active);
		uint32_t all_cycles = (uint32_t) (core_stats.execution_cycles -
						  ud.previous->all);

		ud.stats_valid = true;
		load = (uint8_t) ((255LLU * active_cycles) / all_cycles);
		LOG_DBG("Core %u load %u / %u total %llu / %llu", arch_curr_cpu()->id,
			active_cycles, all_cycles,
			core_stats.total_cycles, core_stats.execution_cycles);
		ud.previous->active = core_stats.total_cycles;
		ud.previous->all = core_stats.execution_cycles;
		ud.all_cycles = all_cycles;
	}
#endif
	core_data->state = THREAD_INFO_STATE_BEING_UPDATED;

	core_data->load = load;
	k_thread_foreach_current_cpu(thread_info_cb, &ud);

	cleanup_old_thread_cycles(&ud);

	core_data->counter++;
	core_data->thread_count = ud.thread_count;
	core_data->state = THREAD_INFO_STATE_UPTODATE;
}

static void thread_info_run(void *data, void *cnum, void *a)
{
	struct thread_info_chunk *chunk = data;
	int core = (int) cnum;
	struct thread_info_core *core_data = &chunk->core[core];

	for (;;) {
		thread_info_get(core_data);
		k_sleep(K_SECONDS(CONFIG_SOF_TELEMETRY2_THREAD_INFO_INTERVAL));
	}
}

static struct thread_info_chunk *thread_info_init(void)
{
	struct thread_info_chunk *chunk;
	int i;

	chunk = telemetry2_chunk_get(TELEMETRY2_ID_THREAD_INFO,
				     sizeof(*chunk));
	if (!chunk)
		return NULL;

	chunk->core_count = CONFIG_MP_MAX_NUM_CPUS;
	for (i = 0; i < CONFIG_MP_MAX_NUM_CPUS; i++)
		chunk->core_offset[i] = offsetof(struct thread_info_chunk, core[i]);

	return chunk;
}

static K_THREAD_STACK_ARRAY_DEFINE(info_thread_stacks, CONFIG_MP_MAX_NUM_CPUS,
				   1024);
static struct k_thread info_thread[CONFIG_MP_MAX_NUM_CPUS];

static int thread_info_start(void)
{
	struct thread_info_chunk *chunk = thread_info_init();
	uint32_t i;

	if (!chunk)
		return 0;

	for (i = 0; i < ARRAY_SIZE(info_thread); i++) {
		char name[24];
		k_tid_t tid = NULL;
		int ret;

		tid = k_thread_create(&info_thread[i], info_thread_stacks[i],
				      1024,
				      thread_info_run,
				      chunk, (void *) i, NULL,
				      K_LOWEST_APPLICATION_THREAD_PRIO, 0, K_FOREVER);
		if (!tid) {
			LOG_ERR("k_thread_create() failed for core %u", i);
			continue;
		}
		ret = k_thread_cpu_pin(tid, i);
		if (ret < 0) {
			LOG_ERR("Pinning thread to code core %u", i);
			k_thread_abort(tid);
			continue;
		}
		snprintf(name, sizeof(name), "%u thread info", i);
		ret = k_thread_name_set(tid, name);
		if (ret < 0)
			LOG_INF("k_thread_name_set failed: %d for %u", ret, i);

		k_thread_start(tid);
		LOG_DBG("Thread %p for core %u started", tid, i);
	}

	return 0;
}

SYS_INIT(thread_info_start, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
