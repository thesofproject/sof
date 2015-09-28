/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_IPC__
#define __INCLUDE_IPC__

#include <stdint.h>

#define MSG_QUEUE_SIZE		8

struct ipc_msg {
	uint32_t type;		/* specific to platform */
	uint32_t size;		/* payload size in bytes */
	uint32_t *data;		/* pointer to payload data */
};

struct ipc {
	struct ipc_msg tx_queue[MSG_QUEUE_SIZE];
	struct ipc_msg rx_msg[MSG_QUEUE_SIZE];

	int tx_pending;
	int rx_pending;

	/* RX call back */
	int (*cb)(struct ipc_msg *msg);
};

struct ipc *ipc_init(int (*cb)(struct ipc_msg *msg));
void ipc_free(struct ipc *ipc);

int ipc_process_msg_queue(struct ipc *ipc);

int ipc_send_msg(struct ipc *ipc, struct ipc_msg *msg);
int ipc_send_short_msg(struct ipc *ipc, uint32_t msg);

#endif
