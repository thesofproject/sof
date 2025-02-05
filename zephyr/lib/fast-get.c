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
#include <rtos/spinlock.h>
#include <rtos/symbol.h>
#include <ipc/topology.h>

struct sof_fast_get_entry {
	const void *dram_ptr;
	void *sram_ptr;
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

	entries = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, SOF_MEM_FLAG_COHERENT, SOF_MEM_CAPS_RAM,
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

const void *fast_get(const void *dram_ptr, size_t size)
{
	struct sof_fast_get_data *data = &fast_get_data;
	struct sof_fast_get_entry *entry;
	k_spinlock_key_t key;
	void *ret;

	key = k_spin_lock(&data->lock);
	do {
		entry = fast_get_find_entry(data, dram_ptr);
		if (!entry) {
			if (fast_get_realloc(data)) {
				ret = NULL;
				goto out;
			}
		}
	} while (!entry);

	if (entry->sram_ptr) {
		if (entry->size != size || entry->dram_ptr != dram_ptr) {
			tr_err(fast_get, "size %u != %u or ptr %p != %p mismatch",
				entry->size, size, entry->dram_ptr, dram_ptr);
			ret = NULL;
			goto out;
		}

		ret = entry->sram_ptr;
		entry->refcount++;
		/*
		 * The data is constant, so it's safe to use cached access to
		 * it, but initially we have to invalidate cached
		 */
		dcache_invalidate_region((__sparse_force void __sparse_cache *)ret, size);
		goto out;
	}

	ret = rmalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, size);
	if (!ret)
		goto out;
	entry->size = size;
	entry->sram_ptr = ret;
	memcpy_s(entry->sram_ptr, entry->size, dram_ptr, size);
	entry->dram_ptr = dram_ptr;
	entry->refcount = 1;
out:
	k_spin_unlock(&data->lock, key);
	tr_dbg(fast_get, "get %p, %p, size %u, refcnt %u", dram_ptr, ret, size,
	       entry ? entry->refcount : 0);

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

void fast_put(const void *sram_ptr)
{
	struct sof_fast_get_data *data = &fast_get_data;
	struct sof_fast_get_entry *entry;
	k_spinlock_key_t key;

	key = k_spin_lock(&fast_get_data.lock);
	entry = fast_put_find_entry(data, sram_ptr);
	if (!entry) {
		tr_err(fast_get, "Put called to unknown address %p", sram_ptr);
		goto out;
	}
	entry->refcount--;
	if (entry->refcount > 0)
		goto out;
	rfree(entry->sram_ptr);
	memset(entry, 0, sizeof(*entry));
out:
	tr_dbg(fast_get, "put %p, DRAM %p size %u refcnt %u", sram_ptr, entry ? entry->dram_ptr : 0,
	       entry ? entry->size : 0, entry ? entry->refcount : 0);
	k_spin_unlock(&data->lock, key);
}
EXPORT_SYMBOL(fast_put);
