/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *	   Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
 */

#include <sof/ipc.h>
#include <sof/intel-ipc.h>

/* testbench ipc */
struct ipc *_ipc;

int platform_ipc_init(struct ipc *ipc)
{
	struct intel_ipc_data *iipc;
	int i;

	_ipc = ipc;

	/* init ipc data */
	iipc = malloc(sizeof(struct intel_ipc_data));
	ipc_set_drvdata(_ipc, iipc);
	_ipc->dsp_msg = NULL;
	list_init(&ipc->empty_list);
	list_init(&ipc->msg_list);
	spinlock_init(&ipc->lock);

	for (i = 0; i < MSG_QUEUE_SIZE; i++)
		list_item_prepend(&ipc->message[i].list, &ipc->empty_list);

	/* allocate page table buffer */
	iipc->page_table = malloc(HOST_PAGE_SIZE);
	if (iipc->page_table)
		bzero(iipc->page_table, HOST_PAGE_SIZE);

	/* PM */
	iipc->pm_prepare_D3 = 0;

	return 0;
}

/* The following definitions are to satisfy libsof linker errors */

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
