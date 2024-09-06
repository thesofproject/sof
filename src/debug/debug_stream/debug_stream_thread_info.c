// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <rtos/alloc.h>
#include <ipc/topology.h>
#include <stdio.h>
#include <soc.h>
#include <adsp_debug_window.h>

#include <user/debug_stream_thread_info.h>

LOG_MODULE_REGISTER(thread_info);

#define THREAD_INFO_MAX_THREADS 64
/* Must be bigger than sizeof(struct thread_info_record_hdr) */
#define THREAD_INFO_INITIAL_RECORD_BUFFER_SIZE 256

#ifdef CONFIG_THREAD_RUNTIME_STATS
/* Data structure to store the cycle counter values from the previous
 * round. The numbers are used to calculate what the load was on this
 * round.
 */
static struct previous_counters { /* Cached data from previous round */
	uint64_t active;	  /* All execution cycles */
	uint64_t all;		  /* All cycles including idle */
	struct thread_counters {
		void *tid;	 /* thread ID (the thread struct ptr) */
		uint64_t cycles; /* cycle counter value */
	} threads[THREAD_INFO_MAX_THREADS]; /* The max amount of threads we follow */
} __aligned(CONFIG_DCACHE_LINE_SIZE) previous[CONFIG_MP_MAX_NUM_CPUS];
#endif

/*
 * Buffer structure for building the Debug Stream record before
 * sending it forward. The buffer is persistent and each record is
 * build in the same buffer. Its reallocated as double size if it
 * becomes too small.
 */
struct record_buf {
	size_t size;
	size_t w_ptr;
	uint8_t *buf;
};

/*
 * Data structure to be passed down to thread_info_cb() by
 * k_thread_foreach_current_cpu().
 */
struct user_data {
	int core;
	struct record_buf *bufd;
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
 * returns it. The tid is actually a k_tread pointer, but since its
 * used just as an id and never accessed, I pass it as a void pointer.
 */

static uint32_t thread_info_get_cycles(void *tid, k_thread_runtime_stats_t *thread_stats,
				       struct user_data *ud, const char *name)

{
	int i;

	if (ud->thread_count >= ARRAY_SIZE(ud->active_threads)) {
		LOG_WRN("Thread count exceeds the max threads %u >= %u",
			ud->thread_count, ARRAY_SIZE(ud->active_threads));
		return 0;
	}

	/* Mark the thread as active */
	ud->active_threads[ud->thread_count] = tid;
	/* look for cached value from previous round for 'tid'-thread */
	for (i = 0; i < ARRAY_SIZE(ud->previous->threads); i++) {
		if (ud->previous->threads[i].tid == tid) {
			/* Calculate number cycles since previous round */
			uint32_t cycles = (uint32_t) (thread_stats->execution_cycles -
						      ud->previous->threads[i].cycles);

			LOG_DBG("%p found at %d (%s %llu)", tid, i,
				name, thread_stats->execution_cycles);
			/* update cached value */
			ud->previous->threads[i].cycles = thread_stats->execution_cycles;
			return cycles;
		}
	}

	/* If no cached value was found, look for an empty slot to store the recent value */
	for (i = 0; i < ARRAY_SIZE(ud->previous->threads); i++) {
		if (ud->previous->threads[i].tid == NULL) {
			ud->previous->threads[i].tid = tid;
			ud->previous->threads[i].cycles = thread_stats->execution_cycles;
			LOG_DBG("%p placed at %d (%s %llu)", tid, i,
				name, ud->previous->threads[i].cycles);
			break;
		}
	}

	/* If there is more than THREAD_INFO_MAX_THREADS threads in this core */
	if (i == ARRAY_SIZE(ud->previous->threads))
		LOG_WRN("No place found for %s %p", name, tid);

	/* If there was no previous counter value to compare, return 0 cycles. */
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

	cycles = thread_info_get_cycles(thread, &thread_stats, ud, name);

	LOG_DBG("thread %s %u / %u", name, cycles, ud->all_cycles);

	return (uint8_t) ((255llu * cycles) / ud->all_cycles);
}
#else
static uint8_t thread_info_cpu_utilization(struct k_thread *thread,
					   struct user_data *ud, const char *name)
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

static int thread_info_buf_realloc(struct record_buf *bufd, size_t req_size)
{
	size_t size = bufd->size;

	while (size < bufd->w_ptr + req_size)
		size *= 2;

	if (size != bufd->size) {
		uint8_t *buf = rmalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, size);

		if (!buf)
			return -ENOMEM;

		memcpy_s(buf, size, bufd->buf, bufd->w_ptr);
		rfree(bufd->buf);
		bufd->size = size;
		bufd->buf = buf;
	}

	return 0;
}

static void thread_info_cb(const struct k_thread *cthread, void *user_data)
{
	struct k_thread *thread = (struct k_thread *)cthread;
	struct user_data *ud = user_data;
	struct thread_info *tinfo;
	const char *name;

	name = k_thread_name_get((k_tid_t)thread);
	if (!name || name[0] == '\0') {
		size_t ptr_str_len = 11; /* length of "0x12345678\0" */

		if (thread_info_buf_realloc(ud->bufd, sizeof(*tinfo) + ptr_str_len))
			return;

		tinfo = (struct thread_info *) &ud->bufd->buf[ud->bufd->w_ptr];
		snprintk(tinfo->name, ptr_str_len, "%p", (void *)thread);
		/* Drop the terminating '\0', that is why -1 is here. */
		tinfo->name_len = ptr_str_len - 1;
		ud->bufd->w_ptr += sizeof(*tinfo) + ptr_str_len - 1;
	} else {
		size_t len = strlen(name);

		if (thread_info_buf_realloc(ud->bufd, sizeof(*tinfo) + len + 1))
			return;

		tinfo = (struct thread_info *) &ud->bufd->buf[ud->bufd->w_ptr];
		strncpy(tinfo->name, name, len + 1);
		tinfo->name_len = len;
		ud->bufd->w_ptr += sizeof(*tinfo) + len;
	}

	tinfo->stack_usage = thread_info_stack_level(thread, name);
	tinfo->cpu_usage = thread_info_cpu_utilization(thread, ud, name);

	LOG_DBG("core %u %s stack %u%% cpu %u%%", ud->core,
		tinfo->name, tinfo->stack_usage * 100U / 255,
		tinfo->cpu_usage * 100U / 255);

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

		/* This entry is already free, continue */
		if (ud->previous->threads[i].tid == NULL)
			continue;

		/* Check if the thread was seen on previous round */
		for (j = 0; j < ud->thread_count; j++) {
			if (ud->active_threads[j] == ud->previous->threads[i].tid) {
				found = true;
				break;
			}
		}
		/* If the thead is not any more active, mark the entry free */
		if (!found) {
			ud->previous->threads[i].tid = NULL;
			ud->previous->threads[i].cycles = 0;
		}
	}
}
#else
static void cleanup_old_thread_cycles(struct user_data *ud) { }
#endif

static void thread_info_get(int core, struct record_buf *bufd)
{
	k_thread_runtime_stats_t core_stats;
	struct user_data ud = {
		.core = core,
		.bufd = bufd,
		.thread_count = 0,
#ifdef CONFIG_THREAD_RUNTIME_STATS
		.active_threads = { NULL },
		.previous = &previous[core],
#endif
	};
	struct thread_info_record_hdr *hdr;
	uint8_t load = 0;
#ifdef CONFIG_THREAD_RUNTIME_STATS
	int ret = k_thread_runtime_stats_cpu_get(core, &core_stats);

	if (ret == 0) {
		uint32_t active_cycles = (uint32_t) (core_stats.total_cycles -
						     ud.previous->active);
		uint32_t all_cycles = (uint32_t) (core_stats.execution_cycles -
						  ud.previous->all);

		ud.stats_valid = true;
		load = (uint8_t) ((255LLU * active_cycles) / all_cycles);
		LOG_DBG("Core %u load %u / %u total %llu / %llu", core,
			active_cycles, all_cycles,
			core_stats.total_cycles, core_stats.execution_cycles);
		ud.previous->active = core_stats.total_cycles;
		ud.previous->all = core_stats.execution_cycles;
		ud.all_cycles = all_cycles;
	}
#endif

	hdr = (struct thread_info_record_hdr *) bufd->buf;
	bufd->w_ptr = sizeof(*hdr);
	hdr->hdr.id = DEBUG_STREAM_RECORD_ID_THREAD_INFO;
	hdr->load = load;
	/* This is best effort debug tool. Unlocked version should be fine. */
	k_thread_foreach_unlocked_filter_by_cpu(core, thread_info_cb, &ud);

	cleanup_old_thread_cycles(&ud);

	hdr->thread_count = ud.thread_count;
	hdr->hdr.size_words = SOF_DIV_ROUND_UP(bufd->w_ptr, sizeof(hdr->hdr.data[0]));
	debug_stream_slot_send_record(&hdr->hdr);
}

static void thread_info_run(void *cnum, void *a, void *b)
{
	int core = (int) cnum;
	struct record_buf bufd = {
		.size = THREAD_INFO_INITIAL_RECORD_BUFFER_SIZE,
		.w_ptr = 0,
	};

	bufd.buf = rmalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, bufd.size);
	if (!bufd.buf) {
		LOG_ERR("malloc failed");
		return;
	}

	for (;;) {
		thread_info_get(core, &bufd);
		k_sleep(K_SECONDS(CONFIG_SOF_DEBUG_STREAM_THREAD_INFO_INTERVAL));
	}
}

#define THREAD_STACK_SIZE (2048)
static K_THREAD_STACK_ARRAY_DEFINE(info_thread_stacks, CONFIG_MP_MAX_NUM_CPUS,
				   THREAD_STACK_SIZE);
static struct k_thread info_thread[CONFIG_MP_MAX_NUM_CPUS];

static int thread_info_start(void)
{
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(info_thread); i++) {
		char name[24];
		k_tid_t tid;
		int ret;

		tid = k_thread_create(&info_thread[i], info_thread_stacks[i],
				      THREAD_STACK_SIZE, thread_info_run,
				      (void *) i, NULL, NULL,
				      K_LOWEST_APPLICATION_THREAD_PRIO, 0,
				      K_FOREVER);
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
