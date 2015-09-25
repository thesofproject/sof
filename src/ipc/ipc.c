/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Generic message handling.
 */

#include <reef/ipc.h>

#define MSG_QUEUE_SIZE		8

struct ipc {
	struct ipc_msg tx_queue[MSG_QUEUE_SIZE];
	struct ipc_msg rx_msg[MSG_QUEUE_SIZE];

	int tx_pending;
	int rx_pending;

	/* RX call back */
	int (*cb)(struct ipc_msg *msg);
};

/* no heap atm */
static struct ipc context;

struct ipc *ipc_init(int (*cb)(struct ipc_msg *msg))
{
	context.rx_pending = 0;
	context.tx_pending = 0;
	context.cb = cb;

	return &context;
}

void ipc_free(struct ipc *ipc)
{
	/* nothing todo here */
}

static int msg_add(struct ipc *ipc, struct ipc_msg *msg)
{

}

int ipc_process_msg_queue(struct ipc *ipc)
{

}

int ipc_send_msg(struct ipc *ipc, struct ipc_msg *msg)
{
}

