/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Intel Corporation. All rights reserved.
 *
 * Author: Adrian Warecki <adrian.warecki@intel.com>
 */

#ifndef __SOF_IPC_NOTIFICATION_POOL_H__
#define __SOF_IPC_NOTIFICATION_POOL_H__

#include <stdint.h>
#include <sof/ipc/msg.h>

/**
 * @brief Retrieves an IPC notification message from the pool.
 *
 * This function retrieves and returns an IPC notification message
 * of the specified size from the notification pool. The size of the
 * message is limited by the maximum size available in the pool.
 *
 * @param size The size of the IPC message to retrieve.
 * @return A pointer to the retrieved IPC message, or NULL if retrieval fails.
 */
struct ipc_msg *ipc_notification_pool_get(size_t size);

#endif /* __SOF_IPC_NOTIFICATION_POOL_H__ */
