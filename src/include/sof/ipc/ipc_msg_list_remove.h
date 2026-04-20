/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_IPC_IPC_MSG_LIST_REMOVE_H__
#define __SOF_IPC_IPC_MSG_LIST_REMOVE_H__

struct ipc_msg;

/**
 * \brief Remove an IPC message from the send queue.
 *
 * Acquires the IPC lock and removes the message from its list.
 * Safe to call from userspace.
 *
 * @param msg The IPC message to remove from the queue.
 */
#if defined(__ZEPHYR__) && defined(CONFIG_SOF_USERSPACE_LL)
__syscall void ipc_msg_list_remove(struct ipc_msg *msg);
#else
void z_impl_ipc_msg_list_remove(struct ipc_msg *msg);
#define ipc_msg_list_remove z_impl_ipc_msg_list_remove
#endif

#if defined(__ZEPHYR__) && defined(CONFIG_SOF_USERSPACE_LL)
#include <zephyr/syscalls/ipc_msg_list_remove.h>
#endif

#endif /* __SOF_IPC_IPC_MSG_LIST_REMOVE_H__ */
