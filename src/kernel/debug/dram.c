// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <rtos/kernel.h>
#include <rtos/symbol.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>

LOG_MODULE_REGISTER(dbg_path, CONFIG_SOF_LOG_LEVEL);

static struct k_spinlock hot_path_lock;
static unsigned int hot_path_depth;
static const char *cold_path_fn;
static bool hot_path_confirmed;

void dbg_path_cold_enter(const char *fn)
{
	k_spinlock_key_t key = k_spin_lock(&hot_path_lock);

	cold_path_fn = fn;

	k_spin_unlock(&hot_path_lock, key);
}
EXPORT_SYMBOL(dbg_path_cold_enter);

void dbg_path_hot_start_watching(void)
{
	k_spinlock_key_t key = k_spin_lock(&hot_path_lock);

	if (!hot_path_depth++) {
		cold_path_fn = NULL;
		hot_path_confirmed = false;
	}

	k_spin_unlock(&hot_path_lock, key);
}

void dbg_path_hot_confirm(void)
{
	k_spinlock_key_t key = k_spin_lock(&hot_path_lock);

	hot_path_confirmed = true;

	k_spin_unlock(&hot_path_lock, key);
}

void dbg_path_hot_stop_watching(void)
{
	bool underrun, error;
	k_spinlock_key_t key = k_spin_lock(&hot_path_lock);

	if (hot_path_depth) {
		underrun = false;
		hot_path_depth--;
		error = hot_path_confirmed && cold_path_fn;
	} else {
		error = underrun = true;
	}

	k_spin_unlock(&hot_path_lock, key);

	if (underrun)
		LOG_ERR("Hot path depth underrun!");
	else
		__ASSERT(!error, "Cold function %s() has run while on hot path!", cold_path_fn);
}
