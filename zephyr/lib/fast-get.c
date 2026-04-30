// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation. All rights reserved.
//
// Author: Jyri Sarha <jyri.sarha@linux.intel.com>

#include <stdbool.h>
#include <stdint.h>
#include <errno.h>

#include <sof/audio/component.h>
#include <sof/lib/fast-get.h>
#include <sof/lib/vregion.h>
#include <sof/objpool.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <rtos/kernel.h>
#include <rtos/spinlock.h>
#include <rtos/symbol.h>
#include <ipc/topology.h>

#ifdef __ZEPHYR__
#include <zephyr/logging/log.h>
#else
#define LOG_DBG(...) do {} while (0)
#define LOG_INF(...) do {} while (0)
#define LOG_WRN(...) do {} while (0)
#define LOG_ERR(...) do {} while (0)
#endif

struct sof_fast_get_entry {
	const void *dram_ptr;
	void *sram_ptr;
	size_t size;
	unsigned int refcount;
};

struct sof_fast_get_data {
	struct k_spinlock lock;
	struct objpool_head pool;
};

static struct sof_fast_get_data fast_get_data = {
	.pool.list = LIST_INIT(fast_get_data.pool.list),
};

LOG_MODULE_REGISTER(fast_get, CONFIG_SOF_LOG_LEVEL);

struct fast_get_find {
	const void *ptr;
	struct sof_fast_get_entry *entry;
};

static bool fast_get_find_entry(void *data, void *arg)
{
	struct sof_fast_get_entry *entry = data;
	struct fast_get_find *find = arg;

	if (find->ptr != entry->dram_ptr)
		return false;

	find->entry = entry;
	return true;
}

#if CONFIG_MM_DRV
#define PAGE_SZ CONFIG_MM_DRV_PAGE_SIZE
#define FAST_GET_MAX_COPY_SIZE (PAGE_SZ / 2)
#else
#include <sof/platform.h>
#define PAGE_SZ HOST_PAGE_SIZE
#define FAST_GET_MAX_COPY_SIZE 0
#endif

#if CONFIG_USERSPACE
static bool fast_get_partition_exists(struct k_mem_domain *domain, void *start, size_t size)
{
	for (unsigned int i = 0; i < domain->num_partitions; i++) {
		struct k_mem_partition *dpart = &domain->partitions[i];

		if (dpart->start == (uintptr_t)start && dpart->size == size)
			return true;
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

const void *fast_get(struct mod_alloc_ctx *alloc, const void *dram_ptr, size_t size)
{
	struct k_heap *heap = alloc ? alloc->heap : NULL;
#if CONFIG_USERSPACE
	bool current_is_userspace = thread_is_userspace(k_current_get());
#endif
	struct sof_fast_get_data *data = &fast_get_data;
	uint32_t alloc_flags = SOF_MEM_FLAG_USER;
	struct sof_fast_get_entry *entry;
	size_t alloc_size, alloc_align;
	const void *alloc_ptr;
	k_spinlock_key_t key;
	void *ret;

	key = k_spin_lock(&data->lock);
	if (IS_ENABLED(CONFIG_USERSPACE) && size > FAST_GET_MAX_COPY_SIZE) {
		alloc_size = ALIGN_UP(size, PAGE_SZ);
		alloc_align = PAGE_SZ;
		alloc_flags |= SOF_MEM_FLAG_LARGE_BUFFER;
	} else {
		alloc_size = size;
		alloc_align = PLATFORM_DCACHE_ALIGN;
	}

	if (size > FAST_GET_MAX_COPY_SIZE || !IS_ENABLED(CONFIG_USERSPACE))
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
			LOG_ERR("size %u != %u or ptr %p != %p mismatch",
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
		 */
		if (current_is_userspace) {
			if (!fast_get_partition_exists(mdom, ret,
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
			} else {
				LOG_WRN("Repeated access request by thread");
			}
		}
#endif

		entry->refcount++;
		goto out;
	}

	if (alloc && alloc->vreg && size <= FAST_GET_MAX_COPY_SIZE)
		/* A userspace allocation, that won't be shared */
		ret = vregion_alloc_align(alloc->vreg, VREGION_MEM_TYPE_INTERIM, alloc_size,
					  alloc_align);
	else
		ret = sof_heap_alloc(heap, alloc_flags, alloc_size, alloc_align);
	if (!ret)
		goto out;

	memcpy_s(ret, alloc_size, dram_ptr, size);
	dcache_writeback_region((__sparse_force void __sparse_cache *)ret, size);

#if CONFIG_USERSPACE
	if (size > FAST_GET_MAX_COPY_SIZE && current_is_userspace) {
		/* Otherwise we've allocated on thread's heap, so it already has access */
		int err = fast_get_access_grant(k_current_get()->mem_domain_info.mem_domain,
						ret, size);

		if (err < 0) {
			LOG_ERR("failed to grant access err=%d", err);
			sof_heap_free(heap, ret);
			ret = NULL;
			goto out;
		}
	}
#endif /* CONFIG_USERSPACE */

	entry->dram_ptr = dram_ptr;
	entry->size = size;
	entry->sram_ptr = ret;
	entry->refcount = 1;
out:
	k_spin_unlock(&data->lock, key);
	LOG_DBG("get %p, %p, size %u, refcnt %u", dram_ptr, ret, size, entry ? entry->refcount : 0);

	return ret;
}
EXPORT_SYMBOL(fast_get);

static bool fast_put_find_entry(void *data, void *arg)
{
	struct sof_fast_get_entry *entry = data;
	struct fast_get_find *find = arg;

	if (find->ptr != entry->sram_ptr)
		return false;

	find->entry = entry;
	return true;
}

void fast_put(struct mod_alloc_ctx *alloc, struct k_mem_domain *mdom, const void *sram_ptr)
{
	struct k_heap *heap = alloc ? alloc->heap : NULL;
	struct sof_fast_get_data *data = &fast_get_data;
	struct sof_fast_get_entry *entry;
	k_spinlock_key_t key;

	key = k_spin_lock(&fast_get_data.lock);

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
		LOG_DBG("freeing buffer %p", sram_ptr);
		if (alloc && alloc->vreg && entry->size <= FAST_GET_MAX_COPY_SIZE)
			vregion_free(alloc->vreg, entry->sram_ptr);
		else
			sof_heap_free(heap, entry->sram_ptr);
	}

#if CONFIG_USERSPACE
	/*
	 * For large buffers, each thread that called fast_get() has a partition
	 * in its memory domain. Each thread must remove its own partition here
	 * to prevent partition leaks.
	 */
	if (entry->size > FAST_GET_MAX_COPY_SIZE && mdom) {
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
	k_spin_unlock(&data->lock, key);
}
EXPORT_SYMBOL(fast_put);
