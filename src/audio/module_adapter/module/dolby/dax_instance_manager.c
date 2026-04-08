// SPDX-License-Identifier: BSD-3-Clause
//
// NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY THIS LICENSE
//
// Copyright(c) 2026 Dolby Laboratories. All rights reserved.
//
// Author: Jun Lai <jun.lai@dolby.com>
//

#include <stdbool.h>

#include <rtos/spinlock.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/compiler_attributes.h>

#include "dax.h"

LOG_MODULE_DECLARE(dolby_dax_audio_processing, CONFIG_SOF_LOG_LEVEL);

/* A DAX user refers to a DAX component, i.e., the processing_module
 * object. When a DAX user intends to process audio data, it first needs
 * to create a DAX instance. However, when multiple DAX users coexist
 * in the system simultaneously, each DAX user will request its own DAX instance.
 * Under limited memory resources, users that make later requests may fail to
 * allocate the required memory. To address this, each DAX user is assigned a
 * priority level, ensuring that higher-priority users are always granted
 * instance resources first, regardless of the order in which the requests
 * were made.
 *
 * The specific priority value will be automatically assigned in the dax_register_user
 * function based on the configuration of the DAX component.
 */
struct dax_user {
	uint32_t id; /* Component ID */
	enum dax_user_priority priority;
	bool allocated; /* Whether the instance memory has been allocated */
};

struct dax_instance_manager {
	struct dax_user users[DAX_MAX_INSTANCE];

	/* The maximum count of DAX instances that can be allocated simultaneously,
	 * determined by the system memory resource
	 */
	int32_t available_inst_cnt;

	struct k_spinlock lock;
	bool initialized;
};

static struct dax_instance_manager inst_mgr;

static void update_alloc_state_l(struct processing_module *mod)
{
	struct dax_adapter_data *adapter_data = module_get_private_data(mod);
	struct sof_dax *dax_ctx = &adapter_data->dax_ctx;
	struct dax_user *user = NULL;

	for (int i = 0; i < DAX_MAX_INSTANCE; i++) {
		if (inst_mgr.users[i].id == adapter_data->comp_id) {
			user = &inst_mgr.users[i];
			break;
		}
	}

	if (!user)
		return;

	if (dax_ctx->p_dax && dax_ctx->persist_buffer.addr && dax_ctx->scratch_buffer.addr)
		user->allocated = true;
	else
		user->allocated = false;
}

static int destroy_instance(struct processing_module *mod)
{
	struct dax_adapter_data *adapter_data = module_get_private_data(mod);
	struct sof_dax *dax_ctx = &adapter_data->dax_ctx;

	if (!dax_ctx->p_dax)
		return -EFAULT;

	/* free internal dax instance data and set dax_ctx->p_dax to NULL */
	dax_free(dax_ctx);
	dax_buffer_release(mod, &dax_ctx->persist_buffer);
	dax_buffer_release(mod, &dax_ctx->scratch_buffer);
	comp_info(mod->dev, "freed instance");

	return 0;
}

static int establish_instance(struct processing_module *mod)
{
	int ret;
	struct comp_dev *dev = mod->dev;
	struct dax_adapter_data *adapter_data = module_get_private_data(mod);
	struct sof_dax *dax_ctx = &adapter_data->dax_ctx;
	uint32_t persist_sz;
	uint32_t scratch_sz;

	if (dax_ctx->p_dax && dax_ctx->persist_buffer.addr && dax_ctx->scratch_buffer.addr)
		return -EEXIST;

	if (dax_ctx->persist_buffer.addr || dax_ctx->scratch_buffer.addr)
		destroy_instance(mod);

	persist_sz = dax_query_persist_memory(dax_ctx);
	if (dax_buffer_alloc(mod, &dax_ctx->persist_buffer, persist_sz) != 0) {
		comp_err(dev, "allocate %u bytes failed for persist", persist_sz);
		ret = -ENOMEM;
		goto err;
	}
	scratch_sz = dax_query_scratch_memory(dax_ctx);
	if (dax_buffer_alloc(mod, &dax_ctx->scratch_buffer, scratch_sz) != 0) {
		comp_err(dev, "allocate %u bytes failed for scratch", scratch_sz);
		ret = -ENOMEM;
		goto err;
	}
	ret = dax_init(dax_ctx);
	if (ret != 0) {
		comp_err(dev, "dax instance initialization failed, ret %d", ret);
		goto err;
	}

	comp_info(dev, "allocated: persist %u, scratch %u. version: %s",
		  persist_sz, scratch_sz, dax_get_version());
	return 0;

err:
	destroy_instance(mod);
	return ret;
}

static bool check_priority_l(struct processing_module *mod)
{
	struct dax_adapter_data *adapter_data = module_get_private_data(mod);
	struct dax_user *user = NULL;
	int32_t allocated_cnt = 0;

	for (int i = 0; i < DAX_MAX_INSTANCE; i++) {
		if (inst_mgr.users[i].id == adapter_data->comp_id) {
			user = &inst_mgr.users[i];
			break;
		}
	}

	/* If the current module (user) is not existing in user manager,
	 * instance memory allocation is not allowed.
	 */
	if (!user)
		return false;

	for (int i = 0; i < DAX_MAX_INSTANCE; i++) {
		if (inst_mgr.users[i].priority > user->priority && !inst_mgr.users[i].allocated)
			return false;
		if (inst_mgr.users[i].allocated)
			allocated_cnt++;
	}

	/* Resource is exhausted */
	if (!user->allocated && allocated_cnt >= inst_mgr.available_inst_cnt)
		return false;

	return true;
}

void dax_instance_manager_init(void)
{
	if (!inst_mgr.initialized) {
		k_spinlock_init(&inst_mgr.lock);
		/* Assume memory source is sufficient for maximum instances at the beginning */
		inst_mgr.available_inst_cnt = DAX_MAX_INSTANCE;
		inst_mgr.initialized = true;
	}
}

int dax_register_user(struct processing_module *mod)
{
	struct dax_adapter_data *adapter_data = module_get_private_data(mod);
	struct sof_dax *dax_ctx = &adapter_data->dax_ctx;
	struct dax_user *user = NULL;
	k_spinlock_key_t key;

	key = k_spin_lock(&inst_mgr.lock);
	for (int i = 0; i < DAX_MAX_INSTANCE; i++) {
		if (!user && inst_mgr.users[i].id == DAX_USER_ID_INVALID)
			user = &inst_mgr.users[i];

		if (inst_mgr.users[i].id == adapter_data->comp_id) {
			k_spin_unlock(&inst_mgr.lock, key);
			return -EEXIST;
		}
	}

	if (!user) {
		k_spin_unlock(&inst_mgr.lock, key);
		return -ENOSPC;
	}

	user->id = adapter_data->comp_id;
	if (dax_ctx->out_device == DAX_DEVICE_SPEAKER) {
		/* If the current component's output device is a speaker, it is assigned
		 * the highest priority.
		 */
		user->priority = DAX_USER_PRIORITY_P1;
	} else if (dax_ctx->out_device == DAX_DEVICE_HEADPHONE) {
		user->priority = DAX_USER_PRIORITY_P0;
	} else {
		user->priority = DAX_USER_PRIORITY_DEFAULT;
	}
	adapter_data->priority = user->priority;

	if (dax_ctx->p_dax && dax_ctx->persist_buffer.addr && dax_ctx->scratch_buffer.addr)
		user->allocated = true;
	else
		user->allocated = false;

	k_spin_unlock(&inst_mgr.lock, key);

	comp_info(mod->dev, "added user %#x, priority %d",
		  adapter_data->comp_id, adapter_data->priority);
	return 0;
}

int dax_unregister_user(struct processing_module *mod)
{
	struct dax_adapter_data *adapter_data = module_get_private_data(mod);
	struct dax_user *user = NULL;
	k_spinlock_key_t key;

	key = k_spin_lock(&inst_mgr.lock);
	for (int i = 0; i < DAX_MAX_INSTANCE; i++) {
		if (inst_mgr.users[i].id == adapter_data->comp_id) {
			user = &inst_mgr.users[i];
			break;
		}
	}

	if (!user) {
		k_spin_unlock(&inst_mgr.lock, key);
		return -EINVAL;
	}
	k_spin_unlock(&inst_mgr.lock, key);

	destroy_instance(mod);
	adapter_data->priority = DAX_USER_PRIORITY_DEFAULT;

	key = k_spin_lock(&inst_mgr.lock);
	user->id = DAX_USER_ID_INVALID;
	user->priority = DAX_USER_PRIORITY_DEFAULT;
	user->allocated = false;
	k_spin_unlock(&inst_mgr.lock, key);

	comp_info(mod->dev, "removed user %#x", adapter_data->comp_id);
	return 0;
}

int dax_check_and_update_instance(struct processing_module *mod)
{
	k_spinlock_key_t key;
	bool has_priority;
	int ret = 0;

	key = k_spin_lock(&inst_mgr.lock);
	/* Check whether any user that has higher priority is waiting for allocating instance
	 * memory (allocated == false), if is, current user should release its instance memory
	 * to make sure the higher priority user can allocate memory successfully.
	 */
	has_priority = check_priority_l(mod);
	k_spin_unlock(&inst_mgr.lock, key);

	/* This section of logic do not need lock protection because
	 * it does not involve access to any shared resources. `has_priority`
	 * may be stale when using in if condition but it is OK, `update_alloc_state_l`
	 * will synchronize instance state which will be used in the next check phase.
	 */
	if (has_priority)
		ret = establish_instance(mod);
	else
		ret = destroy_instance(mod);

	key = k_spin_lock(&inst_mgr.lock);
	update_alloc_state_l(mod);

	/* Auto detect and update the available memory count */
	if (ret == -ENOMEM) {
		inst_mgr.available_inst_cnt = 0;
		for (int i = 0; i < DAX_MAX_INSTANCE; i++) {
			if (inst_mgr.users[i].id != DAX_USER_ID_INVALID &&
			    inst_mgr.users[i].allocated) {
				inst_mgr.available_inst_cnt++;
			}
		}
	}
	k_spin_unlock(&inst_mgr.lock, key);
	return ret;
}
