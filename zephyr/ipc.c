// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

#include <sof/ipc.h>

/* testbench ipc */
extern struct ipc *_ipc;

/* private data for IPC */
struct ipc_data {
	struct ipc_data_host_buffer dh_buffer;
};

int platform_ipc_init(struct ipc *ipc)
{
	struct ipc_data *iipc;

	_ipc = ipc;

	/* init ipc data */
	iipc = k_malloc(sizeof(struct ipc_data));
	ipc_set_drvdata(_ipc, iipc);

	/* allocate page table buffer */
	iipc->dh_buffer.page_table = k_calloc(HOST_PAGE_SIZE, 1);

	/* schedule */
	schedule_task_init(&_ipc->ipc_task, SOF_SCHEDULE_EDF, SOF_TASK_PRI_IPC,
			   ipc_process_task, _ipc, 0, 0);
	return 0;
}

/* The following definitions are to satisfy libsof linker errors */
#if 0
int ipc_stream_send_position(struct comp_dev *cdev,
			     struct sof_ipc_stream_posn *posn)
{
	return 0;
}

int ipc_stream_send_xrun(struct comp_dev *cdev,
			 struct sof_ipc_stream_posn *posn)
{
	return 0;
}
#endif
