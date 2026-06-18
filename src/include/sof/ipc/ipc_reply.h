/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_IPC_IPC_REPLY_H__
#define __SOF_IPC_IPC_REPLY_H__

#include <ipc/header.h>

struct sof_ipc_reply;

/**
 * \brief reply to an IPC message.
 * @param[in] reply pointer to the reply structure.
 */
#if defined(__ZEPHYR__) && defined(CONFIG_SOF_USERSPACE_LL)
__syscall void ipc_msg_reply(struct sof_ipc_reply *reply);
#else
void z_impl_ipc_msg_reply(struct sof_ipc_reply *reply);
#define ipc_msg_reply z_impl_ipc_msg_reply
#endif

#if defined(__ZEPHYR__) && defined(CONFIG_SOF_USERSPACE_LL)
#include <zephyr/syscalls/ipc_reply.h>
#endif

#endif /* __SOF_IPC_IPC_REPLY_H__ */
