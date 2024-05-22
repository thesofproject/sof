// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation. All rights reserved.
//
// Author: Tobiasz Dryjanski <tobiaszx.dryjanski@intel.com>

#include <sof/debug/telemetry/performance_monitor.h>
#include <zephyr/sys/bitarray.h>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <adsp_debug_window.h>

#include <ipc4/base_fw.h>

#define PERFORMANCE_DATA_ENTRIES_COUNT \
	(CONFIG_MEMORY_WIN_3_SIZE / sizeof(struct perf_data_item_comp))

SYS_BITARRAY_DEFINE_STATIC(performance_data_bit_array, PERFORMANCE_DATA_ENTRIES_COUNT);

struct perf_bitmap {
	sys_bitarray_t *array;
	uint16_t occupied;
	size_t size;
};

struct perf_bitmap performance_data_bitmap;

struct perf_data_item_comp *perf_data;

/* Note that ref. FW used one state per core, all set together to the same state
 * by one IPC but only for active cores. It may work slightly different in case
 * where we enable a core while perf meas is started.
 */
enum ipc4_perf_measurements_state_set perf_measurements_state = IPC4_PERF_MEASUREMENTS_DISABLED;

static int perf_bitmap_init(struct perf_bitmap * const bitmap, sys_bitarray_t *array, size_t size)
{
	k_spinlock_key_t key = k_spin_lock(&array->lock);

	bitmap->array = array;
	bitmap->size = size;
	bitmap->occupied = 0;

	k_spin_unlock(&array->lock, key);
	return 0;
}

static int perf_bitmap_alloc(struct perf_bitmap * const bitmap, size_t *offset)
{
	k_spinlock_key_t key;
	int ret = sys_bitarray_alloc(bitmap->array, 1, offset);

	if (!ret) {
		key = k_spin_lock(&bitmap->array->lock);
		bitmap->occupied++;
		k_spin_unlock(&bitmap->array->lock, key);
	}

	return ret;
}

static int perf_bitmap_free(struct perf_bitmap * const bitmap, size_t offset)
{
	k_spinlock_key_t key;
	int ret =  sys_bitarray_free(bitmap->array, 1, offset);

	if (!ret) {
		key = k_spin_lock(&bitmap->array->lock);
		bitmap->occupied--;
		k_spin_unlock(&bitmap->array->lock, key);
	}

	return ret;
}

static int perf_bitmap_setbit(struct perf_bitmap * const bitmap, size_t bit)
{
	return sys_bitarray_set_bit(bitmap->array, bit);
}

static int perf_bitmap_clearbit(struct perf_bitmap * const bitmap, size_t bit)
{
	return sys_bitarray_clear_bit(bitmap->array, bit);
}

static inline uint16_t perf_bitmap_get_occupied(struct perf_bitmap * const bitmap)
{
	return bitmap->occupied;
}

static inline uint16_t perf_bitmap_get_size(struct perf_bitmap * const bitmap)
{
	return bitmap->size;
}

static bool perf_bitmap_is_bit_clear(struct perf_bitmap * const bitmap, size_t bit)
{
	int val;
	int ret = sys_bitarray_test_bit(bitmap->array, bit, &val);

	if (ret < 0)
		return false;
	return !val;
}

struct perf_data_item_comp *perf_data_getnext(void)
{
	int idx;
	int ret = perf_bitmap_alloc(&performance_data_bitmap, &idx);

	if (ret < 0)
		return NULL;
	/* ref. FW did not set the bits, but here we do it to not have to use
	 * isFree() check that the bitarray does not provide yet. Instead we will use isClear
	 * ,and always set bit on bitmap alloc.
	 */
	ret = perf_bitmap_setbit(&performance_data_bitmap, idx);
	if (ret < 0)
		return NULL;
	return &perf_data[idx];
}

int perf_data_free(struct perf_data_item_comp * const item)
{
	/* find index of item */
	int idx = (item - perf_data) / sizeof(*item);
	int ret = perf_bitmap_clearbit(&performance_data_bitmap, idx);

	if (ret < 0)
		return ret;
	ret = perf_bitmap_free(&performance_data_bitmap, idx);
	if (ret < 0)
		return ret;

	return 0;
}

void perf_data_item_comp_reset(struct perf_data_item_comp *perf)
{
	perf->total_iteration_count = 0;
	perf->total_cycles_consumed = 0;
	perf->restricted_total_iterations = 0;
	perf->restricted_total_cycles = 0;
	perf->restricted_peak_cycles = 0;
	perf->item.peak_kcps = 0;
	perf->item.avg_kcps = 0;
}

void perf_data_item_comp_init(struct perf_data_item_comp *perf, uint32_t resource_id,
			      uint32_t power_mode)
{
	perf_data_item_comp_reset(perf);
	perf->item.resource_id = resource_id;
	perf->item.is_removed = false;
	perf->item.power_mode = power_mode;
}

int free_performance_data(struct perf_data_item_comp *item)
{
	int ret;

	if (item) {
		item->item.is_removed = true;
		/* if we don't get the disabled state now, item will be
		 * deleted on next disable perf meas message
		 */
		if (perf_measurements_state == IPC4_PERF_MEASUREMENTS_DISABLED) {
			ret = perf_data_free(item);
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}

enum ipc4_perf_measurements_state_set perf_meas_get_state(void)
{
	return perf_measurements_state;
}

void perf_meas_set_state(enum ipc4_perf_measurements_state_set state)
{
	perf_measurements_state = state;
}

int performance_monitor_init(void)
{
	/* init global performance measurement */
	perf_data = (struct perf_data_item_comp *)ADSP_PMW;
	perf_bitmap_init(&performance_data_bitmap, &performance_data_bit_array,
			 PERFORMANCE_DATA_ENTRIES_COUNT);

	return 0;
}

/* init performance monitor using Zephyr */
SYS_INIT(performance_monitor_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
