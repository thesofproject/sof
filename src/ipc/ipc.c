/* 
 * BSD See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Generic message handling.
 */

#include <reef/debug.h>
#include <reef/ipc.h>
#include <platform/platform.h>

/* no heap atm */
static struct ipc context;

struct ipc *ipc_init(int (*cb)(struct ipc_msg *msg))
{
	int err;

	context.rx_pending = 0;
	context.tx_pending = 0;
	context.cb = cb;

	/* initialise vendor IPC */
	err = platform_ipc_init(&context);
	if (err < 0)
		panic(err);

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

