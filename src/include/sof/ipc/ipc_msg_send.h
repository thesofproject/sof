/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_IPC_IPC_MSG_SEND_H__
#define __SOF_IPC_IPC_MSG_SEND_H__

#include <stdbool.h>

struct ipc_msg;

/**
 * \brief Queues an IPC message for transmission.
 * @param msg The IPC message.
 * @param data The message data.
 * @param high_priority True if a high priority message.
 */
#if defined(__ZEPHYR__) && defined(CONFIG_SOF_USERSPACE_LL)
__syscall void ipc_msg_send(struct ipc_msg *msg, void *data,
			    bool high_priority);
#else
void z_impl_ipc_msg_send(struct ipc_msg *msg, void *data,
			  bool high_priority);
#define ipc_msg_send z_impl_ipc_msg_send
#endif

#if defined(__ZEPHYR__) && defined(CONFIG_SOF_USERSPACE_LL)
#include <zephyr/syscalls/ipc_msg_send.h>
#endif

#endif /* __SOF_IPC_IPC_MSG_SEND_H__ */
