/* SPDX-License-Identifier: BSD-3-Clause
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY THIS LICENSE
 *
 * Copyright(c) 2025 Dolby Laboratories. All rights reserved.
 *
 * Author: Jun Lai <jun.lai@dolby.com>
 */

#ifndef DAX_H
#define DAX_H

#include <rtos/atomic.h>
#include <rtos/spinlock.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <dax_inf.h>

#define DAX_USER_ID_INVALID 0
#define DAX_MAX_INSTANCE 2
#define DAX_PARAMS_STR_BUFFER_SIZE 1536

enum dax_user_priority {
	DAX_USER_PRIORITY_DEFAULT = 0,
	DAX_USER_PRIORITY_P0 = 0,
	DAX_USER_PRIORITY_P1 = 1, /* Highest priority */
};

/**
 * @brief DAX control parameters cached from the configuration thread.
 *
 * sof_dax_set_configuration() and sof_dax_process() may run in different
 * threads. Control parameters are only cached here by the configuration
 * thread and are applied to the DAX instance from the processing thread,
 * which avoids concurrent access to struct sof_dax.
 */
struct dax_cache_params {
	int32_t enable;
	int32_t profile;
	int32_t out_device;
	int32_t ctc_enable;
	int32_t content_processing_enable;
	int32_t volume;
	struct dax_buffer tuning_file;
};

struct dax_adapter_data {
	struct dax_cache_params cache_params;
	atomic_t proc_flags;
	uint32_t comp_id;
	int32_t priority;
	struct k_spinlock lock;
	char params_str[DAX_PARAMS_STR_BUFFER_SIZE];
	struct sof_dax dax_ctx;
};

/**
 * @brief Release memory used by a DAX buffer.
 *
 * @param[in] mod Pointer to the processing module.
 * @param[in,out] dax_buff Pointer to the DAX buffer to release.
 */
void dax_buffer_release(struct processing_module *mod, struct dax_buffer *dax_buff);

/**
 * @brief Allocate memory for a DAX buffer.
 *
 * @param[in] mod Pointer to the processing module.
 * @param[in,out] dax_buff Pointer to the DAX buffer to allocate.
 * @param[in] bytes Number of bytes to allocate.
 *
 * @return 0 on success, negative error code on failure.
 */
int dax_buffer_alloc(struct processing_module *mod, struct dax_buffer *dax_buff, uint32_t bytes);

/**
 * @brief Register the current module as a DAX instance user.
 *
 * @param[in] mod Pointer to the processing module.
 *
 * @return 0 on success, negative error code on failure.
 */
int dax_register_user(struct processing_module *mod);

/**
 * @brief Unregister the current module from DAX instance user management.
 *
 * @param[in] mod Pointer to the processing module.
 *
 * @return 0 on success, negative error code on failure.
 */
int dax_unregister_user(struct processing_module *mod);

/**
 * @brief Reconcile DAX instance allocation based on user priority.
 *
 * @param[in] mod Pointer to the processing module.
 *
 * @return 0 on success, negative error code on failure.
 */
int dax_check_and_update_instance(struct processing_module *mod);

#endif /* DAX_H */
