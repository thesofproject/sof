// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation. All rights reserved.
//
// Author: Tobiasz Dryjanski <tobiaszx.dryjanski@intel.com>

#include <zephyr/debug/sparse.h>
#include <zephyr/sys/bitarray.h>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/debug/telemetry/telemetry.h>

#include <ipc/trace.h>
#include <ipc4/base_fw.h>

#include <adsp_debug_window.h>
#include <errno.h>
#include <limits.h>
#include <mem_window.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_DECLARE(ipc, CONFIG_SOF_LOG_LEVEL);

/* Systic variables, one set per core */
static int telemetry_systick_counter[CONFIG_MAX_CORE_COUNT];
#ifdef CONFIG_SOF_TELEMETRY_PERFORMANCE_MEASUREMENTS
static int telemetry_prev_ccount[CONFIG_MAX_CORE_COUNT];
static int telemetry_perf_period_sum[CONFIG_MAX_CORE_COUNT];
static int telemetry_perf_period_cnt[CONFIG_MAX_CORE_COUNT];
static struct telemetry_perf_queue telemetry_perf_queue[CONFIG_MAX_CORE_COUNT];
#endif

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

#ifdef CONFIG_SOF_TELEMETRY_PERFORMANCE_MEASUREMENTS
static void telemetry_perf_queue_append(struct telemetry_perf_queue *q, size_t element)
{
	if (!q->full) {
		q->elements[q->index] = element;
		q->sum += element;
		q->index++;
		q->size++;
		if (q->index >= SOF_AVG_PERF_MEAS_DEPTH) {
			q->index = 0;
			q->size = SOF_AVG_PERF_MEAS_DEPTH;
			q->full = true;
		}
	} else {
		/* no space, pop tail */
		q->sum -= q->elements[q->index];
		/* replace tail */
		q->elements[q->index] = element;
		q->sum += element;
		/* move tail */
		q->index++;
		if (q->index >= SOF_AVG_PERF_MEAS_DEPTH)
			q->index = 0;
	}
}

static size_t telemetry_perf_queue_avg(struct telemetry_perf_queue *q)
{
	if (!q->size)
		return 0;
	return q->sum / q->size;
}
#endif

int telemetry_init(void)
{
	/* systick_init */
	uint8_t slot_num = SOF_DW_TELEMETRY_SLOT;
	volatile struct adsp_debug_window *window = ADSP_DW;
	struct telemetry_wnd_data *wnd_data = (struct telemetry_wnd_data *)ADSP_DW->slots[slot_num];
	struct system_tick_info *systick_info =
			(struct system_tick_info *)wnd_data->system_tick_info;

	tr_info(&ipc_tr, "Telemetry enabled. May affect performance");

	window->descs[slot_num].type = ADSP_DW_SLOT_TELEMETRY;
	window->descs[slot_num].resource_id = 0;
	wnd_data->separator_1 = 0x0000C0DE;

	/* Zero values per core */
	for (int i = 0; i < CONFIG_MAX_CORE_COUNT; i++) {
		systick_info[i].count = 0;
		systick_info[i].last_time_elapsed = 0;
		systick_info[i].max_time_elapsed = 0;
		systick_info[i].last_ccount = 0;
		systick_info[i].avg_utilization = 0;
		systick_info[i].peak_utilization = 0;
		systick_info[i].peak_utilization_4k = 0;
		systick_info[i].peak_utilization_8k = 0;
	}

	/* init global performance measurement */
	perf_data = (struct perf_data_item_comp *)ADSP_PMW;
	perf_bitmap_init(&performance_data_bitmap, &performance_data_bit_array,
			 PERFORMANCE_DATA_ENTRIES_COUNT);

	return 0;
}

void telemetry_update(uint32_t begin_stamp, uint32_t current_stamp)
{
	int prid = cpu_get_id();

	++telemetry_systick_counter[prid];

	struct telemetry_wnd_data *wnd_data =
		(struct telemetry_wnd_data *)ADSP_DW->slots[SOF_DW_TELEMETRY_SLOT];
	struct system_tick_info *systick_info =
		(struct system_tick_info *)wnd_data->system_tick_info;

	systick_info[prid].count = telemetry_systick_counter[prid];
	systick_info[prid].last_time_elapsed = current_stamp - begin_stamp;
	systick_info[prid].max_time_elapsed =
			MAX(current_stamp - begin_stamp,
			    systick_info[prid].max_time_elapsed);
	systick_info[prid].last_ccount = current_stamp;

#ifdef CONFIG_SOF_TELEMETRY_PERFORMANCE_MEASUREMENTS
	const size_t measured_systick = begin_stamp - telemetry_prev_ccount[prid];

	telemetry_prev_ccount[prid] = begin_stamp;
	if (telemetry_systick_counter[prid] > 2) {
		telemetry_perf_period_sum[prid] += measured_systick;
		telemetry_perf_period_cnt[prid] =
			(telemetry_perf_period_cnt[prid] + 1) % SOF_AVG_PERF_MEAS_PERIOD;
		if (telemetry_perf_period_cnt[prid] == 0) {
			/* Append average of last SOF_AVG_PERF_MEAS_PERIOD runs */
			telemetry_perf_queue_append(&telemetry_perf_queue[prid],
						    telemetry_perf_period_sum[prid]
						    / SOF_AVG_PERF_MEAS_PERIOD);
			telemetry_perf_period_sum[prid] = 0;
			/* Calculate average from all buckets */
			systick_info[prid].avg_utilization =
				telemetry_perf_queue_avg(&telemetry_perf_queue[prid]);
		}

		systick_info[prid].peak_utilization =
			MAX(systick_info[prid].peak_utilization,
			    measured_systick);
		systick_info[prid].peak_utilization_4k =
			MAX(systick_info[prid].peak_utilization_4k,
			    measured_systick);
		systick_info[prid].peak_utilization_8k =
			MAX(systick_info[prid].peak_utilization_8k,
			    measured_systick);

		/* optimized: counter % 0x1000 */
		if ((telemetry_systick_counter[prid] & 0xfff) == 0)
			systick_info[prid].peak_utilization_4k = 0;
		/* optimized: counter % 0x2000 */
		if ((telemetry_systick_counter[prid] & 0x1fff) == 0)
			systick_info[prid].peak_utilization_8k = 0;
	}
#endif
}

/* init telemetry using Zephyr*/
SYS_INIT(telemetry_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

