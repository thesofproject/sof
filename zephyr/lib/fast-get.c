// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation. All rights reserved.
//
// Author: Jyri Sarha <jyri.sarha@linux.intel.com>

#include <stdbool.h>
#include <stdint.h>
#include <errno.h>

#include <sof/lib/fast-get.h>
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
#if CONFIG_USERSPACE
	struct k_thread *thread;
#endif
	size_t size;
	unsigned int refcount;
};

struct sof_fast_get_data {
	struct k_spinlock lock;
	size_t num_entries;
	struct sof_fast_get_entry *entries;
};

static struct sof_fast_get_data fast_get_data = {
	.num_entries = 0,
	.entries = NULL,
};

LOG_MODULE_REGISTER(fast_get, CONFIG_SOF_LOG_LEVEL);

static int fast_get_realloc(struct sof_fast_get_data *data)
{
	struct sof_fast_get_entry *entries;
	/*
	 * Allocate 8 entries for the beginning. Currently we only use 2 entries
	 * at most, so this should provide a reasonable first allocation.
	 */
	const unsigned int init_n_entries = 8;
	unsigned int n_entries = data->num_entries ? data->num_entries * 2 : init_n_entries;

	entries = rzalloc(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT,
			  n_entries * sizeof(*entries));
	if (!entries)
		return -ENOMEM;

	if (data->num_entries) {
		memcpy_s(entries, n_entries * sizeof(*entries), data->entries,
			 data->num_entries * sizeof(*entries));
		rfree(data->entries);
	}

	data->entries = entries;
	data->num_entries = n_entries;

	return 0;
}

static struct sof_fast_get_entry *fast_get_find_entry(struct sof_fast_get_data *data,
						      const void *dram_ptr)
{
	int i;

	for (i = 0; i < data->num_entries; i++) {
		if (data->entries[i].dram_ptr == dram_ptr)
			return &data->entries[i];
	}

	for (i = 0; i < data->num_entries; i++) {
		if (data->entries[i].dram_ptr == NULL)
			return &data->entries[i];
	}

	return NULL;
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
static int fast_get_access_grant(k_tid_t thread, void *addr, size_t size)
{
	struct k_mem_partition part = {
		.start = (uintptr_t)addr,
		.size = ALIGN_UP(size, CONFIG_MM_DRV_PAGE_SIZE),
		.attr = K_MEM_PARTITION_P_RO_U_RO | XTENSA_MMU_CACHED_WB,
	};

	LOG_DBG("add %#zx @ %p", part.size, addr);
	return k_mem_domain_add_partition(thread->mem_domain_info.mem_domain, &part);
}
#endif /* CONFIG_USERSPACE */

const void *fast_get(struct k_heap *heap, const void *dram_ptr, size_t size)
{
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

	do {
		entry = fast_get_find_entry(data, alloc_ptr);
		if (!entry) {
			if (fast_get_realloc(data)) {
				ret = NULL;
				goto out;
			}
		}
	} while (!entry);

#if CONFIG_USERSPACE
	LOG_DBG("userspace %u part %#zx bytes alloc %p entry %p DRAM %p",
		k_current_get()->mem_domain_info.mem_domain->num_partitions, size,
		alloc_ptr, entry->sram_ptr, dram_ptr);
#endif

	if (entry->sram_ptr) {
		if (entry->size != size || entry->dram_ptr != dram_ptr) {
			LOG_ERR("size %u != %u or ptr %p != %p mismatch",
				entry->size, size, entry->dram_ptr, dram_ptr);
			ret = NULL;
			goto out;
		}

#if CONFIG_USERSPACE
		/* We only get there for large buffers */
		if (k_current_get()->mem_domain_info.mem_domain->num_partitions > 1) {
			/* A userspace thread makes the request */
			if (k_current_get() != entry->thread) {
				int err = fast_get_access_grant(k_current_get(),
								entry->sram_ptr, size);

				if (err < 0) {
					ret = NULL;
					goto out;
				}
			} else {
				LOG_WRN("Repeated access request by thread");
			}
		}
#endif

		ret = entry->sram_ptr;
		entry->refcount++;
		/*
		 * The data is constant, so it's safe to use cached access to
		 * it, but initially we have to invalidate cached
		 */
		dcache_invalidate_region((__sparse_force void __sparse_cache *)ret, size);
		goto out;
	}

	/*
	 * If a userspace threads is the first user to fast-get the buffer, an
	 * SRAM copy will be allocated on its own heap, so it will have access
	 * to it
	 */
	ret = sof_heap_alloc(heap, alloc_flags, alloc_size, alloc_align);
	if (!ret)
		goto out;
	entry->size = size;
	entry->sram_ptr = ret;
	memcpy_s(entry->sram_ptr, entry->size, dram_ptr, size);
	dcache_writeback_region((__sparse_force void __sparse_cache *)entry->sram_ptr, size);

#if CONFIG_USERSPACE
	entry->thread = k_current_get();
	if (size > FAST_GET_MAX_COPY_SIZE) {
		/* Otherwise we've allocated on thread's heap, so it already has access */
		int err = fast_get_access_grant(entry->thread, ret, size);

		if (err < 0) {
			sof_heap_free(NULL, ret);
			ret = NULL;
			goto out;
		}
	}
#endif /* CONFIG_USERSPACE */

	entry->dram_ptr = dram_ptr;
	entry->refcount = 1;
out:
	k_spin_unlock(&data->lock, key);
	LOG_DBG("get %p, %p, size %u, refcnt %u", dram_ptr, ret, size, entry ? entry->refcount : 0);

	return ret;
}
EXPORT_SYMBOL(fast_get);

static struct sof_fast_get_entry *fast_put_find_entry(struct sof_fast_get_data *data,
						      const void *sram_ptr)
{
	int i;

	for (i = 0; i < data->num_entries; i++) {
		if (data->entries[i].sram_ptr == sram_ptr)
			return &data->entries[i];
	}

	return NULL;
}

void fast_put(struct k_heap *heap, const void *sram_ptr)
{
	struct sof_fast_get_data *data = &fast_get_data;
	struct sof_fast_get_entry *entry;
	k_spinlock_key_t key;

	key = k_spin_lock(&fast_get_data.lock);
	entry = fast_put_find_entry(data, sram_ptr);
	if (!entry) {
		LOG_ERR("Put called to unknown address %p", sram_ptr);
		goto out;
	}
	entry->refcount--;

#if CONFIG_USERSPACE
	if (entry->size > FAST_GET_MAX_COPY_SIZE &&
	    k_current_get()->mem_domain_info.mem_domain->num_partitions > 1) {
		struct k_mem_partition part = {
			.start = (uintptr_t)entry->sram_ptr,
			.size = ALIGN_UP(entry->size, CONFIG_MM_DRV_PAGE_SIZE),
			.attr = K_MEM_PARTITION_P_RO_U_RO | XTENSA_MMU_CACHED_WB,
		};

		LOG_DBG("remove %#zx @ %p", part.size, entry->sram_ptr);
		k_mem_domain_remove_partition(k_current_get()->mem_domain_info.mem_domain,
					      &part);
	}
#endif

	if (!entry->refcount) {
		sof_heap_free(heap, entry->sram_ptr);
		memset(entry, 0, sizeof(*entry));
	}
out:
	LOG_DBG("put %p, DRAM %p size %u refcnt %u", sram_ptr, entry ? entry->dram_ptr : 0,
	       entry ? entry->size : 0, entry ? entry->refcount : 0);
	k_spin_unlock(&data->lock, key);
}
EXPORT_SYMBOL(fast_put);
