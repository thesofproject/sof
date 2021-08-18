/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_LIB_CACHE_H__

#ifndef __ARCH_LIB_CACHE_H__
#define __ARCH_LIB_CACHE_H__

/*
 * Set virtual core count to align with config, but this can be overridden if
 * needed to stress multicore pipeline logic.
 */
#define CACHE_VCORE_COUNT	CONFIG_CORE_COUNT
#define DCACHE_LINE_SIZE 64

#ifdef TESTBENCH_CACHE_CHECK

#include <stddef.h>
#include <pthread.h>

/*
 * Check logic will memchk contents of all cache entries and
 * report differences on every tick.
 *
 * snapshot is also compared against cache and uncache to spot
 * local changes that are incoherent
 */

#define HOST_CACHE_ELEMS	1024
#define HOST_CACHE_DATA_SIZE	4096

/* tunable parameters */
#define TESTBENCH_CACHE_LINE_SIZE	64
#define TESTBENCH_BACKTRACE_SIZE	1024

/* cache action description */
enum tb_cache_action {
	CACHE_ACTION_NONE	= 0,
	CACHE_ACTION_WB		= 1,	/* cache writeback */
	CACHE_ACTION_INV	= 2,	/* cache invalidate */
};

/* data type for cache action */
enum tb_cache_data_type {
	CACHE_DATA_TYPE_HEAP_UNCACHE	= 0,	/* uncache allocated on heap */
	CACHE_DATA_TYPE_HEAP_CACHE	= 1,	/* cache allocated on heap */
	CACHE_DATA_TYPE_DATA_UNCACHE	= 2,	/* uncache DATA section */
	CACHE_DATA_TYPE_DATA_CACHE	= 3,	/* cache DATA section */
};

/*
 * Cache entry for a heap or data section object on a single core.
 * This tracks the object state and cache usage to detect data clobbering or
 * cache misuse for this core.
 */
struct tb_cache_entry {
	/* runtime infor */
	void *data;		/* points to current cache data */
	void *snapshot;		/* last snapshot on INV or WB action */
	int data_free;		/* does data need to be free() after use */
	int snaphost_new;	/* entry is new, so snapshot is dirty */
	enum tb_cache_data_type type;	/* heap, data */

	/* last user info */
	int line;		/* line of last INV/WB action */
	const char *func;	/* func of last INV/WB action */
	int core;		/* core of last INV/WB action */
	enum tb_cache_action action;	/* last action */

	/* backtrace info */
	void *symbols[TESTBENCH_BACKTRACE_SIZE];	/* last stack usage */
	int symbol_size;			/* back trace symbol size */

};

/*
 * Cache elem for heap or data object on ALL cores. This contains the uncahe
 * and per core cached entries for this object with some tracking data.
 */
struct tb_cache_elem {
	int id;			/* monotonic - used for developer ID only */
	int used;		/* in use - only used by heap objects */
	size_t size;		/* size in bytes */
	struct tb_cache_entry uncache;
	struct tb_cache_entry cache[CACHE_VCORE_COUNT];
};

/*
 * Global testbench cache context.
 * Each virtual core is presented as a thread.
 */
struct tb_cache_context {
	int num_elems;
	pthread_t thread_id[CACHE_VCORE_COUNT];
	struct tb_cache_elem elem[HOST_CACHE_ELEMS];
};

/* Global testbench cache context and current elem ID */
extern struct tb_cache_context *tb_cache;
extern int tb_elem_id;

/* developer debug options */
#define CACHE_DEBUG_STACK_TRACE		0
#define CACHE_DEBUG_CACHELINES		0
#define CACHE_DEBUG_MEM_TYPE		0
#define CACHE_DEBUG_ELEM_ID		0

/*
 * Dump the data object type i.e. it's either DATA or heap.
 */
#if CACHE_DEBUG_MEM_TYPE
static inline void _cache_dump_address_type(void *addr, size_t size)
{
	size_t heap;

	/* try and get ptr type */
	heap = malloc_usable_size(addr);
	if (!heap)
		fprintf(stdout, "  object is DATA %zu\n", size);
	else
		fprintf(stdout, "  object is HEAP %zu\n", size);
}
#else
static inline void _cache_dump_address_type(void *addr, size_t size) {}
#endif
/*
 * Dump the stack backtrace.
 */
#if CACHE_DEBUG_STACK_TRACE
static inline void _cache_dump_backtrace(void)
{
	void *backtrace_data[TESTBENCH_BACKTRACE_SIZE];
	int backtrace_size;

	backtrace_size = backtrace(backtrace_data, TESTBENCH_BACKTRACE_SIZE);
	backtrace_symbols_fd(backtrace_data, backtrace_size, 1);
}
#else
static inline void _cache_dump_backtrace(void) {}
#endif

/*
 * Dump the cachelines
 */
#if CACHE_DEBUG_CACHELINES
static inline void _cache_dump_cacheline(const char *text, char *base,
					 size_t offset, size_t size,
					 size_t region_size, char *base_diff)
{
	uint32_t *d, *diff;
	int i = 0, rem;

	if (!base_diff)
		base_diff = base;

	fprintf(stdout, "data: %s\n", text);

	if (offset > region_size) {
		fprintf(stdout, "error: offset %zu greater than region %zu\n",
				offset, region_size);
		return;
	}

	if (offset + size > region_size) {
		fprintf(stdout, "error: offset %zu + size %zu greater than region %zu\n",
				offset, size, region_size);
		size = region_size - offset;
		fprintf(stdout, "error: resized to %zu (CHECK CODE AS RESIZE NOT DONE IN HW)\n", size);
	}

	rem = size % 16;
	size -= rem;

	for (i = 0; i < size; i += 4) {
		d = (uint32_t *)(base + offset + i);
		diff = (uint32_t *)(base_diff + offset + i);
		if (i % 16 == 0)
			fprintf(stdout, "0x%4.4x : ", i);
		if (i % 16 == 12)
			fprintf(stdout, "0x%8.8x%c\n", d[0], d[0] == diff[0] ? ' ' : '?');
		else
			fprintf(stdout, "0x%8.8x%c ", d[0], d[0] == diff[0] ? ' ' : '?');
	}

	d = (uint32_t *)(base + offset + i);
	diff = (uint32_t *)(base_diff + offset + i);
	switch (rem) {
	case 4:
		fprintf(stdout, "0x%4.4x : 0x%8.8x%c\n", i, d[0], d[0] == diff[0] ? ' ' : '?');
		break;
	case 8:
		fprintf(stdout, "0x%4.4x : 0x%8.8x%c 0x%8.8x%c\n", i,
				d[0], d[0] == diff[0] ? ' ' : '?',
				d[1], d[1] == diff[1] ? ' ' : '?');
		break;
	case 12:
		fprintf(stdout, "0x%4.4x : 0x%8.8x%c 0x%8.8x%c 0x%8.8x%c\n", i,
				d[0], d[0] == diff[0] ? ' ' : '?',
				d[1], d[1] == diff[1] ? ' ' : '?',
				d[2], d[2] == diff[2] ? ' ' : '?');
		break;
	default:
		break;
	}

	fprintf(stdout, "\n");
}
#else
static inline void _cache_dump_cacheline(const char *text, char *base,
					size_t offset, size_t size,
					size_t region_size, char *base_diff)
{}
#endif

/*
 * Calculate the size of the cache operation in bytes (i.e. aligned to the
 * cache line size)
 */
static inline size_t _cache_op_size(size_t req_size)
{
	size_t tmp = req_size % TESTBENCH_CACHE_LINE_SIZE;

	return tmp ? req_size + TESTBENCH_CACHE_LINE_SIZE - tmp : req_size;
}

/*
 * Calculate the alignment offset of the cache operation in bytes (i.e. aligned
 * to the cache line size)
 */
static inline long _cache_op_offset(void *base, void *addr)
{
	unsigned long offset;

	assert(addr >= base);

	offset = (unsigned long)addr - (unsigned long)base;

	return -(offset % TESTBENCH_CACHE_LINE_SIZE);
}

/*
 * Get the current core ID from the thread ID. There will be a 1:1 mapping
 * between thread and core in testbench usage.
 */
static inline int _cache_find_core(const char *func, int line)
{
	int core;
	pthread_t thread_id;

	thread_id = pthread_self();

	/* find core */
	for (core = 0; core < CACHE_VCORE_COUNT; core++) {
		if (tb_cache->thread_id[core] == thread_id)
			return core;
	}

	fprintf(stdout, "error: cant find core for %lu - DEAD at %s:%d\n",
		thread_id, func, line);
	assert(0);
	return -1;
}

/*
 * Find elem based on cache address and core number.
 */
static inline struct tb_cache_elem *_cache_get_elem_from_cache(void *addr, int core)
{
	struct tb_cache_elem *elem;
	int i;

	/* find elem with cache address */
	for (i = 0; i < HOST_CACHE_ELEMS; i++) {
		elem = &tb_cache->elem[i];
		if (elem->cache[core].data == addr) {
#if CACHE_DEBUG_ELEM_ID
			fprintf(stdout, "   get celem id = %d\n", elem->id);
#endif
			return elem;
		}
	}

	/* not found */
	return NULL;
}

/*
 * Find elem based on uncache address.
 */
static inline struct tb_cache_elem *_cache_get_elem_from_uncache(void *addr)
{
	struct tb_cache_elem *elem;
	struct tb_cache_entry *uentry;
	int i;

	/* find elem with cache address */
	for (i = 0; i < HOST_CACHE_ELEMS; i++) {
		elem = &tb_cache->elem[i];
		uentry = &elem->uncache;
		if (uentry->data == addr) {
#if CACHE_DEBUG_ELEM_ID
			fprintf(stdout, "   get uelem id = %d\n", elem->id);
#endif
			return elem;
		}
	}

	/* not found */
	return NULL;
}

/*
 * Find first free elem.
 */
static inline struct tb_cache_elem *_cache_get_free_elem(void)
{
	struct tb_cache_elem *elem;
	int i;

	/* find elem with cache address */
	for (i = 0; i < HOST_CACHE_ELEMS; i++) {
		elem = &tb_cache->elem[i];
		if (elem->used)
			continue;
		elem->id = tb_elem_id++;
		elem->used = 1;
#if CACHE_DEBUG_ELEM_ID
		fprintf(stdout, "   elem %p id = %d\n", elem, elem->id);
#endif
		return elem;
	}

	/* not found */
	return NULL;
}

/*
 * Create and setup a new ucache entry
 */
static inline void _cache_set_udata(struct tb_cache_elem *elem, int core,
				    const char *func, int line,
				    enum tb_cache_data_type type,
				    void *address, size_t size,
				    size_t alloc_size, int alloc,
				    enum tb_cache_action action)
{
	struct tb_cache_entry *uentry = &elem->uncache;

	uentry->func = func;
	uentry->line = line;
	uentry->type = type;
	uentry->core = core;
	uentry->action = action;
	elem->size = size;

	assert(!uentry->data);

	/* are we using client copy or do we allocate our copy */
	if (alloc) {
		uentry->data_free = 1;
		uentry->data = calloc(alloc_size, 1);
	} else
		uentry->data = address;

	uentry->snapshot = malloc(alloc_size);
	uentry->snaphost_new = 1;

	/* memcpy and take a snapshot of original data for comparison later */
	memcpy(uentry->snapshot, uentry->data, size);
}

/*
 * Create and setup a new cache entry
 */
static inline void _cache_new_data(struct tb_cache_elem *elem, int core,
				   const char *func, int line,
				   enum tb_cache_data_type type,
				   void *address, size_t size,
				   size_t alloc_size, int alloc)
{
	struct tb_cache_entry *centry = &elem->cache[core];

	centry->func = func;
	centry->line = line;
	centry->type = type;

	assert(!centry->data);

	/* are we using client copy or do we allocate our copy */
	if (alloc) {
		centry->data_free = 1;
		centry->data = calloc(alloc_size, 1);
	} else
		centry->data = address;

	centry->snapshot = malloc(alloc_size);
	centry->snaphost_new = 1;

	/* memcpy and take a snapshot of original data for comparison later */
	memcpy(centry->snapshot, centry->data, size);
}

/*
 * Create a new elem from a cached address
 */
static inline struct tb_cache_elem *_cache_new_celem(void *addr, int core,
						     const char *func, int line,
						     enum tb_cache_data_type type,
						     size_t size,
						     enum tb_cache_action action)
{
	struct tb_cache_elem *elem;
	int i;
	size_t aligned_size = _cache_op_size(size);

	elem = _cache_get_free_elem();
	if (!elem) {
		fprintf(stdout, "!!no free elems for ccache!\n");
		return NULL;
	}
#if CACHE_DEBUG_ELEM_ID
	fprintf(stdout, "  new c cache elem size %zu:0x%lx\n", size, size);
#endif
	/* create the uncache mapping  */
	_cache_set_udata(elem, core, func, line, type, addr, size,
			 aligned_size, 1, action);

	/* create the cache mappings - we only alloc for new entries */
	for (i = 0; i < CACHE_VCORE_COUNT; i++)
		_cache_new_data(elem, i, func, line, type, addr, size,
				aligned_size, i == core ? 0 : 1);

	return elem;
}

/*
 * Create a new elem from a uncached address
 */
static inline struct tb_cache_elem *
	_cache_new_uelem(void *addr, int core, const char *func, int line,
			 enum tb_cache_data_type type, size_t size,
			 enum tb_cache_action action)
{
	struct tb_cache_elem *elem;
	int i;
	size_t aligned_size = _cache_op_size(size);

	elem = _cache_get_free_elem();
	if (!elem) {
		fprintf(stdout, "!!no free elems for ucache!\n");
		return NULL;
	}
#if CACHE_DEBUG_ELEM_ID
	fprintf(stdout, "  new u cache elem size %zu:0x%lx\n", size, size);
#endif
	/* create the uncache mapping  */
	_cache_set_udata(elem, core, func, line, type, addr, size,
			 aligned_size, 0, action);

	/* create the cache mappings */
	for (i = 0; i < CACHE_VCORE_COUNT; i++)
		_cache_new_data(elem, i, func, line, type, addr, size,
				aligned_size, 1);

	return elem;
}

/*
 * Free a cache element.
 */
static inline void _cache_free_elem(struct tb_cache_elem *elem)
{
	struct tb_cache_entry *uentry = &elem->uncache;
	int core;

	/* TODO check coherency */

	for (core = 0; core < CACHE_VCORE_COUNT; core++) {
		if (elem->cache[core].data) {
			if (elem->cache[core].data_free)
				free(elem->cache[core].data);
			if (elem->cache[core].snapshot)
				free(elem->cache[core].snapshot);
		}
	}
	if (uentry->data_free)
		free(uentry->data);
	if (uentry->snapshot)
		free(uentry->snapshot);
	bzero(elem, sizeof(*elem));
}

/*
 * Free all cache elements.
 */
static inline void _cache_free_all(void)
{
	struct tb_cache_entry *uentry;
	struct tb_cache_elem *elem;
	int i, j;

	/* free all memory */
	for (i = 0; i < HOST_CACHE_ELEMS; i++) {
		elem = &tb_cache->elem[i];
		uentry = &elem->uncache;

		if (uentry->data_free)
			free(uentry->data);

		if (uentry->snapshot)
			free(uentry->snapshot);

		for (j = 0; j < CACHE_VCORE_COUNT; j++) {
			if (elem->cache[j].data_free)
				free(elem->cache[j].data);
			if (elem->cache[j].snapshot)
				free(elem->cache[j].snapshot);
		}
	}
}

/*
 * Invalidate clobber coherency check
 */
static inline void _tb_cache_elem_check_inv_snapshot(struct tb_cache_elem *elem,
						     int core, size_t offset,
						     size_t size,
						     const char *func, int line)
{
	struct tb_cache_entry *centry = &elem->cache[core];
	struct tb_cache_entry *uentry = &elem->uncache;
	int clobbered = 0, dirty;

	/* new snapshots are cant be compared */
	if (!centry->snaphost_new) {

		dirty = memcmp(centry->snapshot, centry->data, elem->size);
		if (dirty) {

			fprintf(stdout, "error: **** clobbering cache - "
				"dirty core %d cache being invalidated\n",
				core);
			_cache_dump_cacheline("snapshot",
					      (char *)centry->snapshot, offset,
					      size, elem->size, centry->data);
			clobbered = 1;
		}
	}

	/* not really clobbering, but potential to break stuff */
	if (uentry->action == CACHE_ACTION_INV && uentry->core != core) {
		fprintf(stdout, "error: **** possible clobbering cache - "
				"double invalidation with different cores and no writeback\n");
		clobbered = 1;
	}

	/* compare snapshot to local data, they should match during invalidation
	 * otherwise we are clobbering local data
	 */

	if (clobbered) {

		fprintf(stdout, "**** error: about to clobber by invalidate core %d elem %d\n",
			core, elem->id);
		fprintf(stdout, "  this user %s() line %d\n", func, line);

		fprintf(stdout, "  core %d last user %s() line %d\n",
			centry->core, centry->func, centry->line);
		backtrace_symbols_fd(centry->symbols, centry->symbol_size, 1);

		_cache_dump_cacheline("snapshot", (char *)centry->snapshot,
				      offset, size, elem->size, centry->data);
		_cache_dump_cacheline("data", (char *)centry->data, offset,
				      size, elem->size, centry->snapshot);

		_cache_dump_cacheline("uncache", (char *)elem->uncache.data,
				      offset, size, elem->size, NULL);
	}
}

/*
 * Writeback clobber coherency check
 * 1) Check that this core was last to invalidate or writeback ucache.
 * 2)
 */
static inline void _tb_cache_elem_check_wb_snapshot(struct tb_cache_elem *elem,
						    int core, size_t offset,
						    size_t size,
						    const char *func, int line)
{
	struct tb_cache_entry *uentry = &elem->uncache;
	int clobbered = 0;

	if (uentry->action == CACHE_ACTION_WB && uentry->core != core) {
		fprintf(stdout, "error: **** clobbering cache - two writeback from different cores\n");
		clobbered = 1;
	}

	if (uentry->action == CACHE_ACTION_INV && uentry->core != core) {
		fprintf(stdout, "error: **** clobbering cache - writeback without invalidation\n");
		clobbered = 1;
	}

	/* compare snapshot to local data, they should match during invalidation
	 * otherwise we are clobbering local data
	 */
	if (clobbered) {
		fprintf(stdout, "**** error: about to clobber by writeback elem %d\n",
			elem->id);
		fprintf(stdout, "  last user %s() line %d\n", uentry->func,
			uentry->line);
		backtrace_symbols_fd(uentry->symbols, uentry->symbol_size, 1);
		fprintf(stdout, "  this user %s() line %d\n", func, line);

		_cache_dump_cacheline("snapshot", (char *)uentry->snapshot,
				      offset, size, elem->size, uentry->data);
		_cache_dump_cacheline("uncache", (char *)uentry->data, offset,
				      size, elem->size, uentry->snapshot);
	}
}

static inline void _tb_cache_elem_update_csnapshot(struct tb_cache_elem *elem,
						   int core, size_t offset,
						   size_t size,
						   const char *func, int line,
						   enum tb_cache_action action)
{
	struct tb_cache_entry *centry = &elem->cache[core];
	struct tb_cache_entry *uentry = &elem->uncache;

	/* copy to snapshot area */
	memcpy((char *)centry->snapshot + offset,
	       (char *)elem->uncache.data + offset, size);
	centry->func = func;
	centry->line = line;
	centry->symbol_size = backtrace(centry->symbols,
					TESTBENCH_BACKTRACE_SIZE);
	uentry->action = action;
	uentry->core = core;
	uentry->snaphost_new = 0;
	centry->snaphost_new = 0;
}

static inline void _tb_cache_elem_update_usnapshot(struct tb_cache_elem *elem,
						   int core, size_t offset,
						   size_t size,
						   const char *func, int line,
						   enum tb_cache_action action)
{
	struct tb_cache_entry *centry = &elem->cache[core];
	struct tb_cache_entry *uentry = &elem->uncache;

	/* copy to snapshot area */
	memcpy((char *)uentry->snapshot + offset, (char *)centry->data + offset,
	       size);
	uentry->func = func;
	uentry->line = line;
	uentry->symbol_size = backtrace(uentry->symbols,
					TESTBENCH_BACKTRACE_SIZE);
	uentry->action = action;
	uentry->core = core;
	uentry->snaphost_new = 0;
}

/*
 * Invalidate cache elem from uncache mapping.
 */
static inline void _tb_cache_elem_invalidate(struct tb_cache_elem *elem,
					     int core, void *addr, size_t size,
					     const char *func, int line)
{
	struct tb_cache_entry *centry = &elem->cache[core];
	long offset = _cache_op_offset(centry->data, addr);
	size_t inv_size = _cache_op_size(size);
	int i;

	_tb_cache_elem_check_inv_snapshot(elem, core, offset, size, func, line);

	_cache_dump_cacheline("inv uncache src", (char *)elem->uncache.data,
			      offset, inv_size, elem->size, NULL);

	for (i = 0; i < CACHE_VCORE_COUNT; i++) {
		centry = &elem->cache[i];

		_cache_dump_cacheline("inv cache before", (char *)centry->data,
				offset, inv_size, elem->size,
				(char *)elem->uncache.data);

		/* copy offset and size are aligned to cache lines */
		memcpy((char *)centry->data + offset,
		       (char *)elem->uncache.data + offset, size);

		_tb_cache_elem_update_csnapshot(elem, i, offset, size, func,
						line, CACHE_ACTION_INV);

		_cache_dump_cacheline("inv after", (char *)centry->data, offset,
				      inv_size, elem->size, NULL);
	}
}

/*
 * Writeback cache elem from core N to uncache mapping.
 */
static inline void _tb_cache_elem_writeback(struct tb_cache_elem *elem,
					    int core, void *addr, size_t size,
					    const char *func, int line)
{
	struct tb_cache_entry *centry = &elem->cache[core];
	long offset = _cache_op_offset(centry->data, addr);
	size_t inv_size = _cache_op_size(size);

	_tb_cache_elem_check_wb_snapshot(elem, core, offset, size, func, line);

	_cache_dump_cacheline("wb uncache before", (char *)elem->uncache.data,
			      offset, inv_size, elem->size,
			      (char *)centry->data);

	/* copy to uncache  - use size as GCC spots the boundaries */
	memcpy((char *)elem->uncache.data + offset, (char *)centry->data + offset,
		size);

	_tb_cache_elem_update_usnapshot(elem, core, offset, size, func, line,
					CACHE_ACTION_WB);

	_cache_dump_cacheline("wb uncache after", (char *)elem->uncache.data,
			      offset, inv_size, elem->size, NULL);
}

static inline void _dcache_writeback_region(void *addr, size_t size,
					    const char *file, const char *func,
					    int line)
{
	int core = _cache_find_core(func, line);
	struct tb_cache_elem *elem;
	size_t phy_size = _cache_op_size(size);

	fprintf(stdout, "**dcache wb core %d %zu(%zu) bytes at %s() %d - %s\n",
		core, size, phy_size, func, line, file);

	_cache_dump_address_type(addr, size);
	_cache_dump_backtrace();

	/* are we writing back an existing cache object ? */
	elem = _cache_get_elem_from_cache(addr, core);
	if (!elem) {
		/* no elem found so create one */
		elem = _cache_new_celem(addr, core, func, line,
					CACHE_DATA_TYPE_DATA_CACHE, size,
					CACHE_ACTION_WB);
		if (!elem)
			return;
	}

	_tb_cache_elem_writeback(elem, core, addr, size, func, line);
}

static inline void _dcache_invalidate_region(void *addr, size_t size,
					     const char *file, const char *func,
					     int line)
{
	int core = _cache_find_core(func, line);
	struct tb_cache_elem *elem;
	size_t phy_size = _cache_op_size(size);

	fprintf(stdout, "**dcache inv core %d %zu(%zu) bytes at %s() %d - %s\n",
		core, size, phy_size, func, line, file);

	_cache_dump_address_type(addr, size);
	_cache_dump_backtrace();

	/* are we invalidating an existing cache object ? */
	elem = _cache_get_elem_from_cache(addr, core);
	if (!elem) {
		/* no elem found so create one */
		elem = _cache_new_celem(addr, core, func, line,
					CACHE_DATA_TYPE_DATA_CACHE, size,
					CACHE_ACTION_INV);
		if (!elem)
			return;
	}

	_tb_cache_elem_invalidate(elem, core, addr, size, func, line);
}

static inline void _icache_invalidate_region(void *addr, size_t size,
					     const char *file,
					     const char *func, int line)
{
	int core = _cache_find_core(func, line);
	struct tb_cache_elem *elem;
	size_t phy_size = _cache_op_size(size);

	fprintf(stdout, "**icache inv core %d %zu(%zu) bytes at %s() %d - %s\n",
		core, size, phy_size, func, line, file);

	_cache_dump_address_type(addr, size);
	_cache_dump_backtrace();

	/* are we invalidating an existing cache object ? */
	elem = _cache_get_elem_from_cache(addr, core);
	if (!elem) {
		/* no elem found so create one */
		elem = _cache_new_celem(addr, core, func, line,
					CACHE_DATA_TYPE_DATA_CACHE, size,
					CACHE_ACTION_INV);
		if (!elem)
			return;
	}

	_tb_cache_elem_invalidate(elem, core, addr, size, func, line);
}

static inline void _dcache_writeback_invalidate_region(void *addr, size_t size,
						       const char *file,
						       const char *func,
						       int line)
{
	int core = _cache_find_core(func, line);
	struct tb_cache_elem *elem;
	size_t phy_size = _cache_op_size(size);

	fprintf(stdout, "**dcache wb+inv core %d %zu(%zu) bytes at %s() %d - %s\n",
		core, size, phy_size, func, line, file);

	_cache_dump_address_type(addr, size);
	_cache_dump_backtrace();

	/* are we invalidating an existing cache object ? */
	elem = _cache_get_elem_from_cache(addr, core);
	if (!elem) {
		/* no elem found so create one */
		elem = _cache_new_celem(addr, core, func, line,
					CACHE_DATA_TYPE_DATA_CACHE, size,
					CACHE_ACTION_WB);
		if (!elem)
			return;
	}

	_tb_cache_elem_writeback(elem, core, addr, size, func, line);
	_tb_cache_elem_invalidate(elem, core, addr, size, func, line);
}

#define dcache_writeback_region(addr, size) \
	_dcache_writeback_region(addr, size, __FILE__, __func__, __LINE__)

#define dcache_invalidate_region(addr, size) \
	_dcache_invalidate_region(addr, size, __FILE__, __func__, __LINE__)

#define icache_invalidate_region(addr, size) \
	_icache_invalidate_region(addr, size, __FILE__, __func__, __LINE__)

#define dcache_writeback_invalidate_region(addr, size) \
	_dcache_writeback_invalidate_region(addr, size, __FILE__, __func__, __LINE__)

#else
static inline void dcache_writeback_region(void *addr, size_t size) {}
static inline void dcache_invalidate_region(void *addr, size_t size) {}
static inline void icache_invalidate_region(void *addr, size_t size) {}
static inline void dcache_writeback_invalidate_region(void *addr,
	size_t size) {}
#endif /* CONFIG_TESTBENCH_CACHE_CHECK */
#endif /* __ARCH_LIB_CACHE_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/cache.h"

#endif /* __SOF_LIB_CACHE_H__ */
