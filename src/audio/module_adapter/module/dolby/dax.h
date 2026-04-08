/* SPDX-License-Identifier: BSD-3-Clause
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY THIS LICENSE
 *
 * Copyright(c) 2025 Dolby Laboratories. All rights reserved.
 *
 * Author: Jun Lai <jun.lai@dolby.com>
 */

#include <sof/audio/module_adapter/module/generic.h>
#include <dax_inf.h>

#define DAX_USER_ID_INVALID 0
#define DAX_MAX_INSTANCE 2

enum dax_user_priority {
	DAX_USER_PRIORITY_DEFAULT = 0,
	DAX_USER_PRIORITY_P0 = 0,
	DAX_USER_PRIORITY_P1 = 1, /* Highest priority */
};

struct dax_adapter_data {
	struct sof_dax dax_ctx;
	atomic_t proc_flags;
	uint32_t comp_id;
	int32_t priority;
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
 * @brief Initialize global DAX instance manager state.
 */
void dax_instance_manager_init(void);

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
