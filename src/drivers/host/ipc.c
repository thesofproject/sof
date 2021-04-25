// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

#include <sof/lib/alloc.h>
#include <sof/ipc/driver.h>
#include <stdlib.h>

/* testbench ipc */
struct ipc *_ipc;

/* private data for IPC */
struct ipc_data {
	struct ipc_data_host_buffer dh_buffer;
};

int ipc_platform_compact_write_msg(ipc_cmd_hdr *hdr, int words)
{
	return 0; /* number of words read - not currently used on this platform */
}

int ipc_platform_compact_read_msg(ipc_cmd_hdr *hdr, int words)
{
	return 0; /* number of words read - not currently used on this platform */
}

enum task_state ipc_platform_do_cmd(void *data)
{
	return SOF_TASK_STATE_COMPLETED;
}

void ipc_platform_complete_cmd(void *data)
{
}

int platform_ipc_init(struct ipc *ipc)
{
	struct ipc_data *iipc;

	_ipc = ipc;

	/* init ipc data */
	iipc = malloc(sizeof(struct ipc_data));
	ipc_set_drvdata(_ipc, iipc);

	/* allocate page table buffer */
	iipc->dh_buffer.page_table = malloc(HOST_PAGE_SIZE);
	if (iipc->dh_buffer.page_table)
		bzero(iipc->dh_buffer.page_table, HOST_PAGE_SIZE);

	return 0;
}
