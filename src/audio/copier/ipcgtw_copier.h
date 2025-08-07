/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Baofeng Tian <baofeng.tian@intel.com>
 */

/**
 * \file audio/ipcgtw_copier.h
 * \brief ipcgtw copier shared header file
 * \authors Baofeng Tian <baofeng.tian@intel.com>
 */

#ifndef __SOF_IPCGTW_COPIER_H__
#define __SOF_IPCGTW_COPIER_H__

#include <sof/audio/component_ext.h>
#include <ipc4/gateway.h>
#include <sof/list.h>
#include <ipc/stream.h>
#include "copier.h"

/* Host communicates with IPC gateways via global IPC messages. To address a particular
 * IPC gateway, its node_id is sent in message payload. Hence we need to keep a list of existing
 * IPC gateways and their node_ids to search for a gateway host wants to address.
 */
struct ipcgtw_data {
	union ipc4_connector_node_id node_id;
	struct comp_dev *dev;
	struct list_item item;

	/* IPC gateway buffer size comes in blob at creation time, we keep size here
	 * to resize buffer later at ipcgtw_params().
	 */
	uint32_t buf_size;
};

/**< IPC header format for IPC gateway messages */
struct ipc4_ipcgtw_cmd {
	union  {
		uint32_t dat;

		struct {
			/**< Command, see below */
			uint32_t cmd : 24;

			/**< One of Global::Type */
			uint32_t type : 5;

			/**< Msg::MSG_REQUEST */
			uint32_t rsp : 1;

			/**< Msg::FW_GEN_MSG */
			uint32_t msg_tgt : 1;

			uint32_t _reserved_0 : 1;
		} r;
	} primary;
	union  {
		uint32_t dat;
		struct {
			uint32_t data_size : 30;
			uint32_t _reserved_0 : 2;
		} r;
	} extension;
} __packed __aligned(4);

/**< Values of ipc4_ipcgtw_cmd::primary.r.cmd */
enum {
	IPC4_IPCGWCMD_GET_DATA = 1,
	IPC4_IPCGWCMD_SET_DATA = 2,
	IPC4_IPCGWCMD_FLUSH_DATA = 3
};

/* Incoming IPC gateway message */
struct ipc4_ipc_gateway_cmd_data {
	/* node_id of the target gateway */
	union ipc4_connector_node_id node_id;
	/* Payload (actual size is in the header extension.r.data_size) */
	uint8_t payload[];
} __packed __aligned(4);

/* Reply to IPC gateway message */
struct ipc4_ipc_gateway_cmd_data_reply {
	union {
		uint32_t size_avail;    /* Reply for IPC4_IPCGWCMD_GET_DATA */
		uint32_t size_consumed; /* Reply for IPC4_IPCGWCMD_SET_DATA */
	} u;
	/* Total reply size is returned in reply header extension.r.data_size.
	 * This payload size if 4 bytes smaller (size of the union above).
	 */
	uint8_t payload[];
} __packed __aligned(4);

int copier_ipcgtw_process(const struct ipc4_ipcgtw_cmd *cmd,
			  void *reply_payload, uint32_t *reply_payload_size);

int copier_ipcgtw_create(struct processing_module *mod, struct copier_data *cd,
			 const struct ipc4_copier_module_cfg *copier, struct pipeline *pipeline);

#if CONFIG_IPC4_GATEWAY
void copier_ipcgtw_free(struct copier_data *cd);
int copier_ipcgtw_params(struct ipcgtw_data *ipcgtw_data, struct comp_dev *dev,
			 struct sof_ipc_stream_params *params);

void copier_ipcgtw_reset(struct comp_dev *dev);
#else
static inline void copier_ipcgtw_free(struct copier_data *cd) {}
static inline void copier_ipcgtw_reset(struct comp_dev *dev) {}
static inline int copier_ipcgtw_params(struct ipcgtw_data *ipcgtw_data, struct comp_dev *dev,
				       struct sof_ipc_stream_params *params)
{
	return 0;
}
#endif
#endif /* __SOF_IPCGTW_COPIER_H__ */
