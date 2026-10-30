/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * @file sof/schedule/ll_schedule_domain.h
 * @brief Low-Latency (LL) scheduling domain architecture, timer/DMA drivers, and core domain operations.
 * @ingroup scheduling_api
 */

#ifndef __SOF_SCHEDULE_LL_SCHEDULE_DOMAIN_H__
#define __SOF_SCHEDULE_LL_SCHEDULE_DOMAIN_H__

#include <rtos/atomic.h>
#include <rtos/panic.h>
#include <rtos/alloc.h>
#include <sof/lib/cpu.h>
#include <rtos/clk.h>
#include <sof/lib/memory.h>
#include <sof/schedule/ll_schedule.h>
#include <rtos/sof.h>
#include <rtos/spinlock.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <stdbool.h>
#include <stdint.h>

/** \addtogroup scheduling_api
 *  @{
 */

struct dma;
struct ll_schedule_domain;
struct task;
struct timer;
struct comp_dev;

/**
 * @brief Low-Latency schedule domain operations table.
 *
 * Implements driver-level primitives for timer and DMA scheduling domains.
 */
struct ll_schedule_domain_ops {
	/**
	 * @brief Register a real-time task with the domain.
	 * @param[in,out] domain Target scheduling domain.
	 * @param[in,out] task Task being registered.
	 * @param[in] handler Domain tick handler callback.
	 * @param[in,out] arg Context argument passed to tick handler.
	 * @return 0 on success, negative error code on failure.
	 */
	int (*domain_register)(struct ll_schedule_domain *domain,
			       struct task *task,
			       void (*handler)(void *arg), void *arg);

	/**
	 * @brief Unregister a task from the domain.
	 * @param[in,out] domain Target scheduling domain.
	 * @param[in,out] task Task being unregistered.
	 * @param[in] num_tasks Current total task count.
	 * @return 0 on success, negative error code on failure.
	 */
	int (*domain_unregister)(struct ll_schedule_domain *domain,
				 struct task *task, uint32_t num_tasks);

#if CONFIG_SOF_USERSPACE_LL
	/**
	 * @brief Initialize the scheduling thread and privileged resources.
	 *
	 * Must be called from kernel context before any user-space tasks register.
	 * @param[in,out] domain Target scheduling domain.
	 * @param[in,out] task Task requiring privileged setup.
	 * @return 0 on success, negative error code on failure.
	 */
	int (*domain_thread_init)(struct ll_schedule_domain *domain,
				  struct task *task);

	/**
	 * @brief Free resources acquired by domain_thread_init().
	 * @param[in,out] domain Target scheduling domain.
	 * @param[in] num_tasks Current total task count.
	 */
	void (*domain_thread_free)(struct ll_schedule_domain *domain,
				   uint32_t num_tasks);
#endif

	/**
	 * @brief Enable domain interrupts/triggers on the specified core.
	 * @param[in,out] domain Target scheduling domain.
	 * @param[in] core Core index to enable.
	 */
	void (*domain_enable)(struct ll_schedule_domain *domain, int core);

	/**
	 * @brief Disable domain interrupts/triggers on the specified core.
	 * @param[in,out] domain Target scheduling domain.
	 * @param[in] core Core index to disable.
	 */
	void (*domain_disable)(struct ll_schedule_domain *domain, int core);

#if CONFIG_CROSS_CORE_STREAM
	/**
	 * @brief Temporarily block domain execution across cores without disabling interrupt triggers.
	 * @param[in,out] domain Target scheduling domain.
	 */
	void (*domain_block)(struct ll_schedule_domain *domain);

	/**
	 * @brief Unblock domain execution across cores.
	 * @param[in,out] domain Target scheduling domain.
	 */
	void (*domain_unblock)(struct ll_schedule_domain *domain);
#endif

	/**
	 * @brief Set the next tick timestamp for the domain.
	 * @param[in,out] domain Target scheduling domain.
	 * @param[in] start Next execution start timestamp in clock ticks.
	 */
	void (*domain_set)(struct ll_schedule_domain *domain, uint64_t start);

	/**
	 * @brief Clear scheduled domain interrupt / tick timer.
	 * @param[in,out] domain Target scheduling domain.
	 */
	void (*domain_clear)(struct ll_schedule_domain *domain);

	/**
	 * @brief Query whether a task in the domain has pending execution work.
	 * @param[in] domain Target scheduling domain.
	 * @param[in] task Task to check.
	 * @param[out] comp Pointer to associated component device, if any.
	 * @return true if work is pending, false otherwise.
	 */
	bool (*domain_is_pending)(struct ll_schedule_domain *domain,
				  struct task *task, struct comp_dev **comp);

	/**
	 * @brief Notification hook invoked when a task within the domain is cancelled.
	 * @param[in,out] domain Target scheduling domain.
	 * @param[in,out] task Task being cancelled.
	 */
	void (*domain_task_cancel)(struct ll_schedule_domain *domain, struct task *task);
};

/**
 * @brief Low-Latency scheduling domain instance descriptor.
 */
struct ll_schedule_domain {
	uint64_t next_tick;		/**< Ticks just set for next run. */
	uint64_t new_target_tick;	/**< For the next set, used during the reschedule stage. */
#ifdef CONFIG_SOF_USERSPACE_LL
	struct k_mutex *lock;		/**< Domain mutex for user-space access. */
#else
	struct k_spinlock lock;         /**< Standard spinlock. */
#endif
	atomic_t total_num_tasks;	/**< Total number of registered tasks across all cores. */
	atomic_t enabled_cores;		/**< Number of currently enabled cores. */
	uint32_t ticks_per_ms;		/**< Number of clock ticks per millisecond. */
	int type;			/**< Domain type (SOF_SCHEDULE_LL_TIMER or SOF_SCHEDULE_LL_DMA). */
	int clk;			/**< Source clock identifier. */
	bool synchronous;		/**< True if tasks in this domain execute synchronously. */
	bool full_sync;			/**< True if tasks are fully synchronous with no time dependency. */
	void *priv_data;		/**< Pointer to private domain driver data. */
	bool enabled[CONFIG_CORE_COUNT];		/**< Per-core enablement flags. */
	const struct ll_schedule_domain_ops *ops;	/**< Domain operations table. */
};

/**
 * @brief Set the private data pointer of a Low-Latency scheduling domain.
 * @param[in,out] domain Target domain.
 * @param[in] data Private context pointer.
 */
#define ll_sch_domain_set_pdata(domain, data) ((domain)->priv_data = (data))

/**
 * @brief Retrieve the private data pointer of a Low-Latency scheduling domain.
 * @param[in] domain Target domain.
 * @return Pointer to domain-specific context data.
 */
#define ll_sch_domain_get_pdata(domain) ((domain)->priv_data)

/**
 * @brief Get the primary platform timer scheduling domain.
 * @return Pointer to global timer scheduling domain.
 */
static inline struct ll_schedule_domain *timer_domain_get(void)
{
	return sof_get()->platform_timer_domain;
}

/**
 * @brief Get the primary platform DMA scheduling domain.
 * @return Pointer to global DMA scheduling domain.
 */
static inline struct ll_schedule_domain *dma_domain_get(void)
{
	return sof_get()->platform_dma_domain;
}

#ifdef CONFIG_SOF_USERSPACE_LL
/**
 * @brief Allocate a task structure in the user-space LL heap.
 * @return Pointer to allocated task, or NULL on failure.
 */
struct task *zephyr_ll_task_alloc(void);

/**
 * @brief Get the user-space heap for Low-Latency scheduling structures.
 * @return Pointer to the k_heap instance.
 */
struct k_heap *zephyr_ll_user_heap(void);

/**
 * @brief Verify that a pointer belongs to the user-space LL heap.
 * @param[in] heap Heap handle to check.
 * @return true if valid, false otherwise.
 */
bool zephyr_ll_user_heap_verify(struct k_heap *heap);

/**
 * @brief Initialize user-space memory partitions and access permissions for LL scheduling.
 */
void zephyr_ll_user_resources_init(void);

/**
 * @brief Grant thread access to per-core Low-Latency mutexes.
 * @param[in,out] thread Target thread.
 * @param[in] core Target core ID.
 */
void user_ll_grant_access(struct k_thread *thread, int core);

#endif /* CONFIG_SOF_USERSPACE_LL */

/**
 * @brief Initialize a Low-Latency scheduling domain descriptor.
 *
 * Allocates a @ref ll_schedule_domain on the user or kernel heap, initializes
 * synchronization locks, resets task and core counters, and computes tick conversion constants.
 *
 * @param[in] type Domain type (@ref SOF_SCHEDULE_LL_TIMER or @ref SOF_SCHEDULE_LL_DMA).
 * @param[in] clk Source clock identifier.
 * @param[in] synchronous True if domain tasks must run synchronously.
 * @param[in] ops Domain operations table pointer.
 * @return Pointer to initialized domain, or NULL on allocation failure.
 */
static inline struct ll_schedule_domain *domain_init
				(int type, int clk, bool synchronous,
				 const struct ll_schedule_domain_ops *ops)
{
	struct ll_schedule_domain *domain;

#ifdef CONFIG_SOF_USERSPACE_LL
	domain = sof_heap_alloc(zephyr_ll_user_heap(), SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT,
				sizeof(*domain), sizeof(void *));
	if (domain)
		memset(domain, 0, sizeof(*domain));
#else
	domain = rzalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT, sizeof(*domain));
#endif
	if (!domain)
		return NULL;
	domain->type = type;
	domain->clk = clk;
	domain->synchronous = synchronous;
	domain->full_sync = false;
#ifdef __ZEPHYR__
	domain->ticks_per_ms = k_ms_to_cyc_ceil64(1);
#else
	domain->ticks_per_ms = clock_ms_to_ticks(clk, 1);
#endif
	domain->ops = ops;
	/* maximum value means no tick has been set to timer */
	domain->next_tick = UINT64_MAX;
	domain->new_target_tick = UINT64_MAX;

#ifdef CONFIG_SOF_USERSPACE_LL
	/* Allocate mutex dynamically for userspace access */
	domain->lock = k_object_alloc(K_OBJ_MUTEX);
	if (!domain->lock) {
		sof_heap_free(zephyr_ll_user_heap(), domain);
		return NULL;
	}
	k_mutex_init(domain->lock);
#else
	k_spinlock_init(&domain->lock);
#endif
	atomic_init(&domain->total_num_tasks, 0);
	atomic_init(&domain->enabled_cores, 0);

	return domain;
}

/**
 * @brief Configure the next interrupt or tick timestamp for the domain.
 * @param[in,out] domain Target scheduling domain.
 * @param[in] start Next execution start timestamp in clock ticks.
 */
static inline void domain_set(struct ll_schedule_domain *domain, uint64_t start)
{
	if (domain->ops->domain_set)
		domain->ops->domain_set(domain, start);
	else
		domain->next_tick = start;
}

/**
 * @brief Clear/disarm the interrupt tick for the domain.
 * @param[in,out] domain Target scheduling domain.
 */
static inline void domain_clear(struct ll_schedule_domain *domain)
{
	if (domain->ops->domain_clear)
		domain->ops->domain_clear(domain);

	/* reset to denote no tick/interrupt is set */
	domain->next_tick = UINT64_MAX;
}

/**
 * @brief Notify the domain that a task has been cancelled.
 * @param[in,out] domain Target scheduling domain.
 * @param[in,out] task Cancelled task pointer.
 */
static inline void domain_task_cancel(struct ll_schedule_domain *domain,
				      struct task *task)
{
	if (domain->ops->domain_task_cancel)
		domain->ops->domain_task_cancel(domain, task);
}

#if CONFIG_SOF_USERSPACE_LL
/**
 * @brief Initialize domain scheduling thread and perform privileged resource allocation.
 *
 * Must be called from kernel context before user-space tasks register.
 *
 * @param[in,out] domain Target scheduling domain.
 * @param[in,out] task Task requiring privileged setup.
 * @return 0 on success, negative error code on failure.
 */
static inline int domain_thread_init(struct ll_schedule_domain *domain,
				     struct task *task)
{
	assert(domain->ops->domain_thread_init);

	return domain->ops->domain_thread_init(domain, task);
}

/**
 * @brief Free resources acquired by domain_thread_init().
 *
 * Must be called from kernel context.
 *
 * @param[in,out] domain Target scheduling domain.
 * @param[in] num_tasks Current task count.
 */
static inline void domain_thread_free(struct ll_schedule_domain *domain,
				      uint32_t num_tasks)
{
	if (domain->ops->domain_thread_free)
		domain->ops->domain_thread_free(domain, num_tasks);
}
#endif

/**
 * @brief Register a real-time task with the scheduling domain.
 * @param[in,out] domain Target scheduling domain.
 * @param[in,out] task Task to register.
 * @param[in] handler Work handler callback function.
 * @param[in,out] arg Private context argument passed to handler.
 * @return 0 on success, negative error code on failure.
 */
static inline int domain_register(struct ll_schedule_domain *domain,
				  struct task *task,
				  void (*handler)(void *arg), void *arg)
{
	int ret;

	assert(domain->ops->domain_register);

	ret = domain->ops->domain_register(domain, task, handler, arg);

	if (!ret)
		/* registered one more task, increase the count */
		atomic_add(&domain->total_num_tasks, 1);

	return ret;
}

/**
 * @brief Unregister a real-time task from the scheduling domain.
 * @param[in,out] domain Target scheduling domain.
 * @param[in,out] task Task to unregister.
 * @param[in] num_tasks Current total registered task count.
 */
static inline void domain_unregister(struct ll_schedule_domain *domain,
				     struct task *task, uint32_t num_tasks)
{
	int ret;

	assert(domain->ops->domain_unregister);

	/* unregistering a task, decrement the count */
	if (task)
		atomic_sub(&domain->total_num_tasks, 1);

	/*
	 * In some cases .domain_unregister() might not return, terminating the
	 * current thread, that's why we had to update state before calling it.
	 */
	ret = domain->ops->domain_unregister(domain, task, num_tasks);
	if (ret < 0 && task)
		/* Failed to unregister the domain, restore state */
		atomic_add(&domain->total_num_tasks, 1);
}

/**
 * @brief Enable Low-Latency scheduling domain execution on the specified core.
 * @param[in,out] domain Target scheduling domain.
 * @param[in] core Target core ID.
 */
static inline void domain_enable(struct ll_schedule_domain *domain, int core)
{
	if (!domain->enabled[core] && domain->ops->domain_enable) {
		domain->ops->domain_enable(domain, core);
		domain->enabled[core] = true;
		atomic_add(&domain->enabled_cores, 1);
	}
}

/**
 * @brief Disable Low-Latency scheduling domain execution on the specified core.
 * @param[in,out] domain Target scheduling domain.
 * @param[in] core Target core ID.
 */
static inline void domain_disable(struct ll_schedule_domain *domain, int core)
{
	if (domain->enabled[core] && domain->ops->domain_disable) {
		domain->ops->domain_disable(domain, core);
		domain->enabled[core] = false;
		atomic_sub(&domain->enabled_cores, 1);
	}
}

#if CONFIG_CROSS_CORE_STREAM
/**
 * @brief Temporarily block domain tick processing across cores without disabling interrupt triggers.
 * @param[in,out] domain Target scheduling domain.
 */
static inline void domain_block(struct ll_schedule_domain *domain)
{
	if (domain->ops->domain_block)
		domain->ops->domain_block(domain);
}

/**
 * @brief Resume domain tick processing across cores.
 * @param[in,out] domain Target scheduling domain.
 */
static inline void domain_unblock(struct ll_schedule_domain *domain)
{
	if (domain->ops->domain_unblock)
		domain->ops->domain_unblock(domain);
}
#endif

/**
 * @brief Query whether a task in the domain has pending execution work.
 * @param[in] domain Target scheduling domain.
 * @param[in] task Task to inspect.
 * @param[out] comp Pointer to associated component device pointer, if any.
 * @return true if work is pending, false otherwise.
 */
static inline bool domain_is_pending(struct ll_schedule_domain *domain,
				     struct task *task, struct comp_dev **comp)
{
	bool ret;

	assert(domain->ops->domain_is_pending);

	ret = domain->ops->domain_is_pending(domain, task, comp);

	return ret;
}


#ifndef __ZEPHYR__
/**
 * @brief Initialize timer-driven Low-Latency scheduling domain for non-Zephyr platforms.
 * @param[in,out] timer Hardware timer device pointer.
 * @param[in] clk Source clock identifier.
 * @return Pointer to initialized scheduling domain.
 */
struct ll_schedule_domain *timer_domain_init(struct timer *timer, int clk);
#else
#if CONFIG_DMA_DOMAIN
#define dma_multi_chan_domain_init(dma_array, num_dma, clk, aggregated_irq)\
	zephyr_dma_domain_init(dma_array, num_dma, clk)
/**
 * @brief Initialize DMA interrupt-driven Low-Latency scheduling domain on Zephyr.
 * @param[in] dma_array Array of DMA controllers tied to the domain.
 * @param[in] num_dma Number of DMA controllers in the array.
 * @param[in] clk Clock source identifier.
 * @return Pointer to initialized scheduling domain, or NULL on error.
 */
struct ll_schedule_domain *zephyr_dma_domain_init(struct dma *dma_array,
						  uint32_t num_dma,
						  int clk);
#endif /* CONFIG_DMA_DOMAIN */

/**
 * @brief Retrieve the global Low-Latency timer scheduling domain descriptor on Zephyr.
 * @return Pointer to active domain descriptor.
 */
struct ll_schedule_domain *zephyr_ll_domain(void);

/**
 * @brief Initialize the primary timer-driven Low-Latency scheduling domain on Zephyr.
 * @param[in] clk Clock source identifier.
 * @return Pointer to initialized scheduling domain, or NULL on failure.
 */
struct ll_schedule_domain *zephyr_domain_init(int clk);

#define timer_domain_init(timer, clk) zephyr_domain_init(clk)

#ifdef CONFIG_SOF_USERSPACE_LL
/**
 * @brief Retrieve the Zephyr domain thread TID for the domain on the current core.
 * @param[in] domain Target scheduling domain.
 * @return Pointer to k_thread instance.
 */
struct k_thread *zephyr_domain_thread_tid(struct ll_schedule_domain *domain);

/**
 * @brief Retrieve the Zephyr domain thread TID for the domain on a specific core.
 * @param[in] core Target core ID.
 * @return Pointer to k_thread instance.
 */
struct k_thread *zephyr_domain_thread_tid_for_core(int core);

/**
 * @brief Retrieve the user-space LL memory domain.
 * @return Pointer to k_mem_domain.
 */
struct k_mem_domain *zephyr_ll_mem_domain(void);

/**
 * @brief Retrieve the Zephyr LL thread handle for the calling core.
 * @return Pointer to k_thread instance.
 */
struct k_thread *zephyr_ll_domain_thread(void);
#endif /* CONFIG_SOF_USERSPACE_LL */

#ifdef CONFIG_SOF_FULL_ZEPHYR_APPLICATION
/**
 * @brief Allocate a completion semaphore for a Low-Latency task.
 * @param[in,out] task Target task.
 * @return 0 on success, negative error code on failure.
 */
__syscall int zephyr_ll_task_sem_alloc(struct task *task);

/**
 * @brief Free a completion semaphore for a Low-Latency task.
 * @param[in,out] task Target task.
 * @return 0 on success, negative error code on failure.
 */
__syscall int zephyr_ll_task_sem_free(struct task *task);
#include <zephyr/syscalls/ll_schedule_domain.h>
#else
int z_impl_zephyr_ll_task_sem_alloc(struct task *task);
int z_impl_zephyr_ll_task_sem_free(struct task *task);
#define zephyr_ll_task_sem_alloc z_impl_zephyr_ll_task_sem_alloc
#define zephyr_ll_task_sem_free z_impl_zephyr_ll_task_sem_free
#endif /* CONFIG_SOF_FULL_ZEPHYR_APPLICATION */
#endif /* __ZEPHYR__ */

/**
 * @brief Initialize multi-channel DMA scheduling domain.
 * @param[in] dma_array Array of DMA controllers.
 * @param[in] num_dma Number of DMA controllers.
 * @param[in] clk Clock source identifier.
 * @param[in] aggregated_irq True if IRQs from channels are aggregated into a single line.
 * @return Pointer to initialized scheduling domain.
 */
struct ll_schedule_domain *dma_multi_chan_domain_init(struct dma *dma_array,
						      uint32_t num_dma, int clk,
						      bool aggregated_irq);

/**
 * @brief Initialize single-channel DMA scheduling domain.
 * @param[in] dma_array Array of DMA controllers.
 * @param[in] num_dma Number of DMA controllers.
 * @param[in] clk Clock source identifier.
 * @return Pointer to initialized scheduling domain.
 */
struct ll_schedule_domain *dma_single_chan_domain_init(struct dma *dma_array,
						       uint32_t num_dma,
						       int clk);

/** @}*/

#endif /* __SOF_SCHEDULE_LL_SCHEDULE_DOMAIN_H__ */
