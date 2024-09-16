// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation. All rights reserved.
//
// Author: Tobiasz Dryjanski <tobiaszx.dryjanski@intel.com>

#include <sof/audio/component.h>
#include <sof/debug/telemetry/performance_monitor.h>
#include <sof/debug/telemetry/telemetry.h>
#include <sof/lib/cpu.h>
#include <sof/lib_manager.h>

#include <zephyr/sys/bitarray.h>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <adsp_debug_window.h>

#include <ipc/trace.h>
#include <ipc4/logging.h>
#include <ipc4/base_fw.h>
#include <ipc4/base_fw_vendor.h>

LOG_MODULE_DECLARE(ipc, CONFIG_SOF_LOG_LEVEL);

#define PERFORMANCE_DATA_ENTRIES_COUNT \
	(CONFIG_MEMORY_WIN_3_SIZE / sizeof(struct perf_data_item_comp))

SYS_BITARRAY_DEFINE_STATIC(performance_data_bit_array, PERFORMANCE_DATA_ENTRIES_COUNT);

struct perf_bitmap {
	sys_bitarray_t *array;
	uint16_t occupied;
	size_t size;
};

struct io_perf_monitor_ctx {
	struct k_spinlock lock;
	enum ipc4_perf_measurements_state_set state;
	struct perf_bitmap io_performance_data_bitmap;
	struct io_perf_data_item *io_perf_data;
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

__attribute__((unused))
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
	int idx = (item - perf_data);
	int ret = perf_bitmap_free(&performance_data_bitmap, idx);
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

int get_performance_data(struct global_perf_data * const global_perf_data)
{
	if (!global_perf_data) {
		tr_err(&ipc_tr, "IPC data is NULL");
		return -EINVAL;
	}

	size_t slots_count;
	size_t slot_idx = 0;
	struct telemetry_wnd_data *wnd_data =
		(struct telemetry_wnd_data *)ADSP_DW->slots[SOF_DW_TELEMETRY_SLOT];
	struct system_tick_info *systick_info =
		(struct system_tick_info *)wnd_data->system_tick_info;

	/* Fill one performance record with performance stats per core */
	for (int core_id = 0; core_id < CONFIG_MAX_CORE_COUNT; ++core_id) {
		if (!(cpu_enabled_cores() & BIT(core_id)))
			continue;
		memset(&global_perf_data->perf_items[slot_idx], 0, sizeof(struct perf_data_item));

		global_perf_data->perf_items[slot_idx].resource_id = core_id;
		global_perf_data->perf_items[slot_idx].avg_kcps =
			systick_info[core_id].avg_utilization;
		global_perf_data->perf_items[slot_idx].peak_kcps =
			systick_info[core_id].peak_utilization;
		++slot_idx;
	}
	slots_count = perf_bitmap_get_occupied(&performance_data_bitmap) + slot_idx;
	global_perf_data->perf_item_count = slots_count;

	/* fill the rest of the IPC records with data from
	 * components registered in MW3 for performance measurement
	 */
	for (int idx = 0; idx < perf_bitmap_get_size(&performance_data_bitmap) &&
	     slot_idx < slots_count; ++idx) {
		if (perf_bitmap_is_bit_clear(&performance_data_bitmap, idx))
			continue;
		global_perf_data->perf_items[slot_idx] = perf_data[idx].item;
		++slot_idx;
	}
	return 0;
}

int get_extended_performance_data(struct extended_global_perf_data * const ext_global_perf_data)
{
	if (!ext_global_perf_data) {
		tr_err(&ipc_tr, "IPC data is NULL");
		return -EINVAL;
	}

	size_t slots_count;
	size_t slot_idx = 0;
	uint64_t total_dsp_cycles[CONFIG_MAX_CORE_COUNT];

	/* TODO
	 * Setting temporary values here.
	 * Replace this with actual total dsp cycles info once it is available.
	 */
	for (int core_id = 0; core_id < CONFIG_MAX_CORE_COUNT; ++core_id)
		total_dsp_cycles[core_id] = 1;

	/* Fill one performance record per core with total dsp cycles */
	for (int core_id = 0; core_id < CONFIG_MAX_CORE_COUNT; ++core_id) {
		if (!(cpu_enabled_cores() & BIT(core_id)))
			continue;

		memset(&ext_global_perf_data->perf_items[slot_idx], 0,
		       sizeof(struct ext_perf_data_item));

		ext_global_perf_data->perf_items[slot_idx].resource_id = core_id;
		ext_global_perf_data->perf_items[slot_idx].module_total_dsp_cycles_consumed =
			total_dsp_cycles[core_id];
		++slot_idx;
	}

	slots_count = perf_bitmap_get_occupied(&performance_data_bitmap) + slot_idx;
	ext_global_perf_data->perf_item_count = slots_count;

	/* fill the rest of the IPC records with data from
	 * components registered in MW3 for performance measurement
	 */
	for (int idx = 0; idx < perf_bitmap_get_size(&performance_data_bitmap) &&
	     slot_idx < slots_count; ++idx) {
		if (perf_bitmap_is_bit_clear(&performance_data_bitmap, idx))
			continue;

		ext_global_perf_data->perf_items[slot_idx].resource_id =
			perf_data[idx].item.resource_id;
		ext_global_perf_data->perf_items[slot_idx].power_mode =
			perf_data[idx].item.power_mode;
		ext_global_perf_data->perf_items[slot_idx].is_removed =
			perf_data[idx].item.is_removed;
		ext_global_perf_data->perf_items[slot_idx].module_total_dsp_iterations =
			perf_data[idx].total_iteration_count;
		ext_global_perf_data->perf_items[slot_idx].module_total_dsp_cycles_consumed =
			perf_data[idx].total_cycles_consumed;
		ext_global_perf_data->perf_items[slot_idx].module_peak_dsp_cycles =
			perf_data[idx].item.peak_kcps * 1000;
		ext_global_perf_data->perf_items[slot_idx].module_peak_restricted_cycles =
			perf_data[idx].restricted_peak_cycles;
		ext_global_perf_data->perf_items[slot_idx].module_total_restricted_cycles_consumed =
			perf_data[idx].restricted_total_cycles;
		ext_global_perf_data->perf_items[slot_idx].module_total_restricted_iterations =
			perf_data[idx].restricted_total_iterations;
		ext_global_perf_data->perf_items[slot_idx].rsvd = 0;
		++slot_idx;
	}
	return 0;
}

void disable_performance_counters(void)
{
	for (int idx = 0; idx < perf_bitmap_get_size(&performance_data_bitmap); ++idx) {
		if (perf_bitmap_is_bit_clear(&performance_data_bitmap, idx))
			continue;
		if (perf_data[idx].item.is_removed)
			perf_data_free(&perf_data[idx]);
	}
}

int enable_performance_counters(void)
{
	struct sof_man_module *man_module;
	struct comp_dev *dev;
	uint32_t comp_id;
	const struct sof_man_fw_desc *desc;

	if (perf_measurements_state != IPC4_PERF_MEASUREMENTS_DISABLED)
		return -EINVAL;

	for (int lib_id = 0; lib_id < LIB_MANAGER_MAX_LIBS; ++lib_id) {
		if (lib_id == 0) {
			desc = basefw_vendor_get_manifest();
		} else {
#if CONFIG_LIBRARY_MANAGER
			desc = (struct sof_man_fw_desc *)lib_manager_get_library_manifest(lib_id);
#else
			desc = NULL;
#endif
		}
		if (!desc)
			continue;

		/* Reinitialize performance data for all created components */
		for (int mod_id = 0 ; mod_id < desc->header.num_module_entries; mod_id++) {
			man_module =
				(struct sof_man_module *)(desc + SOF_MAN_MODULE_OFFSET(mod_id));

			for (int inst_id = 0; inst_id < man_module->instance_max_count; inst_id++) {
				comp_id = IPC4_COMP_ID(mod_id, inst_id);
				dev = ipc4_get_comp_dev(comp_id);

				if (dev)
					comp_init_performance_data(dev);
			}
		}
	}

	/* TODO clear total_dsp_ycles here once implemented */
	return 0;
}

int reset_performance_counters(void)
{
	if (perf_measurements_state == IPC4_PERF_MEASUREMENTS_DISABLED)
		return -EINVAL;

	struct telemetry_wnd_data *wnd_data =
			(struct telemetry_wnd_data *)ADSP_DW->slots[SOF_DW_TELEMETRY_SLOT];
	struct system_tick_info *systick_info =
			(struct system_tick_info *)wnd_data->system_tick_info;

	for (int core_id = 0; core_id < CONFIG_MAX_CORE_COUNT; ++core_id) {
		if (!(cpu_enabled_cores() & BIT(core_id)))
			continue;
		systick_info[core_id].peak_utilization = 0;
	}
	for (int idx = 0; idx < perf_bitmap_get_size(&performance_data_bitmap); ++idx) {
		if (!perf_bitmap_is_bit_clear(&performance_data_bitmap, idx))
			perf_data_item_comp_reset(&perf_data[idx]);
	}
	/* TODO clear totaldspcycles here once implemented */

	return 0;
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

#ifdef CONFIG_SOF_TELEMETRY_IO_PERFORMANCE_MEASUREMENTS

SYS_BITARRAY_DEFINE_STATIC(io_performance_data_bit_array, PERFORMANCE_DATA_ENTRIES_COUNT);

#define IO_PERFORMANCE_ALLOC_BYTES 0x1000
#define IO_PERFORMANCE_MAX_ENTRIES (IO_PERFORMANCE_ALLOC_BYTES / sizeof(struct io_perf_data_item))

static struct io_perf_monitor_ctx perf_monitor_ctx;
static struct io_perf_data_item io_perf_data_items[IO_PERFORMANCE_MAX_ENTRIES];

int io_perf_monitor_init(void)
{
	int ret;
	struct io_perf_monitor_ctx *self = &perf_monitor_ctx;

	k_spinlock_init(&self->lock);
	k_spinlock_key_t key = k_spin_lock(&self->lock);

	self->io_perf_data = (struct io_perf_data_item *)io_perf_data_items;
	self->state = IPC4_PERF_MEASUREMENTS_DISABLED;

	ret = perf_bitmap_init(&self->io_performance_data_bitmap, &io_performance_data_bit_array,
			       IO_PERFORMANCE_MAX_ENTRIES);

	k_spin_unlock(&self->lock, key);
	return ret;
}

static struct io_perf_data_item *io_perf_monitor_get_next_slot(struct io_perf_monitor_ctx *self)
{
	int idx;
	int ret;
	k_spinlock_key_t key = k_spin_lock(&self->lock);

	ret = perf_bitmap_alloc(&self->io_performance_data_bitmap, &idx);
	if (ret < 0)
		return NULL;
	/* ref. FW did not set the bits, but here we do it to not have to use
	 * isFree() check that the bitarray does not provide yet. Instead we will use isClear
	 * ,and always set bit on bitmap alloc.
	 */

	ret = perf_bitmap_setbit(&self->io_performance_data_bitmap, idx);
	if (ret < 0)
		return NULL;

	k_spin_unlock(&self->lock, key);
	return &self->io_perf_data[idx];
}

int io_perf_monitor_release_slot(struct io_perf_data_item *item)
{
	struct io_perf_monitor_ctx *self = &perf_monitor_ctx;
	int idx;
	int ret;
	k_spinlock_key_t key;

	if (!item) {
		tr_err(&ipc_tr, "perf_data_item is null");
		return -EINVAL;
	}

	item->is_removed = true;

	key = k_spin_lock(&self->lock);
	idx = item - self->io_perf_data;
	k_spin_unlock(&self->lock, key);

	/* we assign data items ourselves so neither of those should ever fail */
	ret = perf_bitmap_free(&self->io_performance_data_bitmap, idx);
	assert(!ret);
	return ret;
}

int
io_perf_monitor_get_performance_data(struct io_global_perf_data *io_global_perf_data)
{
	if (!io_global_perf_data)
		return 0;

	struct io_perf_monitor_ctx *self = &perf_monitor_ctx;
	k_spinlock_key_t key = k_spin_lock(&self->lock);

	size_t slot_idx = 0;
	size_t slots_count = self->io_performance_data_bitmap.occupied;
	size_t entries_cont = self->io_performance_data_bitmap.size;

	for (size_t idx = 0; idx < entries_cont && slot_idx < slots_count; ++idx) {
		if (perf_bitmap_is_bit_clear(&self->io_performance_data_bitmap, idx))
			continue;
		io_global_perf_data->perf_items[slot_idx] = self->io_perf_data[idx];
		++slot_idx;
	}
	io_global_perf_data->perf_item_count = slots_count;

	k_spin_unlock(&self->lock, key);
	return 0;
}

static int
io_perf_monitor_disable(struct io_perf_monitor_ctx *self)
{
	return 0;
}

static int
io_perf_monitor_stop(struct io_perf_monitor_ctx *self)
{
	size_t slot_idx = 0;
	size_t slots_count = self->io_performance_data_bitmap.occupied;
	size_t entries_cont = self->io_performance_data_bitmap.size;

	for (size_t idx = 0; idx < entries_cont && slot_idx < slots_count; ++idx) {
		if (perf_bitmap_is_bit_clear(&self->io_performance_data_bitmap, idx))
			continue;

		self->io_perf_data[idx].data = 0;
		++slot_idx;
	}

	return 0;
}

static int
io_perf_monitor_start(struct io_perf_monitor_ctx *self)
{
	return 0;
}

static int
io_perf_monitor_pause(struct io_perf_monitor_ctx *self)
{
	return 0;
}

int io_perf_monitor_set_state(enum ipc4_perf_measurements_state_set state)
{
	struct io_perf_monitor_ctx *self = &perf_monitor_ctx;
	int ret = 0;
	k_spinlock_key_t key = k_spin_lock(&self->lock);

	switch (state) {
	case IPC4_PERF_MEASUREMENTS_DISABLED:
		ret = io_perf_monitor_disable(self);
		break;
	case IPC4_PERF_MEASUREMENTS_STOPPED:
		ret = io_perf_monitor_stop(self);
		break;
	case IPC4_PERF_MEASUREMENTS_STARTED:
		ret = io_perf_monitor_start(self);
		break;
	case IPC4_PERF_MEASUREMENTS_PAUSED:
		ret = io_perf_monitor_pause(self);
		break;
	default:
		ret = -EINVAL;
	}

	if (ret == 0)
		self->state = state;

	k_spin_unlock(&self->lock, key);
	return ret;
}

inline enum ipc4_perf_measurements_state_set io_perf_monitor_get_state(void)
{
	struct io_perf_monitor_ctx *self = &perf_monitor_ctx;

	return self->state;
}

int io_perf_monitor_init_data(struct io_perf_data_item **slot_id,
			      struct io_perf_data_item *init_data)
{
	if (!slot_id)
		return IPC4_ERROR_INVALID_PARAM;

	struct io_perf_monitor_ctx *self = &perf_monitor_ctx;
	struct io_perf_data_item *new_slot = io_perf_monitor_get_next_slot(self);

	if (!new_slot)
		return IPC4_FAILURE;

	new_slot->id = init_data->id;
	new_slot->instance = init_data->instance;
	new_slot->direction = init_data->direction;
	new_slot->state = init_data->state;
	new_slot->power_mode = init_data->power_mode;
	new_slot->is_removed = false;
	new_slot->data = 0;

	*slot_id = new_slot;

	return 0;
}

void io_perf_monitor_update_data(struct io_perf_data_item *slot_id, uint32_t increment)
{
	if (!slot_id)
		return;

	struct io_perf_monitor_ctx *self = &perf_monitor_ctx;

	/* this does not need a lock if each perf slot has only one user */
	if (self->state == IPC4_PERF_MEASUREMENTS_STARTED)
		slot_id->data += increment;
}

inline void io_perf_monitor_update_io_state(struct io_perf_data_item *slot_id, bool const power_up)
{
	slot_id->state = power_up;
}

inline void io_perf_monitor_update_power_mode(struct io_perf_data_item *slot_id,
					      bool const power_mode)
{
	slot_id->power_mode = power_mode;
}
#endif /* CONFIG_SOF_TELEMETRY_IO_PERFORMANCE_MEASUREMENTS */
