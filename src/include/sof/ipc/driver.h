/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_IPC_DRIVER_H__
#define __SOF_IPC_DRIVER_H__

#include <sof/platform.h>
#include <sof/sof.h>
#include <sof/ipc/common.h>
#include <sof/schedule/task.h>
#include <stdbool.h>
#include <stdint.h>

struct ipc_msg;

/**
 * \brief Provides platform specific IPC initialization.
 * @param ipc Global IPC context
 * @return 0 if succeeded, error code otherwise.
 *
 * This function must be implemented by the platform. It is called from the
 * main IPC code, at the end of ipc_init().
 *
 * If the platform requires any private data to be associated with the IPC
 * context, it may allocate it here and attach to the global context using
 * ipc_set_drvdata(). Other platform specific IPC functions, like
 * ipc_platform_do_cmd(), may obtain it later from the context using
 * ipc_get_drvdata().
 */
int platform_ipc_init(struct ipc *ipc);

/**
 * \brief Perform IPC command from host.
 * @return The task state of the IPC command work.
 */
enum task_state ipc_platform_do_cmd(void *data);

/**
 * \brief Tell host we have completed the last IPC command.
 */
void ipc_platform_complete_cmd(void *data);

/**
 * \brief Send IPC message to host.
 * @param msg The IPC message to send to host.
 * @return 0 on success.
 */
int ipc_platform_send_msg(struct ipc_msg *msg);

/**
 * \brief Retrieves the ipc_data_host_buffer allocated by the platform ipc.
 * @return Pointer to the data.
 *
 * This function must be implemented by platforms which use
 * ipc...page_descriptors() while processing host page tables.
 */
struct ipc_data_host_buffer *ipc_platform_get_host_buffer(struct ipc *ipc);

/**
 * \brief Read a compact IPC message from hardware.
 * @param[in] hdr IPC header data
 * @param[in] words Number of words to read in header.
 * @return Number of word written.
 */
int ipc_platform_compact_read_msg(ipc_cmd_hdr *hdr, int words);

/**
 * \brief Write a compact IPC message to hardware.
 * @param[in] hdr Compact message header data.
 * @param[in] words Number of words to be written from header.
 * @return Number of word written.
 */
int ipc_platform_compact_write_msg(ipc_cmd_hdr *hdr, int words);

/**
 * \brief Initialise IPC hardware for polling mode.
 * @return 0 if successful error code otherwise.
 */
int ipc_platform_poll_init(void);

/**
 * \brief Tell host DSP has completed command.
 */
void ipc_platform_poll_set_cmd_done(void);

/**
 * \brief Check whether there is a new IPC command from host.
 * @return 1 if new command is pending from host.
 */
int ipc_platform_poll_is_cmd_pending(void);

/**
 * \brief Check whether host is ready for new IPC from DSP.
 * @return 1 if host is ready for a new command from DSP.
 */
int ipc_platform_poll_is_host_ready(void);

/**
 * \brief Transmit new message to host.
 * @return 0 if successful error code otherwise.
 */
int ipc_platform_poll_tx_host_msg(struct ipc_msg *msg);

#endif
