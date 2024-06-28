// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation. All rights reserved.
//
// Author: Tobiasz Dryjanski <tobiaszx.dryjanski@intel.com>

#include <zephyr/sys/bitarray.h>

struct perf_bitmap {
	sys_bitarray_t *array;
	uint16_t occupied;
	size_t size;
};

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
