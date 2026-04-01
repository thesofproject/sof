/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_IPC_COMMON_H__
#define __SOF_IPC_COMMON_H__

#include <rtos/bit.h>
#include <rtos/alloc.h>
#include <sof/list.h>
#include <rtos/task.h>
#include <rtos/spinlock.h>
#include <rtos/sof.h>
#include <user/trace.h>
#include <ipc/header.h>
#include <ipc/stream.h>

#include <stdbool.h>
#include <stdint.h>

struct dma_sg_elem_array;
struct ipc_msg;
struct ipc4_message_request;

/* validates internal non tail structures within IPC command structure */
#define IPC_IS_SIZE_INVALID(object)					\
	((object).hdr.size != sizeof(object))

#define IPC_TAIL_IS_SIZE_INVALID(object)					\
	((object).comp.hdr.size + (object).comp.ext_data_length < sizeof(object))

/* ipc trace context, used by multiple units */
extern struct tr_ctx ipc_tr;

/* convenience error trace for mismatched internal structures */
#define IPC_SIZE_ERROR_TRACE(ctx, object)		\
	tr_err(ctx, "ipc: size %d expected %zu",	\
	       (object).hdr.size, sizeof(object))

/* Returns pipeline source component */
#define ipc_get_ppl_src_comp(ipc, ppl_id) \
	ipc_get_ppl_comp(ipc, ppl_id, PPL_DIR_UPSTREAM)

/* Returns pipeline sink component */
#define ipc_get_ppl_sink_comp(ipc, ppl_id) \
	ipc_get_ppl_comp(ipc, ppl_id, PPL_DIR_DOWNSTREAM)

#define IPC_TASK_INLINE		BIT(0)
#define IPC_TASK_IN_THREAD	BIT(1)
#define IPC_TASK_SECONDARY_CORE	BIT(2)
#define IPC_TASK_POWERDOWN      BIT(3)

struct ipc {
	struct k_spinlock lock;	/* locking mechanism */
	void *comp_data;

	/* PM */
	int pm_prepare_D3;	/* do we need to prepare for D3 */

	struct list_item msg_list;	/* queue of messages to be sent */
	bool is_notification_pending;	/* notification is being sent to host */
	uint32_t task_mask;		/* tasks to be completed by this IPC */
	unsigned int core;		/* core, processing the IPC */

	struct list_item comp_list;	/* list of component devices */

	/* processing task */
#if CONFIG_TWB_IPC_TASK
	struct task *ipc_task;
#else
	struct task ipc_task;
#endif

#ifdef CONFIG_SOF_TELEMETRY_IO_PERFORMANCE_MEASUREMENTS
	/* io performance measurement */
	struct io_perf_data_item *io_perf_in_msg_count;
	struct io_perf_data_item *io_perf_out_msg_count;
#endif

#ifdef __ZEPHYR__
	struct k_work_delayable z_delayed_work;
	struct k_work_q ipc_send_wq;
#endif

	void *private;
};

#define ipc_set_drvdata(ipc, data) \
	((ipc)->private = data)
#define ipc_get_drvdata(ipc) \
	((ipc)->private)

extern struct task_ops ipc_task_ops;

/**
 * \brief Get the IPC global context.
 * @return The global IPC context.
 */
static inline struct ipc *ipc_get(void)
{
	return sof_get()->ipc;
}

/**
 * \brief Initialise global IPC context.
 * @param[in,out] sof Global SOF context.
 * @return 0 on success.
 */
int ipc_init(struct sof *sof);

/**
 * \brief Free global IPC context.
 * @param[in] ipc Global IPC context.
 */
void ipc_free(struct ipc *ipc);

/**
 * \brief Data provided by the platform which use ipc...page_descriptors().
 *
 * Note: this should be made private for ipc-host-ptable.c and ipc
 * drivers for platforms that use ptables.
 */
struct ipc_data_host_buffer {
	/* DMA */
#if CONFIG_ZEPHYR_NATIVE_DRIVERS
	struct sof_dma *dmac;
#else
	struct dma *dmac;
#endif
	uint8_t *page_table;
};

/**
 * \brief Processes page tables for the host buffer.
 * @param[in] ipc Ipc
 * @param[in] ring Ring description sent via Ipc
 * @param[in] direction Direction (playback/capture)
 * @param[out] elem_array Array of SG elements
 * @param[out] ring_size Size of the ring
 * @return Status, 0 if successful, error code otherwise.
 */
int ipc_process_host_buffer(struct ipc *ipc,
			    struct sof_ipc_host_buffer *ring,
			    uint32_t direction,
			    struct dma_sg_elem_array *elem_array,
			    uint32_t *ring_size);

/**
 * \brief Send DMA trace host buffer position to host.
 * @return 0 on success.
 */
int ipc_dma_trace_send_position(void);

/**
 * \brief send a IPC buffer status notify message
 */
void ipc_send_buffer_status_notify(void);

struct dai_data;
/**
 * \brief Configure DAI.
 * @return 0 on success.
 */
int ipc_dai_data_config(struct dai_data *dd, struct comp_dev *dev);

/**
 * \brief Processes IPC4 userspace module message.
 * @param[in] ipc4 IPC4 message request.
 * @param[in] reply IPC message reply structure.
 * @return IPC4_SUCCESS on success, error code otherwise.
 */
int ipc4_user_process_module_message(struct ipc4_message_request *ipc4, struct ipc_msg *reply);

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

/**
 * \brief create a IPC boot complete message.
 * @param[in] header header.
 * @param[in] data data.
 */
void ipc_boot_complete_msg(struct ipc_cmd_hdr *header, uint32_t data);

#if defined(CONFIG_PM_DEVICE) && defined(CONFIG_INTEL_ADSP_IPC)
/**
 * @brief Send an IPC response to Host power transition request informing
 * that power transition failed.
 * @note Normally an reply to the Host IPC message is performed in the
 * low level assembly code to make sure DSP completed all operations before
 * power cut-off.
 * However, when power transition fails for some reason, we should send the
 * IPC response informing about the failure.
 * This happens in abnormal circumstances since the response is send not during
 * IPC task but during power transition logic in the Idle thread.
 */
void ipc_send_failed_power_transition_response(void);
#endif /* CONFIG_PM_DEVICE && CONFIG_INTEL_ADSP_IPC */
/**
 * \brief Send a IPC notification that FW has hit
 *        a DSP notification.
 */
void ipc_send_panic_notification(void);

/**
 * \brief Read a compact IPC message or return NULL for normal message.
 * @return Pointer to the compact message data.
 */
struct ipc_cmd_hdr *ipc_compact_read_msg(void);

/**
 * \brief Write a compact IPC message.
 * @param[in] hdr Compact message header data.
 * @return Number of words written.
 */
int ipc_compact_write_msg(struct ipc_cmd_hdr *hdr);

/**
 * \brief Prepare an IPC message for sending.
 * @param[in] msg The ipc msg.
 * @return pointer to raw header or NULL.
 */
struct ipc_cmd_hdr *ipc_prepare_to_send(const struct ipc_msg *msg);

/**
 * \brief Validate mailbox contents for valid IPC header.
 * @return pointer to header if valid or NULL.
 */
struct ipc_cmd_hdr *mailbox_validate(void);

/**
 * Generic IPC command handler. Expects that IPC command (the header plus
 * any optional payload) is deserialized from the IPC HW by the platform
 * specific method.
 *
 * @param _hdr Points to the IPC command header.
 */
void ipc_cmd(struct ipc_cmd_hdr *_hdr);

/**
 * \brief IPC message to be processed on other core.
 * @param[in] core Core id for IPC to be processed on.
 * @param[in] blocking Process in blocking mode: wait for completion.
 * @return 1 if successful (reply sent by other core), error code otherwise.
 */
int ipc_process_on_core(uint32_t core, bool blocking);

/**
 * \brief reply to an IPC message.
 * @param[in] reply pointer to the reply structure.
 */
void ipc_msg_reply(struct sof_ipc_reply *reply);

/**
 * \brief Call platform-specific IPC completion function.
 */
void ipc_complete_cmd(struct ipc *ipc);

/* GDB stub: should enter GDB after completing the IPC processing */
extern bool ipc_enter_gdb;

#endif /* __SOF_DRIVERS_IPC_H__ */
