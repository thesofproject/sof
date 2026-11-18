/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 */

/**
 * \file include/ipc4/handler.h
 * \brief IPC4 message handlers, userspace bridge, and compound message synchronization.
 * \ingroup ipc4_envelope_api
 */

#ifndef __SOF_IPC4_HANDLER_H__
#define __SOF_IPC4_HANDLER_H__

struct comp_dev;
struct comp_driver;
struct ipc4_message_request;
struct ipc4_pipeline_set_state;

/**
 * \brief Processes an IPC4 userspace module message.
 * \ingroup ipc4_envelope_api
 * @param[in] ipc4 IPC4 message request structure.
 * @param[in,out] reply IPC message reply structure populated upon return.
 * @return IPC4_SUCCESS (0) on success, or negative/IPC4 status code on failure.
 */
int ipc4_user_process_module_message(struct ipc4_message_request *ipc4, struct ipc_msg *reply);

/**
 * \brief Load a dynamically loadable module instance into userspace memory.
 * \ingroup ipc4_envelope_api
 * @param[in] drv Component driver operations structure.
 * @param[in] mi IPC4 module initialization parameters (SOF_IPC4_MOD_INIT_INSTANCE).
 * @return 0 on success, or negative error code on allocation/loading failure.
 */
int ipc4_user_module_load(const struct comp_driver *drv,
			  const struct ipc4_module_init_instance *mi);

/**
 * \brief Process MOD_CONFIG_GET or MOD_CONFIG_SET in any execution context.
 * \ingroup ipc4_envelope_api
 * @param[in] ipc4 IPC4 message request structure.
 * @param[in] set True for MOD_CONFIG_SET, false for MOD_CONFIG_GET.
 * @param[out] reply_ext Receives extension DWORD value for CONFIG_GET (may be NULL).
 * @return IPC4 status code (IPC4_SUCCESS on success).
 */
int ipc4_process_module_config(struct ipc4_message_request *ipc4,
			       bool set, uint32_t *reply_ext);

/**
 * \brief Process MOD_LARGE_CONFIG_SET in any execution context.
 * \ingroup ipc4_envelope_api
 * @param[in] ipc4 IPC4 message request structure containing block parameters and offset.
 * @return IPC4 status code (IPC4_SUCCESS on success).
 */
int ipc4_process_large_config_set(struct ipc4_message_request *ipc4);

/**
 * \brief Processes an IPC4 userspace global firmware message.
 * \ingroup ipc4_envelope_api
 * @param[in] ipc4 IPC4 message request structure.
 * @param[in,out] reply IPC message reply structure.
 * @return IPC4_SUCCESS on success, error code otherwise.
 */
int ipc4_user_process_glb_message(struct ipc4_message_request *ipc4, struct ipc_msg *reply);

/**
 * \brief Process SET_PIPELINE_STATE IPC4 message (prepare + trigger phases).
 * \ingroup ipc4_envelope_api
 * @param[in] ipc4 IPC4 message request structure containing target state and pipeline ID.
 * @return 0 on success, or IPC4 error code on failure.
 */
int ipc4_set_pipeline_state(struct ipc4_message_request *ipc4);

/**
 * \brief Get the first pipeline ID of a SET_PIPELINE_STATE message.
 * \ingroup ipc4_envelope_api
 * @param[in] ipc4 IPC4 message request structure.
 * @param[in] state Copy of the SET_PIPELINE_STATE message header.
 * @param[out] ppl_id Receives the pipeline ID array of a multi-pipeline
 *		      message, NULL otherwise (may be NULL).
 * @param[out] ppl_count Receives the number of pipelines (may be NULL).
 * @return The first pipeline ID, or a negative error code.
 */
int ipc4_pipeline_id_get(struct ipc4_message_request *ipc4,
			 struct ipc4_pipeline_set_state *state,
			 const uint32_t **ppl_id, unsigned int *ppl_count);

/**
 * \brief Copy the kcontrol payload header of a LARGE_CONFIG_GET from the hostbox.
 * \ingroup ipc4_envelope_api
 * @param[in] dev Target component, NULL for the base firmware.
 * @param[in] param_id Large config parameter ID.
 * @param[out] data_out Reply data buffer.
 * @param[in] data_size Size of the payload in the hostbox.
 */
void ipc4_prepare_for_kcontrol_get(struct comp_dev *dev, uint8_t param_id,
				   char *data_out, uint32_t data_size);

/**
 * \brief Process a vendor config LARGE_CONFIG_GET.
 * \ingroup ipc4_envelope_api
 * @param[in] dev Target component, NULL for the base firmware.
 * @param[in] drv Component driver.
 * @param[in] init_block True for the first block.
 * @param[in] final_block True for the last block.
 * @param[in,out] data_off_size Payload size in, reply data size out.
 * @param[out] data_out Reply data buffer.
 * @param[in] data_in Request payload.
 * @return IPC4 status code (IPC4_SUCCESS on success).
 */
int ipc4_get_vendor_config_module_instance(struct comp_dev *dev,
					   const struct comp_driver *drv,
					   bool init_block,
					   bool final_block,
					   uint32_t *data_off_size,
					   char *data_out,
					   const char *data_in);

/**
 * \brief Process a vendor config LARGE_CONFIG_SET.
 * \ingroup ipc4_envelope_api
 * @param[in] dev Target component, NULL for the base firmware.
 * @param[in] drv Component driver.
 * @param[in] module_id Module ID, for error messages.
 * @param[in] instance_id Instance ID, for error messages.
 * @param[in] init_block True for the first block.
 * @param[in] final_block True for the last block.
 * @param[in] data_off_size Payload size.
 * @param[in] data Request payload.
 * @return IPC4 status code (IPC4_SUCCESS on success).
 */
int ipc4_set_vendor_config_module_instance(struct comp_dev *dev,
					   const struct comp_driver *drv,
					   uint32_t module_id,
					   uint32_t instance_id,
					   bool init_block,
					   bool final_block,
					   uint32_t data_off_size,
					   const char *data);

/**
 * \brief Complete an IPC compound message transaction and signal waiting threads.
 * \ingroup ipc4_envelope_api
 * @param[in] msg_id Compound IPC message transaction identifier.
 * @param[in] error Overall error code of the compound transaction.
 */
void ipc_compound_msg_done(uint32_t msg_id, int error);

/**
 * \brief Connect components together on a pipeline graph.
 * \ingroup ipc4_envelope_api
 * @param[in] ipc The global IPC context pointer.
 * @param[in] bu IPC4 bind-unbind configuration data.
 * @return IPC4_SUCCESS on success, or IPC4 error code otherwise.
 */
int ipc4_comp_connect(struct ipc *ipc, const struct ipc4_module_bind_unbind *bu);

/**
 * \brief Disconnect components in a pipeline graph.
 * \ingroup ipc4_envelope_api
 * @param[in] ipc The global IPC context pointer.
 * @param[in] bu IPC4 bind-unbind configuration data.
 * @return IPC4_SUCCESS on success, or IPC4 error code otherwise.
 */
int ipc4_comp_disconnect(struct ipc *ipc, const struct ipc4_module_bind_unbind *bu);

#if defined(__ZEPHYR__) && defined(CONFIG_SOF_FULL_ZEPHYR_APPLICATION)
/**
 * \brief Increment the IPC compound message pre-start barrier counter.
 * \ingroup ipc4_envelope_api
 * @param[in] msg_id IPC message ID.
 */
__syscall void ipc_compound_pre_start(int msg_id);

/**
 * \brief Decrement the IPC compound message pre-start barrier counter on return.
 * \ingroup ipc4_envelope_api
 * @param[in] msg_id IPC message ID.
 * @param[in] ret Return value of the IPC command.
 * @param[in] delayed True if the reply is deferred/asynchronous.
 */
__syscall void ipc_compound_post_start(uint32_t msg_id, int ret, bool delayed);

/**
 * \brief Wait for active IPC compound message operations to complete.
 * \ingroup ipc4_envelope_api
 * @return 0 on success, or negative error code on timeout.
 */
__syscall int ipc_wait_for_compound_msg(void);

#include <zephyr/syscalls/handler.h>
#else
/**
 * @brief Compound IPC message pre-start handler (internal implementation).
 * @param msg_id IPC message identifier.
 */
void z_impl_ipc_compound_pre_start(int msg_id);
#define ipc_compound_pre_start z_impl_ipc_compound_pre_start

/**
 * @brief Compound IPC message post-start handler (internal implementation).
 * @param msg_id IPC message identifier.
 * @param ret Return status of the IPC processing.
 * @param delayed True if response is deferred/asynchronous.
 */
void z_impl_ipc_compound_post_start(uint32_t msg_id, int ret, bool delayed);
#define ipc_compound_post_start z_impl_ipc_compound_post_start

/**
 * @brief Wait for active compound IPC message completion (internal implementation).
 * @return 0 on success, negative error code on timeout.
 */
int z_impl_ipc_wait_for_compound_msg(void);
#define ipc_wait_for_compound_msg z_impl_ipc_wait_for_compound_msg
#endif

#endif /* __SOF_IPC4_HANDLER_H__ */
