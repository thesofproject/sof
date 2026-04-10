/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 */

#ifndef __SOF_IPC4_HANDLER_H__
#define __SOF_IPC4_HANDLER_H__

struct ipc4_message_request;

/**
 * \brief Processes IPC4 userspace module message.
 * @param[in] ipc4 IPC4 message request.
 * @param[in] reply IPC message reply structure.
 * @return IPC4_SUCCESS on success, error code otherwise.
 */
int ipc4_user_process_module_message(struct ipc4_message_request *ipc4, struct ipc_msg *reply);

/**
 * @brief Process MOD_CONFIG_GET or MOD_CONFIG_SET in any execution context.
 * @param[in] ipc4 IPC4 message request.
 * @param[in] set true for CONFIG_SET, false for CONFIG_GET.
 * @param[out] reply_ext Receives extension value for CONFIG_GET (may be NULL).
 * @return IPC4 status code (0 on success).
 */
int ipc4_process_module_config(struct ipc4_message_request *ipc4,
			       bool set, uint32_t *reply_ext);

/**
 * \brief Processes IPC4 userspace global message.
 * @param[in] ipc4 IPC4 message request.
 * @param[in] reply IPC message reply structure.
 * @return IPC4_SUCCESS on success, error code otherwise.
 */
int ipc4_user_process_glb_message(struct ipc4_message_request *ipc4, struct ipc_msg *reply);

/**
 * \brief Increment the IPC compound message pre-start counter.
 * @param[in] msg_id IPC message ID.
 */
void ipc_compound_pre_start(int msg_id);

/**
 * \brief Decrement the IPC compound message pre-start counter on return value status.
 * @param[in] msg_id IPC message ID.
 * @param[in] ret Return value of the IPC command.
 * @param[in] delayed True if the reply is delayed.
 */
void ipc_compound_post_start(uint32_t msg_id, int ret, bool delayed);

/**
 * \brief Complete the IPC compound message.
 * @param[in] msg_id IPC message ID.
 * @param[in] error Error code of the IPC command.
 */
void ipc_compound_msg_done(uint32_t msg_id, int error);

/**
 * \brief Wait for the IPC compound message to complete.
 * @return 0 on success, error code otherwise on timeout.
 */
int ipc_wait_for_compound_msg(void);

#endif /* __SOF_IPC4_HANDLER_H__ */
