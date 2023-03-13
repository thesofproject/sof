/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2023 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_IPC4_IPCGTW_H__
#define __SOF_IPC4_IPCGTW_H__

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
};

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
};

int ipcgtw_process_cmd(const struct ipc4_ipcgtw_cmd *cmd,
		       void *reply_payload, uint32_t *reply_payload_size);

#endif /* __SOF_IPC4_IPCGTW_H__ */
