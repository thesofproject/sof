// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation. All rights reserved.
//
// Author: Jyri Sarha <jyri.sarha@linux.intel.com>

#include <stdbool.h>
#include <stdint.h>
#include <errno.h>

#include <sof/audio/component.h>
#include <sof/debug/telemetry/telemetry.h>
#include <sof/lib/fast-get.h>
#include <sof/lib/vregion.h>
#if CONFIG_SOF_USERSPACE_LL && CONFIG_SOF_VREGIONS
#include <sof/kernel/lib/vpage.h>
#endif
#if CONFIG_SOF_USERSPACE_LL
#include <sof/schedule/ll_schedule_domain.h>
#endif
#include <sof/objpool.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <rtos/kernel.h>
#include <rtos/spinlock.h>
#include <rtos/symbol.h>
#include <ipc/topology.h>

#include <zephyr/logging/log.h>

struct sof_fast_get_entry {
	const void *dram_ptr;
	void *sram_ptr;
	size_t size;
	unsigned int refcount;
	bool ll;	/* a copy for LL threads, see fast_get_ll() */
};

struct sof_fast_get_data {
#if CONFIG_SOF_USERSPACE_LL
	/* held while allocating large copies, see fast_get_large_alloc() */
	struct k_mutex lock;
#else
	struct k_spinlock lock;
#endif
	struct objpool_head pool;
};

static struct sof_fast_get_data fast_get_data = {
#if CONFIG_SOF_USERSPACE_LL
	.lock = Z_MUTEX_INITIALIZER(fast_get_data.lock),
#endif
	.pool.list = LIST_INIT(fast_get_data.pool.list),
};

#if CONFIG_SOF_USERSPACE_LL
typedef int fast_get_key_t;

static fast_get_key_t fast_get_lock(void)
{
	return k_mutex_lock(&fast_get_data.lock, K_FOREVER);
}

static void fast_get_unlock(fast_get_key_t key)
{
	k_mutex_unlock(&fast_get_data.lock);
}
#else
typedef k_spinlock_key_t fast_get_key_t;

static fast_get_key_t fast_get_lock(void)
{
	return k_spin_lock(&fast_get_data.lock);
}

static void fast_get_unlock(fast_get_key_t key)
{
	k_spin_unlock(&fast_get_data.lock, key);
}
#endif

#ifdef CONFIG_SOF_TELEMETRY_MEMORY
static uint32_t fast_get_total_calls;
static uint16_t fast_get_active_entries;
static uint16_t fast_get_fail_count;
static uint32_t fast_get_live_bytes;
static uint32_t fast_get_peak_bytes;
#endif

LOG_MODULE_REGISTER(fast_get, CONFIG_SOF_LOG_LEVEL);

struct fast_get_find {
	const void *ptr;
	bool ll;
	struct sof_fast_get_entry *entry;
};

/* finds an unused entry when find->ptr is NULL */
static bool fast_get_find_entry(void *data, void *arg)
{
	struct sof_fast_get_entry *entry = data;
	struct fast_get_find *find = arg;

	if (find->ptr != entry->dram_ptr || find->ll != entry->ll)
		return false;

	find->entry = entry;
	return true;
}

#if CONFIG_MM_DRV
#define PAGE_SZ CONFIG_MM_DRV_PAGE_SIZE
#define FAST_GET_MAX_COPY_SIZE (PAGE_SZ / 2)
#else
#include <sof/kernel/platform.h>
#define PAGE_SZ HOST_PAGE_SIZE
#define FAST_GET_MAX_COPY_SIZE 0
#endif

#if CONFIG_USERSPACE
/*
 * Removing a partition leaves an unused slot in domain->partitions[], so used
 * slots can be located beyond num_partitions. Scan all slots, skipping unused
 * ones, until all num_partitions used slots have been checked.
 */
static bool fast_get_partition_exists(struct k_mem_domain *domain, void *start, size_t size)
{
	unsigned int n = domain->num_partitions;

	for (unsigned int i = 0; i < arch_mem_domain_max_partitions_get() && n; i++) {
		struct k_mem_partition *dpart = &domain->partitions[i];

		if (!dpart->size)
			continue;

		if (dpart->start == (uintptr_t)start && dpart->size == size)
			return true;

		n--;
	}

	return false;
}

/*
 * Check whether the domain can already access the region via a partition
 * covering it, e.g. when the buffer was allocated from a heap which is
 * mapped to the domain as a whole (like the LL user-space heap). Adding
 * another partition in that case would fail as overlapping.
 */
static bool fast_get_partition_covers(struct k_mem_domain *domain, void *start, size_t size)
{
	uintptr_t addr = (uintptr_t)start;
	unsigned int n = domain->num_partitions;

	for (unsigned int i = 0; i < arch_mem_domain_max_partitions_get() && n; i++) {
		struct k_mem_partition *dpart = &domain->partitions[i];

		if (!dpart->size)
			continue;

		if (addr >= dpart->start && addr + size <= dpart->start + dpart->size)
			return true;

		n--;
	}

	return false;
}

static int fast_get_access_grant(struct k_mem_domain *mdom, void *addr, size_t size)
{
	struct k_mem_partition part = {
		.start = (uintptr_t)addr,
		.size = ALIGN_UP(size, CONFIG_MM_DRV_PAGE_SIZE),
		.attr = K_MEM_PARTITION_P_RO_U_RO | XTENSA_MMU_CACHED_WB,
	};

	LOG_DBG("add %#zx @ %p", part.size, addr);
	return k_mem_domain_add_partition(mdom, &part);
}
#endif /* CONFIG_USERSPACE */

/* large copies are page aligned and shared, see fast_get() */
static bool fast_get_is_large(size_t size)
{
	return IS_ENABLED(CONFIG_USERSPACE) && size > FAST_GET_MAX_COPY_SIZE;
}

#if CONFIG_SOF_USERSPACE_LL && CONFIG_SOF_VREGIONS
/*
 * The heap of the allocation contexts is the LL user heap, which kernel code
 * must not allocate from, since user threads can write its metadata. Take
 * large copies, which are page aligned and mapped read-only into the memory
 * domain of each user, from the page allocator instead.
 */
static void *fast_get_large_alloc(struct k_heap *heap, uint32_t flags, size_t size)
{
	return vpage_alloc(size / PAGE_SZ);
}

static void fast_get_large_free(struct k_heap *heap, void *ptr)
{
	vpage_free(ptr);
}
#else
static void *fast_get_large_alloc(struct k_heap *heap, uint32_t flags, size_t size)
{
	return sof_heap_alloc(heap, flags, size, PAGE_SZ);
}

static void fast_get_large_free(struct k_heap *heap, void *ptr)
{
	sof_heap_free(heap, ptr);
}
#endif

const void *fast_get(struct mod_alloc_ctx *alloc, const void *dram_ptr, size_t size)
{
	struct k_heap *heap = alloc ? alloc->heap : NULL;
#if CONFIG_USERSPACE
	bool current_is_userspace = thread_is_userspace(k_current_get());
#endif
	uint32_t alloc_flags = SOF_MEM_FLAG_USER;
	struct sof_fast_get_entry *entry;
	size_t alloc_size, alloc_align;
	const void *alloc_ptr;
	fast_get_key_t key;
	void *ret;

	key = fast_get_lock();
	if (fast_get_is_large(size)) {
		alloc_size = ALIGN_UP(size, PAGE_SZ);
		alloc_align = PAGE_SZ;
		alloc_flags |= SOF_MEM_FLAG_LARGE_BUFFER;
	} else {
		alloc_size = size;
		alloc_align = PLATFORM_DCACHE_ALIGN;
	}

	/* The module driver heap is shared by all instances of a given module.
	 * Instances can share the allocated buffer.
	 */
	if (size > FAST_GET_MAX_COPY_SIZE || !IS_ENABLED(CONFIG_USERSPACE) ||
	    IS_ENABLED(CONFIG_SOF_USERSPACE_USE_DRIVER_HEAP))
		alloc_ptr = dram_ptr;
	else
		/* When userspace is enabled only share large buffers */
		alloc_ptr = NULL;

	struct fast_get_find find = {
		.ptr = alloc_ptr,
	};
	objpool_iterate(&fast_get_data.pool, fast_get_find_entry, &find);
	entry = find.entry;
	if (!entry) {
		entry = objpool_alloc(&fast_get_data.pool, sizeof(*entry),
				      SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT);
		if (!entry) {
			ret = NULL;
			goto out;
		}
	}

#if CONFIG_USERSPACE
	LOG_DBG("%s: %#zx bytes alloc %p entry %p DRAM %p",
		current_is_userspace ? "userspace" : "kernel", size,
		alloc_ptr, entry->sram_ptr, dram_ptr);
#endif

	if (entry->sram_ptr) {
		if (entry->size != size || entry->dram_ptr != dram_ptr) {
			LOG_ERR("size %zu != %zu or ptr %p != %p mismatch",
				entry->size, size, entry->dram_ptr, dram_ptr);
			ret = NULL;
			goto out;
		}

		ret = entry->sram_ptr;

#if CONFIG_USERSPACE
		struct k_mem_domain *mdom = k_current_get()->mem_domain_info.mem_domain;

		/*
		 * We only get there for large buffers, since small buffers with
		 * enabled userspace don't create fast-get entries
		 * 
		 * We also reach this point when using the module driver heap.
		 * Since the heap is already shared across module instances,
		 * we skip memory domain manipulation.
		 */
		if (current_is_userspace && size > FAST_GET_MAX_COPY_SIZE) {
			if (!fast_get_partition_covers(mdom, ret,
						       ALIGN_UP(size, CONFIG_MM_DRV_PAGE_SIZE))) {
				LOG_DBG("grant access to domain %p", mdom);

				int err = fast_get_access_grant(mdom, ret, size);

				if (err < 0) {
					LOG_ERR("failed to grant additional access err=%d", err);
					ret = NULL;
					goto out;
				}
				/*
				 * The data is constant, so it's safe to use cached access to
				 * it, but initially we have to invalidate caches
				 */
				dcache_invalidate_region((__sparse_force void __sparse_cache *)ret,
							 size);
			} else if (fast_get_partition_exists(mdom, ret,
							     ALIGN_UP(size, CONFIG_MM_DRV_PAGE_SIZE))) {
				LOG_WRN("Repeated access request by thread");
			}
		}
#endif

		entry->refcount++;
		goto out;
	}

	if (alloc && alloc->vreg && size <= FAST_GET_MAX_COPY_SIZE)
		/* A userspace allocation, that won't be shared */
		ret = vregion_alloc_align(alloc->vreg, alloc_size,
					  alloc_align);
	else if (fast_get_is_large(size))
		ret = fast_get_large_alloc(heap, alloc_flags, alloc_size);
	else
		ret = sof_heap_alloc(heap, alloc_flags, alloc_size, alloc_align);
	if (!ret)
		goto out;

	memcpy_s(ret, alloc_size, dram_ptr, size);
	dcache_writeback_region((__sparse_force void __sparse_cache *)ret, size);

#if CONFIG_USERSPACE
	struct k_mem_domain *mdom = k_current_get()->mem_domain_info.mem_domain;

	/*
	 * Otherwise we've allocated on thread's heap, so it already has access.
	 * The same is true for large buffers allocated from a heap, that is
	 * mapped to the thread's domain as a whole.
	 */
	if (size > FAST_GET_MAX_COPY_SIZE && current_is_userspace &&
	    !fast_get_partition_covers(mdom, ret, ALIGN_UP(size, CONFIG_MM_DRV_PAGE_SIZE))) {
		int err = fast_get_access_grant(mdom, ret, size);

		if (err < 0) {
			LOG_ERR("failed to grant access err=%d", err);
			fast_get_large_free(heap, ret);
			ret = NULL;
			goto out;
		}
	}
#endif /* CONFIG_USERSPACE */

	entry->dram_ptr = dram_ptr;
	entry->size = size;
	entry->sram_ptr = ret;
	entry->refcount = 1;
#ifdef CONFIG_SOF_TELEMETRY_MEMORY
	fast_get_active_entries++;
	fast_get_live_bytes += (uint32_t)size;
	if (fast_get_live_bytes > fast_get_peak_bytes)
		fast_get_peak_bytes = fast_get_live_bytes;
#endif
out:
#ifdef CONFIG_SOF_TELEMETRY_MEMORY
	fast_get_total_calls++;
	if (!ret)
		fast_get_fail_count++;
#endif
	fast_get_unlock(key);
	LOG_DBG("get %p, %p, size %u, refcnt %u", dram_ptr, ret, size, entry ? entry->refcount : 0);

	return ret;
}
EXPORT_SYMBOL(fast_get);

#ifdef CONFIG_SOF_TELEMETRY_MEMORY
void telemetry_collect_fast_get_stats(struct sof_tel_fast_get_info *fg)
{
	if (!fg)
		return;

	fg->active_entries = fast_get_active_entries;
	fg->fail_count = fast_get_fail_count;
	fg->total_sram_bytes = fast_get_live_bytes;
	fg->peak_sram_bytes = fast_get_peak_bytes;
	fg->total_get_calls = fast_get_total_calls;
}
#endif

static bool fast_put_find_entry(void *data, void *arg)
{
	struct sof_fast_get_entry *entry = data;
	struct fast_get_find *find = arg;

	if (find->ptr != entry->sram_ptr || find->ll != entry->ll)
		return false;

	find->entry = entry;
	return true;
}

void fast_put(struct mod_alloc_ctx *alloc, struct k_mem_domain *mdom, const void *sram_ptr)
{
	struct k_heap *heap = alloc ? alloc->heap : NULL;
	struct sof_fast_get_entry *entry;
	fast_get_key_t key;

	key = fast_get_lock();

	struct fast_get_find find = {
		.ptr = sram_ptr,
	};
	objpool_iterate(&fast_get_data.pool, fast_put_find_entry, &find);
	entry = find.entry;
	if (!entry) {
		LOG_ERR("Put called to unknown address %p", sram_ptr);
		goto out;
	}

	entry->refcount--;

	if (!entry->refcount) {
#ifdef CONFIG_SOF_TELEMETRY_MEMORY
		if (fast_get_active_entries > 0)
			fast_get_active_entries--;
		if (fast_get_live_bytes >= (uint32_t)entry->size)
			fast_get_live_bytes -= (uint32_t)entry->size;
		else
			fast_get_live_bytes = 0;
#endif
		LOG_DBG("freeing buffer %p", sram_ptr);
		/*
		 * Free pages while the domain still maps them: vpage_free()
		 * writes back and invalidates the cache in the current domain.
		 */
		if (alloc && alloc->vreg && entry->size <= FAST_GET_MAX_COPY_SIZE)
			vregion_free(alloc->vreg, entry->sram_ptr);
		else if (fast_get_is_large(entry->size))
			fast_get_large_free(heap, entry->sram_ptr);
		else
			sof_heap_free(heap, entry->sram_ptr);
	}

#if CONFIG_USERSPACE
	/*
	 * For large buffers, each thread that called fast_get() has a partition
	 * in its memory domain. Each thread must remove its own partition here
	 * to prevent partition leaks. No partition was added, if the domain
	 * already had access to the buffer via a partition covering it.
	 */
	if (entry->size > FAST_GET_MAX_COPY_SIZE && mdom &&
	    fast_get_partition_exists(mdom, (void *)sram_ptr,
				      ALIGN_UP(entry->size, CONFIG_MM_DRV_PAGE_SIZE))) {
		struct k_mem_partition part = {
			.start = (uintptr_t)sram_ptr,
			.size = ALIGN_UP(entry->size, CONFIG_MM_DRV_PAGE_SIZE),
			.attr = K_MEM_PARTITION_P_RO_U_RO | XTENSA_MMU_CACHED_WB,
		};

		LOG_DBG("removing partition %p size %#zx memory domain %p",
			(void *)part.start, part.size, mdom);
		int err = k_mem_domain_remove_partition(mdom, &part);

		if (err)
			LOG_WRN("partition removal failed: %d", err);
	}
#endif

	if (!entry->refcount)
		memset(entry, 0, sizeof(*entry));
out:
	LOG_DBG("put %p, DRAM %p size %u refcnt %u", sram_ptr, entry ? entry->dram_ptr : 0,
		entry ? entry->size : 0, entry ? entry->refcount : 0);
	fast_get_unlock(key);
}
EXPORT_SYMBOL(fast_put);

#if CONFIG_SOF_USERSPACE_LL
/*
 * Copies for LL user threads, in the fast_get area of the LL memory region.
 * The area is mapped read-only into the LL memory domain, so they cannot
 * change a copy that other modules share. The entries and the allocator
 * state are in kernel memory. Only the cached alias of the area is mapped,
 * so the copy is written through the uncached alias, and the cached alias
 * is invalidated on the calling core: the core may hold lines of an earlier
 * copy at the same address. LL modules run on the core of their
 * pipeline, where they also get their copies.
 */
const void *z_impl_fast_get_ll(const void *dram_ptr, size_t size)
{
	struct fast_get_find find = {
		.ptr = dram_ptr,
		.ll = true,
	};
	struct sof_fast_get_entry *entry;
	const void *ret = NULL;
	fast_get_key_t key;
	void *copy;

	key = fast_get_lock();

	objpool_iterate(&fast_get_data.pool, fast_get_find_entry, &find);
	entry = find.entry;
	if (entry) {
		if (entry->size != size) {
			LOG_ERR("size %zu != %zu mismatch for %p", entry->size, size, dram_ptr);
			goto out;
		}

		entry->refcount++;
		ret = entry->sram_ptr;
		goto out;
	}

	find = (struct fast_get_find){ .ptr = NULL };
	objpool_iterate(&fast_get_data.pool, fast_get_find_entry, &find);
	entry = find.entry;
	if (!entry) {
		entry = objpool_alloc(&fast_get_data.pool, sizeof(*entry),
				      SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT);
		if (!entry)
			goto out;
	}

	copy = zephyr_ll_fg_alloc(size);
	if (!copy)
		goto out;

	memcpy_s(copy, size, dram_ptr, size);

	entry->dram_ptr = dram_ptr;
	entry->size = size;
	entry->sram_ptr = (__sparse_force void *)sys_cache_cached_ptr_get(copy);
	entry->refcount = 1;
	entry->ll = true;
	ret = entry->sram_ptr;
#ifdef CONFIG_SOF_TELEMETRY_MEMORY
	fast_get_active_entries++;
	fast_get_live_bytes += (uint32_t)size;
	if (fast_get_live_bytes > fast_get_peak_bytes)
		fast_get_peak_bytes = fast_get_live_bytes;
#endif
out:
	if (ret)
		sys_cache_data_invd_range((void *)ret, size);
#ifdef CONFIG_SOF_TELEMETRY_MEMORY
	fast_get_total_calls++;
	if (!ret)
		fast_get_fail_count++;
#endif
	fast_get_unlock(key);
	LOG_DBG("get %p, %p, size %zu, refcnt %u", dram_ptr, ret, size,
		ret ? entry->refcount : 0);

	return ret;
}

void z_impl_fast_put_ll(const void *sram_ptr)
{
	struct fast_get_find find = {
		.ptr = sram_ptr,
		.ll = true,
	};
	struct sof_fast_get_entry *entry;
	fast_get_key_t key;

	key = fast_get_lock();

	objpool_iterate(&fast_get_data.pool, fast_put_find_entry, &find);
	entry = find.entry;
	if (!entry) {
		LOG_ERR("Put called to unknown address %p", sram_ptr);
		goto out;
	}

	if (--entry->refcount)
		goto out;

#ifdef CONFIG_SOF_TELEMETRY_MEMORY
	if (fast_get_active_entries > 0)
		fast_get_active_entries--;
	if (fast_get_live_bytes >= (uint32_t)entry->size)
		fast_get_live_bytes -= (uint32_t)entry->size;
	else
		fast_get_live_bytes = 0;
#endif
	zephyr_ll_fg_free(entry->sram_ptr, entry->size);
	memset(entry, 0, sizeof(*entry));
out:
	fast_get_unlock(key);
}

#ifdef CONFIG_USERSPACE
#include <zephyr/internal/syscall_handler.h>
#include <sof/userspace/syscall_caller.h>

/*
 * Only the LL and IPC user threads may use the fast_get area: the copies
 * are read-only to the LL memory domain only, and the entries count the
 * users of the LL modules only.
 */
const void *z_vrfy_fast_get_ll(const void *dram_ptr, size_t size)
{
	SOF_SYSCALL_CALLER_CHECK(SOF_SYSUSER_INFRA);
	K_OOPS(K_SYSCALL_MEMORY_READ(dram_ptr, size));

	return z_impl_fast_get_ll(dram_ptr, size);
}
#include <zephyr/syscalls/fast_get_ll_mrsh.c>

void z_vrfy_fast_put_ll(const void *sram_ptr)
{
	SOF_SYSCALL_CALLER_CHECK(SOF_SYSUSER_INFRA);

	z_impl_fast_put_ll(sram_ptr);
}
#include <zephyr/syscalls/fast_put_ll_mrsh.c>
#endif /* CONFIG_USERSPACE */
#endif /* CONFIG_SOF_USERSPACE_LL */
